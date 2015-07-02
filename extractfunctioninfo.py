import sys, traceback
import types
from cStringIO import StringIO
from elftools.elf import elffile

RETURN_VOID = 0
RETURN_4BYTES = 1
RETURN_8BYTES = 2
RETURN_FLOAT = 3
RETURN_DOUBLE = 4
RETURN_GREAT = 5

returnNames = (
'RETURN_VOID',
'RETURN_4BYTES',
'RETURN_8BYTES',
'RETURN_FLOAT',
'RETURN_DOUBLE',
'RETURN_GREAT',
)

class ContinueException(Exception):
    pass

class FunctionDesc(object):
    def __init__(self):
        self.m_returnType = RETURN_VOID
        self.m_paramtersSize = 0
        self.m_lowPC = 0
        self.m_markedSubroutine = None

    def __repr__(self):
        if isinstance(self.m_paramtersSize, (types.IntType, types.LongType)):
            paramterSize = "%d" % self.m_paramtersSize
        else:
            paramterSize = "unspecified"
        if self.m_markedSubroutine:
            markedSubroutine = ", marked subroutines: (" + ", ".join([ "{0}".format(a) for a in self.m_markedSubroutine ]) + ")"
        else:
            markedSubroutine = ""
        return "return type:%s, param size: %s, lowpc: %x%s" % (returnNames[self.m_returnType], paramterSize, self.m_lowPC, markedSubroutine)

class FunctionTable(object):
    def __init__(self):
        """
        this table's key is the function's linkage name/name
        value is the a FunctionDesc
        """
        self.m_table = {}
    def add(self, name, functionDesc):
        if name in self.m_table:
            raise Exception("name appears twice.")
            return
        self.m_table[name] = functionDesc

    def get(self, name):
        return self.m_table[name];

    def getNothrow(self, name):
        return self.m_table.get(name)

    def items(self):
        return self.m_table.items()

class DIETable(object):
    def __init__(self):
        """
        this table's key is offset, value is DIE
        """
        self.m_table = {}

    def add(self, offset, die):
        self.m_table[offset] = die

    def get(self, offset):
        return self.m_table[offset]

