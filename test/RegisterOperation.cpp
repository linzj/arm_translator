#include "RegisterOperation.h"
#include "VexHeaders.h"

#define REGISTER_LIST(macro)                                        \
    macro("rax", offsetof(VexGuestAMD64State,guest_RAX))            \
    macro("rcx", offsetof(VexGuestAMD64State, guest_RCX))           \
    macro("rdx", offsetof(VexGuestAMD64State, guest_RDX))           \
    macro("rbx", offsetof(VexGuestAMD64State, guest_RBX))           \
    macro("rsp", offsetof(VexGuestAMD64State, guest_RSP))           \
    macro("rbp", offsetof(VexGuestAMD64State, guest_RBP))           \
    macro("rsi", offsetof(VexGuestAMD64State, guest_RSI))           \
    macro("rdi", offsetof(VexGuestAMD64State, guest_RDI))           \
    macro("rip", offsetof(VexGuestAMD64State, guest_RIP))           \
    macro("r8", offsetof(VexGuestAMD64State, guest_R8))             \
    macro("r9", offsetof(VexGuestAMD64State, guest_R9))             \
    macro("r10", offsetof(VexGuestAMD64State, guest_R10))           \
    macro("r11", offsetof(VexGuestAMD64State, guest_R11))           \
    macro("r12", offsetof(VexGuestAMD64State, guest_R12))           \
    macro("r13", offsetof(VexGuestAMD64State, guest_R13))           \
    macro("r14", offsetof(VexGuestAMD64State, guest_R14))           \
    macro("r15", offsetof(VexGuestAMD64State, guest_R15))

#define INIT_LIST(name, off) \
    {                        \
        name, off            \
    }                        \
    ,

RegisterOperation::RegisterOperation()
    : m_map({ REGISTER_LIST(INIT_LIST) })
{
}

uintptr_t* RegisterOperation::getRegisterPointer(VexGuestState* state, const std::string& registerName)
{
    const uintptr_t* ret = getRegisterPointer(static_cast<const VexGuestState*>(state), registerName);
    return const_cast<uintptr_t*>(ret);
}

const uintptr_t* RegisterOperation::getRegisterPointer(const VexGuestState* state, const std::string& registerName)
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
