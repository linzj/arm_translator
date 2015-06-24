#ifndef REGISTERINIT_H
#define REGISTERINIT_H
#include <stdint.h>
#include <memory>
#include <string>
#include "cpu.h"
class RegisterInitControl {
public:
    RegisterInitControl() = default;
    virtual ~RegisterInitControl();
    RegisterInitControl(const RegisterInitControl&) = delete;
    const RegisterInitControl& operator=(const RegisterInitControl&) = delete;

    virtual void init(CPUARMState& env, const std::string& name) = 0;
    virtual void reset() = 0;

    static std::unique_ptr<RegisterInitControl> createConstantInit(uintptr_t val);
    static std::unique_ptr<RegisterInitControl> createMemoryInit(unsigned long long size, unsigned long long val);
    static std::unique_ptr<RegisterInitControl> createVecInit(void* vec);
};
struct RegisterInit {
    std::string m_name;
    std::unique_ptr<RegisterInitControl> m_control;
};
#endif /* REGISTERINIT_H */
