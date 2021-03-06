#ifndef CHECK_H
#define CHECK_H
#include <memory>
#include <string>
#include "cpu.h"
struct NumberVector;
class Check {
public:
    Check();
    virtual ~Check();
    Check(const Check&) = delete;
    const Check& operator=(const Check&) = delete;
    virtual bool check(const CPUARMState* state, const uintptr_t* twoWords, std::string& info) const = 0;
    static std::unique_ptr<Check> createCheckRegisterEqConst(const char* name, unsigned long long val);
    static std::unique_ptr<Check> createCheckRegisterEqFloatConst(const char* name, double val);
    static std::unique_ptr<Check> createCheckVecRegisterEqConst(const char* name, NumberVector*);
    static std::unique_ptr<Check> createCheckRegisterEq(const char* name1, const char* name2);
    static std::unique_ptr<Check> createCheckState(unsigned long long val);
    static std::unique_ptr<Check> createCheckMemory(const char* registerName, unsigned long long val);
};

#endif /* CHECK_H */
