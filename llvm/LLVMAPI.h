#ifndef LLVMAPI_H
#define LLVMAPI_H
#include "LLVMAPIFunctions.h"

struct LLVMAPI {
#define LLVM_API_FUNCTION_DECLARATION(returnType, name, signature) \
    returnType (*name) signature;
    FOR_EACH_LLVM_API_FUNCTION(LLVM_API_FUNCTION_DECLARATION)
#undef LLVM_API_FUNCTION_DECLARATION
    
};
extern LLVMAPI* llvmAPI;
#endif /* LLVMAPI_H */
