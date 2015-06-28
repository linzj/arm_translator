import sys
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

class FunctionTable(object):
    def __init__(self):
        """
        this table's key is the function's linkage name/name
        value is the a FunctionDesc
        """
        self.m_table = {}
    def add(self, name, functionDesc):
        if name in self.m_table:
            #raise Exception("name appears twice.")
            return
        self.m_table[name] = functionDesc

    def get(self, name):
        return self.m_table[name];

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

    def handleDIEType(self, die):
        if die.tag == 'DW_TAG_subprogram':
            self.m_functions.append(die)
            return

        if die.tag and isinstance(die.tag, str):
            setattr(die, "m_baseOffset", self.m_baseOffset)
            self.m_DIETable.add(die.offset, die)

    def getFuncName(self, fdie):
        r = fdie.attributes.get('DW_AT_linkage_name')
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
                ty = self.m_DIETable.get(offset + fdie.m_baseOffset)
            except Exception as e:
                raise ContinueException(e)
            bs = ty.attributes.get('DW_AT_byte_size')
            if not bs:
                #must be const modifer or something like that
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

    def getParamSize(self, fdie):
        ret = 0
        try:
            for param in fdie.iter_children():
                if param.tag != 'DW_TAG_formal_parameter':
                    if param.tag == 'DW_TAG_template_type_param':
                        continue
                    if param.tag == 16647 or param.tag == 16648:
                        continue
                    if param.tag == 'DW_TAG_template_value_param':
                        continue
                    if param.tag == 'DW_TAG_lexical_block':
                        continue
                    if param.tag == 'DW_TAG_unspecified_parameters':
                        raise ContinueException()
                    raise Exception("Unknown function child: " + param.tag)
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
                ret += value
            return ret
        except Exception as e:
            raise e

    def composeFunction(self, fdie):
        name = self.getFuncName(fdie)
        self.m_debugName = name
        returnType = self.getReturnType(fdie)
        paramtersSize = self.getParamSize(fdie)
        if returnType == RETURN_GREAT:
            paramtersSize += self.m_elfClass / 8
        funcDesc = FunctionDesc()
        funcDesc.m_returnType = returnType
        funcDesc.m_paramtersSize = paramtersSize
        lowPCAttr = fdie.attributes.get('DW_AT_low_pc')
        if lowPCAttr:
            funcDesc.m_lowPC = lowPCAttr.value
        self.m_functionTable.add(name, funcDesc)

    def compose(self):
       for f in self.m_functions:
           try:
               self.composeFunction(f)
           except Exception:
            pass

    def handle(self, elf):
        self.m_elfClass = elf.elfclass
        dw = elf.get_dwarf_info()
        for cu in dw.iter_CUs():
            self.m_baseOffset = cu.cu_offset
            for die in cu.iter_DIEs():
                self.handleDIEType(die)
        self.compose()
        self.m_functions = None
        self.m_DIETable = None

    def __repr__(self):
        io = StringIO()
        for funcName, funcDesc in self.m_functionTable.items():
            print >>io, "name: %s, return type:%s, param size: %d, lowpc: %x" % (funcName, returnNames[funcDesc.m_returnType], funcDesc.m_paramtersSize, funcDesc.m_lowPC)
        return io.getvalue()

def main(argv):
    with open(argv[1], 'r') as f:
        db = ElfDb()
        elf = elffile.ELFFile(f)
        if not elf.has_dwarf_info():
            print "%s has no dwarf info." % argv[1]
            return
        db.handle(elf)
    print "%s" % (str(db))

if __name__ == '__main__':
    main(sys.argv)
