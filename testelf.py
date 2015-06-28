from elftools.elf import elffile

def handleDIE(die):
    print('%s' % (repr(die)))
    #for c in die.iter_children():
    #    handleDIE(c)

def handle(elf):
    dw = elf.get_dwarf_info()
    for cu in dw.iter_CUs():
        for die in cu.iter_DIEs():
            if die.tag == 'DW_TAG_subprogram':
                handleDIE(die)

def main():
    with open('/tmp/1', 'r') as f:
        handle(elffile.ELFFile(f))

if __name__ == '__main__':
    main()
