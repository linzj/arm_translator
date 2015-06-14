#ifndef REGISTEROPERATION_H
#define REGISTEROPERATION_H
#include <unordered_map>
#include <string>
#include "VexHeaders.h"

class RegisterOperation {
public:
    RegisterOperation(const RegisterOperation&) = delete;
    const RegisterOperation& operator=(const RegisterOperation&) = delete;
    uintptr_t* getRegisterPointer(VexGuestState* state, const std::string& registerName);
    const uintptr_t* getRegisterPointer(const VexGuestState* state, const std::string& registerName);
    size_t getRegisterPointerOffset(const char* registerName);
    static RegisterOperation& getDefault();

private:
    RegisterOperation();
    std::unordered_map<std::string, size_t> m_map;
};
#endif /* REGISTEROPERATION_H */
