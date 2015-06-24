#include <vector>
#include <stdlib.h>
#include "log.h"
#include "RegisterInit.h"
#include "RegisterOperation.h"
#include "RegisterAssign.h"
#include "Vec.h"
RegisterInitControl::~RegisterInitControl() {}

namespace {
class RegisterInitConstant : public RegisterInitControl {
public:
    RegisterInitConstant(uintptr_t val)
        : m_val(val)
    {
    }

private:
    void init(CPUARMState& env, const std::string& name) override;
    void reset() override;
    uintptr_t m_val;
};

class RegisterInitMemory : public RegisterInitControl {
public:
    RegisterInitMemory(unsigned long long size, unsigned long long val)
        : m_size(size)
        , m_val(val)
        , m_buffer(nullptr)
    {
    }
    ~RegisterInitMemory();

private:
    void init(CPUARMState& env, const std::string& name) override;
    uintptr_t getVal();
    void reset() override;
    unsigned long long m_size;
    unsigned long long m_val;
    void* m_buffer;
};

class RegisterInitNumVec : public RegisterInitControl {
public:
    RegisterInitNumVec(NumberVector* val)
        : m_val(val)
    {
    }

private:
    void init(CPUARMState& env, const std::string& name) override;
    void reset() override;
    std::unique_ptr<NumberVector> m_val;
};

void RegisterInitConstant::init(CPUARMState& env, const std::string& name)
{
    RegisterAssign assign;
    assign.assign(&env, name, m_val);
}

void RegisterInitConstant::reset()
{
}

RegisterInitMemory::~RegisterInitMemory()
{
    if (m_buffer) {
        free(m_buffer);
    }
}

uintptr_t RegisterInitMemory::getVal()
{
    if (m_size <= 0) {
        return 0;
    }
    EMASSERT(m_buffer == nullptr);
    m_buffer = malloc(m_size);
    if (m_size == 1) {
        *static_cast<uint8_t*>(m_buffer) = static_cast<uint8_t>(m_val);
    }
    else if (m_size == 2) {
        *static_cast<uint16_t*>(m_buffer) = static_cast<uint16_t>(m_val);
    }
    else {
        *static_cast<uint32_t*>(m_buffer) = static_cast<uint32_t>(m_val);
    }
    uintptr_t val = reinterpret_cast<uintptr_t>(m_buffer);
}

void RegisterInitMemory::init(CPUARMState& env, const std::string& name)
{
    RegisterAssign assign;
    assign.assign(&env, name, getVal());
}

void RegisterInitMemory::reset()
{
    if (m_buffer) {
        free(m_buffer);
        m_buffer = nullptr;
    }
}

void RegisterInitNumVec::init(CPUARMState& env, const std::string& name)
{
    RegisterOperation& op = RegisterOperation::getDefault();
    uintptr_t* pointer = op.getRegisterPointer(&env, name);
    switch (m_val->m_type) {
    case 64: {
        EMASSERT(m_val->m_intVec->size() <= 2);
        uint64_t* p = reinterpret_cast<uint64_t*>(pointer);
        for (auto v : *m_val->m_intVec) {
            *p++ = v;
        }
    } break;
    case 32: {
        EMASSERT(m_val->m_intVec->size() <= 4);
        uint32_t* p = reinterpret_cast<uint32_t*>(pointer);
        for (auto v : *m_val->m_intVec) {
            *p++ = v;
        }
    } break;
    case 16: {
        EMASSERT(m_val->m_intVec->size() <= 8);
        uint16_t* p = reinterpret_cast<uint16_t*>(pointer);
        for (auto v : *m_val->m_intVec) {
            *p++ = v;
        }
    } break;
    case 8: {
        EMASSERT(m_val->m_intVec->size() <= 16);
        uint8_t* p = reinterpret_cast<uint8_t*>(pointer);
        for (auto v : *m_val->m_intVec) {
            *p++ = v;
        }
    } break;
    default:
        EMUNREACHABLE();
    }
}

void RegisterInitNumVec::reset()
{
}
}

std::unique_ptr<RegisterInitControl> RegisterInitControl::createConstantInit(uintptr_t val)
{
    return std::unique_ptr<RegisterInitControl>(new RegisterInitConstant(val));
}

std::unique_ptr<RegisterInitControl> RegisterInitControl::createMemoryInit(unsigned long long size, unsigned long long val)
{
    return std::unique_ptr<RegisterInitControl>(new RegisterInitMemory(size, val));
}

std::unique_ptr<RegisterInitControl> RegisterInitControl::createVecInit(void* vec)
{
    return std::unique_ptr<RegisterInitControl>(new RegisterInitNumVec(static_cast<NumberVector*>(vec)));
}
