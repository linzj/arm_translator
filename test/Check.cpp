#include "Check.h"
#include "RegisterOperation.h"
#include <sstream>
#include <iomanip>
Check::Check() {}
Check::~Check() {}
namespace {
class CheckRegisterEqConst : public Check {
public:
    CheckRegisterEqConst(const char* name, unsigned long long val)
        : m_regName(name)
        , m_val(val)
    {
    }

private:
    virtual bool check(const VexGuestState* state, const uintptr_t*, std::string& info) const override
    {
        RegisterOperation& op = RegisterOperation::getDefault();
        const uintptr_t* p = op.getRegisterPointer(state, m_regName);
        std::ostringstream oss;
        oss << "CheckRegisterEqConst " << ((*p == m_val) ? "PASSED" : "FAILED")
            << "; m_regName = " << m_regName
            << "; m_val = " << std::hex
            << m_val;
        info = oss.str();
        return *p == m_val;
    }
    std::string m_regName;
    unsigned long long m_val;
};

class CheckRegisterEq : public Check {
public:
    CheckRegisterEq(const char* name1, const char* name2)
        : m_regName1(name1)
        , m_regName2(name2)
    {
    }

private:
    virtual bool check(const VexGuestState* state, const uintptr_t*, std::string& info) const override
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
    virtual bool check(const VexGuestState*, const uintptr_t* w, std::string& info) const override
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
    virtual bool check(const VexGuestState* state, const uintptr_t*, std::string& info) const override
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
}

std::unique_ptr<Check> Check::createCheckRegisterEqConst(const char* name, unsigned long long val)
{
    return std::unique_ptr<Check>(new CheckRegisterEqConst(name, val));
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
