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
            raise Exception("name appears twice.")
        self.m_table[name] = functionDesc

    def get(self, name):
        return self.m_table[name];

    def items(self):
        return self.m_table.items()

class TypeTable(object):
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
        self.m_typeTable = TypeTable()
        self.m_functionTable = FunctionTable()
        self.m_elfclass = None

    def handleDIEType(self, die):
        if die.tag == 'DW_TAG_subprogram':
            self.m_functions.append(die)
            return

        if die.tag and isinstance(die.tag, str) and 'type' in die.tag:
            self.m_typeTable.add(die.offset, die)

    def getFuncName(self, fdie):
        r = fdie.attributes['DW_AT_linkage_name']
        if r:
            return r.value
        r = fdie.attributes['DW_AT_name']
        return r.value

    def getReturnType(self, fdie):
        te = fdie.attributes['DW_AT_type']
        if not te:
            return RETURN_VOID
        while True:
            offset = te.value
            ty = self.m_typeTable.get(offset)
            bs = ty.attributes['DW_AT_byte_size']
            if not bs:
                #must be const modifer or something like that
                te = ty['DW_AT_type']
                continue
            bsValue = bs.value
            if bsValue > 8:
                #a hidden parmeter will be pass
                return RETURN_GREAT
            if bsValue < 4:
                return RETURN_4BYTES
            name = ty.attributes['DW_AT_name'].value
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
        for param in fdie.iter_children():
            if param.tag != 'DW_TAG_formal_parameter':
                raise Exception("Unknown function child.")
            te = param.attributes['DW_AT_type']
            while True:
                ty = self.m_typeTable.get(te.value)
                bs = ty.attributes.get('DW_AT_byte_size')
                if bs == None:
                    te = ty.attributes.get('DW_AT_type')
                    continue
                break
            ret += bs.value
        return ret

    def composeFunction(self, fdie):
        name = self.getFuncName(fdie)
        returnType = self.getReturnType(fdie)
        paramtersSize = self.getParamSize(fdie)
        if returnType == RETURN_GREAT:
            paramtersSize += self.m_elfClass / 8
        funcDesc = FunctionDesc()
        funcDesc.m_returnType = returnType
        funcDesc.m_paramtersSize = paramtersSize
        funcDesc.m_lowPC = fdie.attributes['DW_AT_low_pc'].value
        self.m_functionTable.add(name, funcDesc)

    def compose(self):
       for f in self.m_functions:
           self.composeFunction(f)

    def handle(self, elf):
        self.m_elfClass = elf.elfclass
        dw = elf.get_dwarf_info()
        for cu in dw.iter_CUs():
            for die in cu.iter_DIEs():
                self.handleDIEType(die)
        self.compose()
        self.m_functions = None
        self.m_typeTable = None

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
