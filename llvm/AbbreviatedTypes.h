#ifndef ABBREVIATEDTYPES_H
#define ABBREVIATEDTYPES_H
#include "LLVMHeaders.h"
namespace jit {
typedef LLVMAtomicOrdering LAtomicOrdering;
typedef LLVMBasicBlockRef LBasicBlock;
typedef LLVMBuilderRef LBuilder;
typedef LLVMCallConv LCallConv;
typedef LLVMContextRef LContext;
typedef LLVMIntPredicate LIntPredicate;
typedef LLVMLinkage LLinkage;
typedef LLVMModuleRef LModule;
typedef LLVMRealPredicate LRealPredicate;
typedef LLVMTypeRef LType;
typedef LLVMValueRef LValue;
typedef LLVMMemoryBufferRef LMemoryBuffer;
}
#endif /* ABBREVIATEDTYPES_H */
