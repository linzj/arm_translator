#ifndef REGISTEROPERATION_H
#define REGISTEROPERATION_H
#include <unordered_map>
#include <string>

class RegisterOperation {
public:
    RegisterOperation(const RegisterOperation&) = delete;
    const RegisterOperation& operator=(const RegisterOperation&) = delete;
    uintptr_t* getRegisterPointer(CPUARMState* state, const std::string& registerName);
    const uintptr_t* getRegisterPointer(const CPUARMState* state, const std::string& registerName);
    size_t getRegisterPointerOffset(const char* registerName);
    static RegisterOperation& getDefault();

private:
    RegisterOperation();
    std::unordered_map<std::string, size_t> m_map;
};
#endif /* REGISTEROPERATION_H */
