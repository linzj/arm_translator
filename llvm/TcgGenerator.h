#ifndef TCGGENERATOR_H
#define TCGGENERATOR_H
#include "tcg_functions.h"
#include "cpu.h"
namespace jit {
void translate(CPUARMState* env, void** buffer, size_t* s);
}
#endif /* TCGGENERATOR_H */
