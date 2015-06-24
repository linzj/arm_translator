#ifndef TCGGENERATOR_H
#define TCGGENERATOR_H
#include "tcg_functions.h"
#include "cpu.h"
namespace jit {
class ExecutableMemoryAllocator;
struct TranslateDesc {
    void* m_dispDirect;
    void* m_dispIndirect;
    void* m_dispAssist;
    ExecutableMemoryAllocator* m_executableMemAllocator;
};
void translate(CPUARMState* env, const TranslateDesc& desc);
}
#endif /* TCGGENERATOR_H */