class ElfDb(object):
    def __init__(self):
        self.m_functions = []
        self.m_DIETable = DIETable()
        self.m_functionTable = FunctionTable()
        self.m_elfclass = None
        self.m_debugName = None
        self.m_baseOffset = 0
        self.m_returnPointer = None
        self.m_unspecifiedParamter = None
        self.m_markedSubroutine = None

    def handleDIEType(self, die):
        if die.tag == 'DW_TAG_subprogram':
            self.m_functions.append(die)
            setattr(die, "m_baseOffset", self.m_baseOffset)
            return

        if die.tag and isinstance(die.tag, str):
            setattr(die, "m_baseOffset", self.m_baseOffset)
            self.m_DIETable.add(die.offset, die)

    def getFuncName(self, fdie):
        spec = fdie.attributes.get('DW_AT_specification')
        if spec:
            fdie = self.m_DIETable.get(fdie.m_baseOffset + spec.value)
        r = fdie.attributes.get('DW_AT_linkage_name')
        if r:
            return r.value
        r = fdie.attributes.get('DW_AT_MIPS_linkage_name')
        if r:
            return r.value
        r = fdie.attributes.get('DW_AT_name')
        if not r:
            raise ContinueException("unknown func name")
        return r.value

    def getReturnType(self, fdie):
        te = fdie.attributes.get('DW_AT_type')
        if not te:
            return RETURN_VOID
        while True:
            offset = te.value + fdie.m_baseOffset
            try:
                ty = self.m_DIETable.get(offset)
            except Exception as e:
                raise ContinueException(e)
            bs = ty.attributes.get('DW_AT_byte_size')
            if not bs:
                #must be const modifer or something like that
                if ty.tag == 'DW_TAG_pointer_type':
                    return self.m_returnPointer
                try:
                    te = ty.attributes['DW_AT_type']
                except Exception as e:
                    raise ContinueException()
                continue
            bsValue = bs.value
            if bsValue > 8:
                #a hidden parmeter will be pass
                return RETURN_GREAT
            if bsValue < 4:
                return RETURN_4BYTES
            nameAttr = ty.attributes.get('DW_AT_name')
            if nameAttr:
                name = nameAttr.value
                if name == 'float':
                    return RETURN_FLOAT
                if name == 'double':
                    return RETURN_DOUBLE
            if bsValue == 4:
                return RETURN_4BYTES
            if bsValue == 8:
                return RETURN_8BYTES
            raise Exception("Unknown return type.")

    # for arm eabi compatibility
    def getParamSize(self, sizes):
        ret = 0
        for size in sizes:
            if size == 8:
                if ret & 7:
                    ret += 12
                else:
                    ret += 8
            else:
                ret += 4
        return ret

    def getParamSizes(self, fdie):
        ret = []
        try:
            index = 0
            for param in fdie.iter_children():
                if param.tag != 'DW_TAG_formal_parameter':
                    if param.tag == 'DW_TAG_unspecified_parameters':
                        self.m_unspecifiedParamter = True
                        return
                    continue
                while True:
                    ao = param.attributes.get('DW_AT_abstract_origin')
                    if not ao:
                        break
                    param = self.m_DIETable.get(ao.value + param.m_baseOffset)
                te = param.attributes['DW_AT_type']
                while True:
                    ty = self.m_DIETable.get(te.value + param.m_baseOffset)
                    bs = ty.attributes.get('DW_AT_byte_size')
                    if bs == None:
                        te = ty.attributes.get('DW_AT_type')
                        if not te:
                            value = 0
                            break
                        continue
                    value = bs.value
                    break

                ret.append(value)
                self.markSubroutineParam(param, index)
                index += 1
            return ret
        except Exception as e:
            traceback.print_exc(e, file = sys.stdout)
            raise e

    def unwrapPointerTypedef(self, die):
        if die.tag == 'DW_TAG_pointer_type' or die.tag == 'DW_TAG_typedef':
            te = die.attributes.get('DW_AT_type')
            if not te:
                return None
            return self.m_DIETable.get(te.value + die.m_baseOffset)
        return None

    def markSubroutineParam(self, param, index):
        te = param.attributes.get('DW_AT_type')
        ty = self.m_DIETable.get(te.value + param.m_baseOffset)
        while True:
            ty2 = self.unwrapPointerTypedef(ty)
            if not ty2:
                break
            ty = ty2
        if ty.tag == 'DW_TAG_subroutine_type':
            self.m_markedSubroutine.append(index)

    def clearMarkedSubroutineParam(self):
        self.m_markedSubroutine = []

    def composeFunction(self, fdie):
        name = self.getFuncName(fdie)
        self.m_debugName = name
        returnType = self.getReturnType(fdie)
        self.m_unspecifiedParamter = False
        self.clearMarkedSubroutineParam()
        paramtersSizes = self.getParamSizes(fdie)
        if not self.m_unspecifiedParamter and returnType == RETURN_GREAT:
            paramtersSizes.insert(0, self.m_elfClass / 8)
        paramtersSize = self.getParamSize(paramtersSizes)
        funcDesc = FunctionDesc()
        funcDesc.m_returnType = returnType
        if self.m_unspecifiedParamter:
            funcDesc.m_paramtersSize = None
        else:
            funcDesc.m_paramtersSize = paramtersSize
        lowPCAttr = fdie.attributes.get('DW_AT_low_pc')
        if lowPCAttr:
            funcDesc.m_lowPC = lowPCAttr.value
        funcDesc.m_markedSubroutine = self.m_markedSubroutine
        self.m_functionTable.add(name, funcDesc)

    def compose(self):
       for f in self.m_functions:
           try:
               self.composeFunction(f)
           except Exception:
            pass
    def updateLowPC(self, elf):
        """
        update m_lowPC for each function in .symtab section
        """
        sect = elf.get_section_by_name(".symtab")
        if not sect:
            raise Exception("unable to find .symtab section")
        for sym in sect.iter_symbols():
            funcDesc = self.m_functionTable.getNothrow(sym.name)
            if not funcDesc:
                continue
            if funcDesc.m_lowPC == 0:
                funcDesc.m_lowPC = sym.entry.st_value

    def handle(self, elf):
        self.m_elfClass = elf.elfclass
        if self.m_elfClass == 64:
            self.m_returnPointer = RETURN_8BYTES
        else:
            self.m_returnPointer = RETURN_4BYTES
        dw = elf.get_dwarf_info()
        for cu in dw.iter_CUs():
            self.m_baseOffset = cu.cu_offset
            for die in cu.iter_DIEs():
                self.handleDIEType(die)
        self.compose()
        self.m_functions = None
        self.m_DIETable = None
        self.updateLowPC(elf)
    def funcTable(self):
        return self.m_functionTable

    def __repr__(self):
        io = StringIO()
        for funcName, funcDesc in self.m_functionTable.items():
            print >>io, "name: %s, %s" % (funcName, str(funcDesc))
        return io.getvalue()

def main(argv):
    for i in range(1, len(argv)):
        with open(argv[i], 'r') as f:
            db = ElfDb()
            elf = elffile.ELFFile(f)
            if not elf.has_dwarf_info():
                print "%s has no dwarf info." % argv[1]
                return
            db.handle(elf)
        print "%s" % (str(db))

if __name__ == '__main__':
    main(sys.argv)
