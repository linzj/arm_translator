#ifndef TCGGENERATOR_H
#define TCGGENERATOR_H
#include "tcg_functions.h"
#include "cpu.h"
namespace jit {
class ExecutableMemoryAllocator;
struct TranslateDesc {
    void* m_dispDirect;
    void* m_dispIndirect;
    void (*m_dispHot)(CPUARMState*);
    ExecutableMemoryAllocator* m_executableMemAllocator;
    bool m_optimal;
    // output is here
    size_t m_guestExtents;
};
void translate(CPUARMState* env, TranslateDesc& desc);
void patchDirectJump(uintptr_t from, uintptr_t to);
void unpatchDirectJump(uintptr_t from, uintptr_t to);
}
#endif /* TCGGENERATOR_H */
