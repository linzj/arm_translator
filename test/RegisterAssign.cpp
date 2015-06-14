#include "log.h"
#include "RegisterOperation.h"
#include "RegisterAssign.h"

RegisterAssign::RegisterAssign()
{
}
RegisterAssign::~RegisterAssign() {}
void RegisterAssign::assign(VexGuestState* state, const std::string& registerName, unsigned long long val)
{
    uintptr_t* p = RegisterOperation::getDefault().getRegisterPointer(state, registerName);
    EMASSERT(p != nullptr);
    *p = val;
}
