#ifndef ABBREVIATIONS_H
#define ABBREVIATIONS_H
#include <cstring>
#include "AbbreviatedTypes.h"
#include "LLVMAPI.h"

// This file contains short-form calls into the LLVM C API. It is meant to
// save typing and make the lowering code clearer. If we ever call an LLVM C API
// function more than once in the FTL lowering code, we should add a shortcut for
// it here.
namespace jit {

static inline LType voidType(LContext context) { return llvmAPI->VoidTypeInContext(context); }
static inline LType int1Type(LContext context) { return llvmAPI->Int1TypeInContext(context); }
static inline LType int8Type(LContext context) { return llvmAPI->Int8TypeInContext(context); }
static inline LType int16Type(LContext context) { return llvmAPI->Int16TypeInContext(context); }
static inline LType int32Type(LContext context) { return llvmAPI->Int32TypeInContext(context); }
static inline LType int64Type(LContext context) { return llvmAPI->Int64TypeInContext(context); }
static inline LType int128Type(LContext context) { return llvmAPI->IntTypeInContext(context, 128); }
static inline LType intPtrType(LContext context) { return llvmAPI->Int64TypeInContext(context); }
static inline LType floatType(LContext context) { return llvmAPI->FloatTypeInContext(context); }
static inline LType doubleType(LContext context) { return llvmAPI->DoubleTypeInContext(context); }

static inline LType pointerType(LType type) { return llvmAPI->PointerType(type, 0); }
static inline LType arrayType(LType type, unsigned count) { return llvmAPI->ArrayType(type, count); }
static inline LType vectorType(LType type, unsigned count) { return llvmAPI->VectorType(type, count); }

enum PackingMode { NotPacked,
    Packed };
static inline LType structType(LContext context, LType* elementTypes, unsigned elementCount, PackingMode packing = NotPacked)
{
    return llvmAPI->StructTypeInContext(context, elementTypes, elementCount, packing == Packed);
}
static inline LType structType(LContext context, PackingMode packing = NotPacked)
{
    return structType(context, 0, 0, packing);
}
static inline LType structType(LContext context, LType element1, PackingMode packing = NotPacked)
{
    return structType(context, &element1, 1, packing);
}
static inline LType structType(LContext context, LType element1, LType element2, PackingMode packing = NotPacked)
{
    LType elements[] = { element1, element2 };
    return structType(context, elements, 2, packing);
}

// FIXME: Make the Variadicity argument not be the last argument to functionType() so that this function
// can use C++11 variadic templates
// https://bugs.webkit.org/show_bug.cgi?id=141575
enum Variadicity { NotVariadic,
    Variadic };
static inline LType functionType(LType returnType, const LType* paramTypes, unsigned paramCount, Variadicity variadicity)
{
    return llvmAPI->FunctionType(returnType, const_cast<LType*>(paramTypes), paramCount, variadicity == Variadic);
}
template <typename VectorType>
inline LType functionType(LType returnType, const VectorType& vector, Variadicity variadicity = NotVariadic)
{
    return functionType(returnType, vector.begin(), vector.size(), variadicity);
}
static inline LType functionType(LType returnType, Variadicity variadicity = NotVariadic)
{
    return functionType(returnType, 0, 0, variadicity);
}
static inline LType functionType(LType returnType, LType param1, Variadicity variadicity = NotVariadic)
{
    return functionType(returnType, &param1, 1, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2 };
    return functionType(returnType, paramTypes, 2, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, LType param3, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2, param3 };
    return functionType(returnType, paramTypes, 3, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, LType param3, LType param4, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2, param3, param4 };
    return functionType(returnType, paramTypes, 4, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, LType param3, LType param4, LType param5, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2, param3, param4, param5 };
    return functionType(returnType, paramTypes, 5, variadicity);
}
static inline LType functionType(LType returnType, LType param1, LType param2, LType param3, LType param4, LType param5, LType param6, Variadicity variadicity = NotVariadic)
{
    LType paramTypes[] = { param1, param2, param3, param4, param5, param6 };
    return functionType(returnType, paramTypes, 6, variadicity);
}

static inline LType typeOf(LValue value) { return llvmAPI->TypeOf(value); }

static inline LType getElementType(LType value) { return llvmAPI->GetElementType(value); }

static inline LValue sizeOf(LType type) { return llvmAPI->SizeOf(type); }

static inline unsigned mdKindID(LContext context, const char* string) { return llvmAPI->GetMDKindIDInContext(context, string, std::strlen(string)); }
static inline LValue mdString(LContext context, const char* string, unsigned length) { return llvmAPI->MDStringInContext(context, string, length); }
static inline LValue mdString(LContext context, const char* string) { return mdString(context, string, std::strlen(string)); }
static inline LValue mdNode(LContext context, LValue* args, unsigned numArgs) { return llvmAPI->MDNodeInContext(context, args, numArgs); }
template <typename VectorType>
static inline LValue mdNode(LContext context, const VectorType& vector) { return mdNode(context, const_cast<LValue*>(vector.begin()), vector.size()); }
static inline LValue mdNode(LContext context) { return mdNode(context, 0, 0); }
static inline LValue mdNode(LContext context, LValue arg1) { return mdNode(context, &arg1, 1); }
static inline LValue mdNode(LContext context, LValue arg1, LValue arg2)
{
    LValue args[] = { arg1, arg2 };
    return mdNode(context, args, 2);
}
static inline LValue mdNode(LContext context, LValue arg1, LValue arg2, LValue arg3)
{
    LValue args[] = { arg1, arg2, arg3 };
    return mdNode(context, args, 3);
}

static inline void setMetadata(LValue instruction, unsigned kind, LValue metadata) { llvmAPI->SetMetadata(instruction, kind, metadata); }

static inline LValue getFirstInstruction(LBasicBlock block) { return llvmAPI->GetFirstInstruction(block); }
static inline LValue getNextInstruction(LValue instruction) { return llvmAPI->GetNextInstruction(instruction); }

static inline LValue addFunction(LModule module, const char* name, LType type) { return llvmAPI->AddFunction(module, name, type); }
static inline LValue getNamedFunction(LModule module, const char* name) { return llvmAPI->GetNamedFunction(module, name); }
static inline LValue getFirstFunction(LModule module) { return llvmAPI->GetFirstFunction(module); }
static inline LValue getNextFunction(LValue function) { return llvmAPI->GetNextFunction(function); }

static inline void setFunctionCallingConv(LValue function, LCallConv convention) { llvmAPI->SetFunctionCallConv(function, convention); }
static inline void addTargetDependentFunctionAttr(LValue function, const char* key, const char* value) { llvmAPI->AddTargetDependentFunctionAttr(function, key, value); }
static inline void removeFunctionAttr(LValue function, LLVMAttribute pa) { llvmAPI->RemoveFunctionAttr(function, pa); }

static inline LLVMLinkage getLinkage(LValue global) { return llvmAPI->GetLinkage(global); }
static inline void setLinkage(LValue global, LLVMLinkage linkage) { llvmAPI->SetLinkage(global, linkage); }
static inline void setVisibility(LValue global, LLVMVisibility viz) { llvmAPI->SetVisibility(global, viz); }
static inline LLVMBool isDeclaration(LValue global) { return llvmAPI->IsDeclaration(global); }

static inline LLVMBool linkModules(LModule dest, LModule str, LLVMLinkerMode mode, char** outMessage) { return llvmAPI->LinkModules(dest, str, mode, outMessage); }

static inline const char* getValueName(LValue global) { return llvmAPI->GetValueName(global); }

static inline LValue getNamedGlobal(LModule module, const char* name) { return llvmAPI->GetNamedGlobal(module, name); }
static inline LValue getFirstGlobal(LModule module) { return llvmAPI->GetFirstGlobal(module); }
static inline LValue getNextGlobal(LValue global) { return llvmAPI->GetNextGlobal(global); }

static inline LValue addExternFunction(LModule module, const char* name, LType type)
{
    LValue result = addFunction(module, name, type);
    setLinkage(result, LLVMExternalLinkage);
    return result;
}

static inline LLVMBool createMemoryBufferWithContentsOfFile(const char* path, LLVMMemoryBufferRef* outMemBuf, char** outMessage)
{
    return llvmAPI->CreateMemoryBufferWithContentsOfFile(path, outMemBuf, outMessage);
}

static inline LLVMBool parseBitcodeInContext(LLVMContextRef contextRef, LLVMMemoryBufferRef memBuf, LModule* outModule, char** outMessage)
{
    return llvmAPI->ParseBitcodeInContext(contextRef, memBuf, outModule, outMessage);
}

static inline void disposeMemoryBuffer(LLVMMemoryBufferRef memBuf) { llvmAPI->DisposeMemoryBuffer(memBuf); }

static inline LModule moduleCreateWithNameInContext(const char* moduleID, LContext context) { return llvmAPI->ModuleCreateWithNameInContext(moduleID, context); }
static inline void disposeModule(LModule m) { llvmAPI->DisposeModule(m); }

static inline void disposeMessage(char* outMsg) { llvmAPI->DisposeMessage(outMsg); }

static inline LValue getParam(LValue function, unsigned index) { return llvmAPI->GetParam(function, index); }

static inline void getParamTypes(LType function, LType* dest) { return llvmAPI->GetParamTypes(function, dest); }
static inline LValue getUndef(LType type) { return llvmAPI->GetUndef(type); }

enum BitExtension { ZeroExtend,
    SignExtend };
static inline LValue constInt(LType type, unsigned long long value, BitExtension extension = ZeroExtend) { return llvmAPI->ConstInt(type, value, extension == SignExtend); }
static inline LValue constReal(LType type, double value) { return llvmAPI->ConstReal(type, value); }
static inline LValue constIntToPtr(LValue value, LType type) { return llvmAPI->ConstIntToPtr(value, type); }
static inline LValue constNull(LType type) { return llvmAPI->ConstNull(type); }
static inline LValue constBitCast(LValue value, LType type) { return llvmAPI->ConstBitCast(value, type); }

static inline LBasicBlock getFirstBasicBlock(LValue function) { return llvmAPI->GetFirstBasicBlock(function); }
static inline LBasicBlock getNextBasicBlock(LBasicBlock block) { return llvmAPI->GetNextBasicBlock(block); }

static inline LBasicBlock appendBasicBlock(LContext context, LValue function, const char* name = "") { return llvmAPI->AppendBasicBlockInContext(context, function, name); }
static inline LBasicBlock insertBasicBlock(LContext context, LBasicBlock beforeBasicBlock, const char* name = "") { return llvmAPI->InsertBasicBlockInContext(context, beforeBasicBlock, name); }

static inline LValue buildPhi(LBuilder builder, LType type) { return llvmAPI->BuildPhi(builder, type, ""); }
static inline void addIncoming(LValue phi, const LValue* values, const LBasicBlock* blocks, unsigned numPredecessors)
{
    llvmAPI->AddIncoming(phi, const_cast<LValue*>(values), const_cast<LBasicBlock*>(blocks), numPredecessors);
}

static inline LValue buildAlloca(LBuilder builder, LType type) { return llvmAPI->BuildAlloca(builder, type, ""); }
static inline LValue buildAdd(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildAdd(builder, left, right, ""); }
static inline LValue buildSub(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildSub(builder, left, right, ""); }
static inline LValue buildMul(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildMul(builder, left, right, ""); }
static inline LValue buildDiv(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildSDiv(builder, left, right, ""); }
static inline LValue buildRem(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildSRem(builder, left, right, ""); }
static inline LValue buildNeg(LBuilder builder, LValue value) { return llvmAPI->BuildNeg(builder, value, ""); }
static inline LValue buildFAdd(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildFAdd(builder, left, right, ""); }
static inline LValue buildFSub(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildFSub(builder, left, right, ""); }
static inline LValue buildFMul(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildFMul(builder, left, right, ""); }
static inline LValue buildFDiv(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildFDiv(builder, left, right, ""); }
static inline LValue buildFRem(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildFRem(builder, left, right, ""); }
static inline LValue buildFNeg(LBuilder builder, LValue value) { return llvmAPI->BuildFNeg(builder, value, ""); }
static inline LValue buildAnd(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildAnd(builder, left, right, ""); }
static inline LValue buildOr(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildOr(builder, left, right, ""); }
static inline LValue buildXor(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildXor(builder, left, right, ""); }
static inline LValue buildShl(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildShl(builder, left, right, ""); }
static inline LValue buildAShr(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildAShr(builder, left, right, ""); }
static inline LValue buildLShr(LBuilder builder, LValue left, LValue right) { return llvmAPI->BuildLShr(builder, left, right, ""); }
static inline LValue buildNot(LBuilder builder, LValue value) { return llvmAPI->BuildNot(builder, value, ""); }
static inline LValue buildLoad(LBuilder builder, LValue pointer) { return llvmAPI->BuildLoad(builder, pointer, ""); }
static inline LValue buildStructGEP(LBuilder builder, LValue pointer, unsigned idx) { return llvmAPI->BuildStructGEP(builder, pointer, idx, ""); }
static inline LValue buildStore(LBuilder builder, LValue value, LValue pointer) { return llvmAPI->BuildStore(builder, value, pointer); }
static inline LValue buildSExt(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildSExt(builder, value, type, ""); }
static inline LValue buildZExt(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildZExt(builder, value, type, ""); }
static inline LValue buildFPToSI(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildFPToSI(builder, value, type, ""); }
static inline LValue buildFPToUI(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildFPToUI(builder, value, type, ""); }
static inline LValue buildSIToFP(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildSIToFP(builder, value, type, ""); }
static inline LValue buildUIToFP(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildUIToFP(builder, value, type, ""); }
static inline LValue buildIntCast(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildIntCast(builder, value, type, ""); }
static inline LValue buildFPCast(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildFPCast(builder, value, type, ""); }
static inline LValue buildIntToPtr(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildIntToPtr(builder, value, type, ""); }
static inline LValue buildPtrToInt(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildPtrToInt(builder, value, type, ""); }
static inline LValue buildBitCast(LBuilder builder, LValue value, LType type) { return llvmAPI->BuildBitCast(builder, value, type, ""); }
static inline LValue buildICmp(LBuilder builder, LIntPredicate cond, LValue left, LValue right) { return llvmAPI->BuildICmp(builder, cond, left, right, ""); }
static inline LValue buildFCmp(LBuilder builder, LRealPredicate cond, LValue left, LValue right) { return llvmAPI->BuildFCmp(builder, cond, left, right, ""); }
static inline LValue buildInsertElement(LBuilder builder, LValue vector, LValue element, LValue index) { return llvmAPI->BuildInsertElement(builder, vector, element, index, ""); }

enum SynchronizationScope { SingleThread,
    CrossThread };
static inline LValue buildFence(LBuilder builder, LAtomicOrdering ordering, SynchronizationScope scope = CrossThread)
{
    return llvmAPI->BuildFence(builder, ordering, scope == SingleThread, "");
}

static inline LValue buildCall(LBuilder builder, LValue function, const LValue* args, unsigned numArgs)
{
    return llvmAPI->BuildCall(builder, function, const_cast<LValue*>(args), numArgs, "");
}
template <typename VectorType>
inline LValue buildCall(LBuilder builder, LValue function, const VectorType& vector)
{
    return buildCall(builder, function, vector.begin(), vector.size());
}
static inline LValue buildCall(LBuilder builder, LValue function)
{
    return buildCall(builder, function, 0, 0);
}
static inline LValue buildCall(LBuilder builder, LValue function, LValue arg1)
{
    return buildCall(builder, function, &arg1, 1);
}
template <typename... Args>
LValue buildCall(LBuilder builder, LValue function, LValue arg1, Args... args)
{
    LValue argsArray[] = { arg1, args... };
    return buildCall(builder, function, argsArray, sizeof(argsArray) / sizeof(LValue));
}

static inline void setInstructionCallingConvention(LValue instruction, LCallConv callingConvention) { llvmAPI->SetInstructionCallConv(instruction, callingConvention); }
static inline LValue buildExtractValue(LBuilder builder, LValue aggVal, unsigned index) { return llvmAPI->BuildExtractValue(builder, aggVal, index, ""); }
static inline LValue buildSelect(LBuilder builder, LValue condition, LValue taken, LValue notTaken) { return llvmAPI->BuildSelect(builder, condition, taken, notTaken, ""); }
static inline LValue buildBr(LBuilder builder, LBasicBlock destination) { return llvmAPI->BuildBr(builder, destination); }
static inline LValue buildCondBr(LBuilder builder, LValue condition, LBasicBlock taken, LBasicBlock notTaken) { return llvmAPI->BuildCondBr(builder, condition, taken, notTaken); }
static inline LValue buildSwitch(LBuilder builder, LValue value, LBasicBlock fallThrough, unsigned numCases) { return llvmAPI->BuildSwitch(builder, value, fallThrough, numCases); }
static inline void addCase(LValue switchInst, LValue value, LBasicBlock target) { llvmAPI->AddCase(switchInst, value, target); }
template <typename VectorType>
static inline LValue buildSwitch(LBuilder builder, LValue value, const VectorType& cases, LBasicBlock fallThrough)
{
    LValue result = buildSwitch(builder, value, fallThrough, cases.size());
    for (unsigned i = 0; i < cases.size(); ++i)
        addCase(result, cases[i].value(), cases[i].target());
    return result;
}
static inline LValue buildRet(LBuilder builder, LValue value) { return llvmAPI->BuildRet(builder, value); }
static inline LValue buildRetVoid(LBuilder builder) { return llvmAPI->BuildRetVoid(builder); }
static inline LValue buildUnreachable(LBuilder builder) { return llvmAPI->BuildUnreachable(builder); }
static inline void setTailCall(LValue callInst, bool istail) { return llvmAPI->SetTailCall(callInst, istail); }

static inline void dumpModule(LModule module) { llvmAPI->DumpModule(module); }
static inline void verifyModule(LModule module)
{
    char* error = 0;
    llvmAPI->VerifyModule(module, LLVMAbortProcessAction, &error);
    llvmAPI->DisposeMessage(error);
}
}
#endif /* ABBREVIATIONS_H */
