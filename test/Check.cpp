#include <string.h>
#include <sstream>
#include <iomanip>
#include "Vec.h"
#include "Check.h"
#include "RegisterOperation.h"
Check::Check() {}
Check::~Check() {}
namespace {
template <typename TyDst, typename TySrc>
TyDst bitCast(TySrc src)
{
    TyDst dst;
    memcpy(&dst, &src, sizeof(dst));
    return dst;
}

class CheckRegisterEqConst : public Check {
public:
    CheckRegisterEqConst(const char* name, unsigned long long val)
        : m_regName(name)
        , m_val(val)
    {
    }

private:
    virtual bool check(const CPUARMState* state, const uintptr_t*, std::string& info) const override
    {
        RegisterOperation& op = RegisterOperation::getDefault();
        const intptr_t* p = reinterpret_cast<const intptr_t*>(op.getRegisterPointer(state, m_regName));
        std::ostringstream oss;
        oss << "CheckRegisterEqConst " << ((*p == static_cast<long>(m_val)) ? "PASSED" : "FAILED")
            << "; m_regName = " << m_regName
            << "; m_val = " << std::hex
            << m_val;
        info = oss.str();
        return *p == static_cast<long>(m_val);
    }
    std::string m_regName;
    unsigned long long m_val;
};

class CheckRegisterEqFloatConst : public Check {
public:
    CheckRegisterEqFloatConst(const char* name, double val)
        : m_regName(name)
        , m_val(val)
    {
    }

private:
    virtual bool check(const CPUARMState* state, const uintptr_t*, std::string& info) const override
    {
        RegisterOperation& op = RegisterOperation::getDefault();
        const intptr_t* p = reinterpret_cast<const intptr_t*>(op.getRegisterPointer(state, m_regName));
        float floatVal = bitCast<float>(*p);
        std::ostringstream oss;
        oss << "CheckRegisterEqFloatConst " << ((floatVal == m_val) ? "PASSED" : "FAILED")
            << "; m_regName = " << m_regName
            << "; m_val = " << std::hex
            << m_val;
        info = oss.str();
        return floatVal == m_val;
    }
    std::string m_regName;
    float m_val;
};

class CheckRegisterEq : public Check {
public:
    CheckRegisterEq(const char* name1, const char* name2)
        : m_regName1(name1)
        , m_regName2(name2)
    {
    }

private:
    virtual bool check(const CPUARMState* state, const uintptr_t*, std::string& info) const override
    {
        RegisterOperation& op = RegisterOperation::getDefault();
        const uintptr_t* p1 = op.getRegisterPointer(state, m_regName1);
        const uintptr_t* p2 = op.getRegisterPointer(state, m_regName2);
        std::ostringstream oss;
        oss << "CheckRegisterEq " << ((*p1 == *p2) ? "PASSED" : "FAILED")
            << "; m_regName1 = " << m_regName1
            << "; m_regName2 = " << m_regName2;
        info = oss.str();
        return *p1 == *p2;
    }
    std::string m_regName1;
    std::string m_regName2;
};

class CheckState : public Check {
public:
    CheckState(unsigned long val)
        : m_val(val)
    {
    }

private:
    virtual bool check(const CPUARMState*, const uintptr_t* w, std::string& info) const override
    {
        std::ostringstream oss;
        oss << "CheckState " << ((w[0] == m_val) ? "PASSED" : "FAILED")
            << "; m_val = " << std::hex << m_val
            << "; w[0] = " << std::hex << w[0];
        info = oss.str();
        return (w[0] == m_val);
    }
    unsigned long long m_val;
};

class CheckMemory : public Check {
public:
    CheckMemory(const char* regName, unsigned long val)
        : m_regName(regName)
        , m_val(val)
    {
    }

private:
    virtual bool check(const CPUARMState* state, const uintptr_t*, std::string& info) const override
    {
        std::ostringstream oss;
        RegisterOperation& op = RegisterOperation::getDefault();
        const uintptr_t* p = op.getRegisterPointer(state, m_regName);
        uintptr_t* const* pp = reinterpret_cast<uintptr_t* const*>(p);
        oss << "CheckMemory " << (((**pp) == m_val) ? "PASSED" : "FAILED")
            << "; m_val = " << std::hex << m_val
            << "; memory = " << std::hex << **pp;
        info = oss.str();
        return ((**pp) == m_val);
    }
    std::string m_regName;
    unsigned long long m_val;
};

class CheckVecRegisterEqConst : public Check {
public:
    CheckVecRegisterEqConst(const char* regName, void* val)
        : m_regName(regName)
        , m_val(static_cast<NumberVector*>(val))
    {
    }

private:
    virtual bool check(const CPUARMState* state, const uintptr_t*, std::string& info) const override
    {
        std::ostringstream oss;
        bool check = checkPrivate(*state);
        oss << "CheckVecRegisterEqConst " << (check ? "PASSED" : "FAILED");
        oss << "; m_val = " << *m_val;
        info = oss.str();
        return check;
    }

    bool checkPrivate(const CPUARMState& env) const
    {
        RegisterOperation& op = RegisterOperation::getDefault();
        const uintptr_t* pointer = op.getRegisterPointer(&env, m_regName);
        switch (m_val->m_type) {
        case 64: {
            EMASSERT(m_val->m_intVec->size() <= 2);
            const uint64_t* p = reinterpret_cast<const uint64_t*>(pointer);
            for (auto v : *m_val->m_intVec) {
                if (*p++ != v) {
                    return false;
                }
            }
        } break;
        case 32: {
            EMASSERT(m_val->m_intVec->size() <= 4);
            const uint32_t* p = reinterpret_cast<const uint32_t*>(pointer);
            for (auto v : *m_val->m_intVec) {
                if (*p++ != v) {
                    return false;
                }
            }
        } break;
        case 16: {
            EMASSERT(m_val->m_intVec->size() <= 8);
            const uint16_t* p = reinterpret_cast<const uint16_t*>(pointer);
            for (auto v : *m_val->m_intVec) {
                if (*p++ != v) {
                    return false;
                }
            }
        } break;
        case 8: {
            EMASSERT(m_val->m_intVec->size() <= 16);
            const uint8_t* p = reinterpret_cast<const uint8_t*>(pointer);
            for (auto v : *m_val->m_intVec) {
                if (*p++ != v) {
                    return false;
                }
            }
        } break;
        default:
            EMUNREACHABLE();
        }
        return true;
    }
    std::string m_regName;
    std::unique_ptr<NumberVector> m_val;
};
}

std::unique_ptr<Check> Check::createCheckRegisterEqConst(const char* name, unsigned long long val)
{
    return std::unique_ptr<Check>(new CheckRegisterEqConst(name, val));
}

std::unique_ptr<Check> Check::createCheckRegisterEqFloatConst(const char* name, double val)
{
    return std::unique_ptr<Check>(new CheckRegisterEqFloatConst(name, val));
}

std::unique_ptr<Check> Check::createCheckRegisterEq(const char* name1, const char* name2)
{
    return std::unique_ptr<Check>(new CheckRegisterEq(name1, name2));
}

std::unique_ptr<Check> Check::createCheckState(unsigned long long val)
{
    return std::unique_ptr<Check>(new CheckState(val));
}

std::unique_ptr<Check> Check::createCheckMemory(const char* registerName, unsigned long long val)
{
    return std::unique_ptr<Check>(new CheckMemory(registerName, val));
}

std::unique_ptr<Check> Check::createCheckVecRegisterEqConst(const char* name, NumberVector* num)
{
    return std::unique_ptr<Check>(new CheckVecRegisterEqConst(name, num));
}
