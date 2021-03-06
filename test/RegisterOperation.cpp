#include "RegisterOperation.h"

#define REGISTER_LIST(macro)                                       \
    macro("r0", offsetof(CPUARMState, regs[0]))                    \
    macro("r1", offsetof(CPUARMState, regs[1]))                    \
    macro("r2", offsetof(CPUARMState, regs[2]))                    \
    macro("r3", offsetof(CPUARMState, regs[3]))                    \
    macro("r4", offsetof(CPUARMState, regs[4]))                    \
    macro("r5", offsetof(CPUARMState, regs[5]))                    \
    macro("r6", offsetof(CPUARMState, regs[6]))                    \
    macro("r7", offsetof(CPUARMState, regs[7]))                    \
    macro("r8", offsetof(CPUARMState, regs[8]))                    \
    macro("r9", offsetof(CPUARMState, regs[9]))                    \
    macro("r10", offsetof(CPUARMState, regs[10]))                  \
    macro("r11", offsetof(CPUARMState, regs[11]))                  \
    macro("r12", offsetof(CPUARMState, regs[12]))                  \
    macro("r13", offsetof(CPUARMState, regs[13]))                  \
    macro("r14", offsetof(CPUARMState, regs[14]))                  \
    macro("r15", offsetof(CPUARMState, regs[15]))                  \
    macro("q0", offsetof(CPUARMState, vfp.regs[0]))                \
    macro("q1", offsetof(CPUARMState, vfp.regs[2]))                \
    macro("q2", offsetof(CPUARMState, vfp.regs[4]))                \
    macro("q3", offsetof(CPUARMState, vfp.regs[6]))                \
    macro("q4", offsetof(CPUARMState, vfp.regs[8]))                \
    macro("q5", offsetof(CPUARMState, vfp.regs[10]))               \
    macro("q6", offsetof(CPUARMState, vfp.regs[12]))               \
    macro("q7", offsetof(CPUARMState, vfp.regs[14]))               \
    macro("d0", offsetof(CPUARMState, vfp.regs[0]))                \
    macro("d1", offsetof(CPUARMState, vfp.regs[1]))                \
    macro("d2", offsetof(CPUARMState, vfp.regs[2]))                \
    macro("d3", offsetof(CPUARMState, vfp.regs[3]))                \
    macro("d4", offsetof(CPUARMState, vfp.regs[4]))                \
    macro("d5", offsetof(CPUARMState, vfp.regs[5]))                \
    macro("d6", offsetof(CPUARMState, vfp.regs[6]))                \

#define INIT_LIST(name, off) \
    {                        \
        name, off            \
    }                        \
    ,

RegisterOperation::RegisterOperation()
    : m_map({ REGISTER_LIST(INIT_LIST) })
{
}

uintptr_t* RegisterOperation::getRegisterPointer(CPUARMState* state, const std::string& registerName)
{
    const uintptr_t* ret = getRegisterPointer(static_cast<const CPUARMState*>(state), registerName);
    return const_cast<uintptr_t*>(ret);
}

const uintptr_t* RegisterOperation::getRegisterPointer(const CPUARMState* state, const std::string& registerName)
{
    auto found = m_map.find(registerName);
    if (found == m_map.end()) {
        return nullptr;
    }
    return reinterpret_cast<const uintptr_t*>(reinterpret_cast<const char*>(state) + found->second);
}

size_t RegisterOperation::getRegisterPointerOffset(const char* registerName)
{
    auto found = m_map.find(registerName);
    if (found == m_map.end()) {
        return 0;
    }
    return found->second;
}

RegisterOperation& RegisterOperation::getDefault()
{
    static RegisterOperation s_op;
    return s_op;
}
