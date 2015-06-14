#ifndef REGISTERS_H
#define REGISTERS_H
namespace jit {
enum AMD64 {
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RSP = 4,
    RBP = 5,
    R11 = 11,
    RSI = 6,
    RDI = 7,
    R8 = 8,
    R9 = 9,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15,
    RBX = 3,
};

class Reg {
public:
    Reg(void)
        : m_val(invalid())
    {
    }
    Reg(int val_)
        : m_val(val_)
    {
    }
    Reg(AMD64 val_)
        : m_val(val_)
    {
    }
    int val() const { return m_val; }
    bool isFloat() const { return m_isFloat; }
    static inline int invalid() { return -1; }

private:
    int m_val;

protected:
    bool m_isFloat = false;
};

class FPRReg : public Reg {
public:
    FPRReg(int val_)
        : Reg(val_)
    {
        m_isFloat = true;
    }
};

class DWARFRegister {
public:
    DWARFRegister()
        : m_dwarfRegNum(-1)
    {
    }

    explicit DWARFRegister(int16_t dwarfRegNum_)
        : m_dwarfRegNum(dwarfRegNum_)
    {
    }

    int16_t dwarfRegNum() const { return m_dwarfRegNum; }
    Reg reg() const;

private:
    int16_t m_dwarfRegNum;
};
}
#endif /* REGISTERS_H */
