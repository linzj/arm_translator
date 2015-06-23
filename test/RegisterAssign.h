#ifndef REGISTERASSIGN_H
#define REGISTERASSIGN_H
#include <memory>
#include <string>
#include "cpu.h"
class RegisterOperation;

class RegisterAssign {
public:
    RegisterAssign();
    ~RegisterAssign();
    void assign(CPUARMState* state, const std::string& registerName, unsigned long long val);
    void assign(CPUARMState* state, const std::string& registerName, const void* data);
};
#endif /* REGISTERASSIGN_H */
