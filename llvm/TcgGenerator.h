#ifndef TCGGENERATOR_H
#define TCGGENERATOR_H
#include "tcg_functions.h"
#include "cpu.h"
namespace jit {
struct TranslateDesc {
    void* m_dispDirect;
    void* m_dispIndirect;
    void* m_dispAssist;
};
void translate(CPUARMState* env, const TranslateDesc& desc, void** buffer, size_t* s);
}
#endif /* TCGGENERATOR_H */
