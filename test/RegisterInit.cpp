#include <stdlib.h>
#include "log.h"
#include "RegisterInit.h"
RegisterInitControl::~RegisterInitControl() {}

namespace {
class RegisterInitConstant : public RegisterInitControl {
public:
    RegisterInitConstant(uintptr_t val)
        : m_val(val)
    {
    }

private:
    uintptr_t init() override;
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
    uintptr_t init() override;
    void reset() override;
    unsigned long long m_size;
    unsigned long long m_val;
    void* m_buffer;
};

uintptr_t RegisterInitConstant::init()
{
    return m_val;
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

uintptr_t RegisterInitMemory::init()
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
    return reinterpret_cast<uintptr_t>(m_buffer);
}

void RegisterInitMemory::reset()
{
    if (m_buffer) {
        free(m_buffer);
        m_buffer = nullptr;
    }
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
