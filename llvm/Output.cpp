#include <assert.h>
#include <llvm/IR/IRBuilder.h>
#include "CompilerState.h"
#include "Output.h"

namespace jit {
Output::Output(CompilerState& state)
    : m_state(state)
    , m_repo(state.m_context, state.m_module)
    , m_builder(nullptr)
    , m_stackMapsId(1)
{
    m_argType = pointerType(arrayType(repo().intPtr, state.m_platformDesc.m_contextSize / sizeof(intptr_t)));
    state.m_function = addFunction(
        state.m_module, "main", functionType(repo().int64, m_argType));
    m_builder = llvmAPI->CreateBuilderInContext(state.m_context);

    m_prologue = appendBasicBlock("Prologue");
    positionToBBEnd(m_prologue);
    buildGetArg();
}

Output::~Output()
{
    llvmAPI->DisposeBuilder(m_builder);
}

LBasicBlock Output::appendBasicBlock(const char* name)
{
    return jit::appendBasicBlock(m_state.m_context, m_state.m_function, name);
}

void Output::positionToBBEnd(LBasicBlock bb)
{
    m_current = bb;
    llvmAPI->PositionBuilderAtEnd(m_builder, bb);
}

LValue Output::constInt1(int val)
{
    return jit::constInt(repo().int1, val);
}

LValue Output::constInt8(int val)
{
    return jit::constInt(repo().int8, val);
}

LValue Output::constInt16(int val)
{
    return jit::constInt(repo().int16, val);
}

LValue Output::constInt32(int i)
{
    return jit::constInt(m_repo.int32, i);
}

LValue Output::constInt64(long long l)
{
    return jit::constInt(m_repo.int64, l);
}

LValue Output::constInt128(long long l)
{
    return jit::constInt(m_repo.int128, l);
}

LValue Output::constIntPtr(uintptr_t l)
{
    return jit::constInt(m_repo.intPtr, l);
}

LValue Output::constFloat(double val)
{
    return jit::constReal(m_repo.floatType, val);
}

LValue Output::constDouble(double val)
{
    return jit::constReal(m_repo.doubleType, val);
}

LValue Output::constV128(unsigned short val)
{
    LValue tmp = constInt16(val);
    LValue tmpArray[] = { tmp, tmp, tmp, tmp };
    return llvmAPI->ConstVector(tmpArray, 4);
}

LValue Output::constV256(unsigned val)
{
    LValue tmp = constInt32(val);
    LValue tmpArray[] = { tmp, tmp, tmp, tmp };
    return llvmAPI->ConstVector(tmpArray, 4);
}

LValue constV256(unsigned);

LValue Output::buildStructGEP(LValue structVal, unsigned field)
{
    return jit::buildStructGEP(m_builder, structVal, field);
}

LValue Output::buildLoad(LValue toLoad)
{
    return jit::buildLoad(m_builder, toLoad);
}

LValue Output::buildStore(LValue val, LValue pointer)
{
    return jit::buildStore(m_builder, val, pointer);
}

LValue Output::buildAdd(LValue lhs, LValue rhs)
{
    return jit::buildAdd(m_builder, lhs, rhs);
}

LValue Output::buildOr(LValue left, LValue right)
{
    return llvmAPI->BuildOr(m_builder, left, right, "");
}

LValue Output::buildShl(LValue lhs, LValue rhs)
{
    return llvmAPI->BuildShl(m_builder, lhs, rhs, "");
}

LValue Output::buildLShr(LValue lhs, LValue rhs)
{
    return llvmAPI->BuildLShr(m_builder, lhs, rhs, "");
}

LValue Output::buildAnd(LValue lhs, LValue rhs)
{
    return jit::buildAnd(m_builder, lhs, rhs);
}

LValue Output::buildBr(LBasicBlock bb)
{
    return jit::buildBr(m_builder, bb);
}

LValue Output::buildCondBr(LValue condition, LBasicBlock taken, LBasicBlock notTaken)
{
    return jit::buildCondBr(m_builder, condition, taken, notTaken);
}

LValue Output::buildRet(LValue ret)
{
    return jit::buildRet(m_builder, ret);
}

LValue Output::buildRetVoid(void)
{
    return jit::buildRetVoid(m_builder);
}

LValue Output::buildCast(LLVMOpcode Op, LLVMValueRef Val, LLVMTypeRef DestTy)
{
    return llvmAPI->BuildCast(m_builder, Op, Val, DestTy, "");
}

LValue Output::buildPointerCast(LLVMValueRef Val, LLVMTypeRef DestTy)
{
    return llvmAPI->BuildPointerCast(m_builder, Val, DestTy, "");
}

LValue Output::buildPhi(LType type) { return llvmAPI->BuildPhi(m_builder, type, ""); }

void Output::buildGetArg()
{
    m_arg = llvmAPI->GetParam(m_state.m_function, 0);
}

void Output::buildDirectPatch(uintptr_t where)
{
    PatchDesc desc = { PatchType::Direct };
    buildPatchCommon(constInt64(where), desc, m_state.m_platformDesc.m_directSize);
}

void Output::buildDirectPatch(LValue where)
{
    PatchDesc desc = { PatchType::Direct };
    buildPatchCommon(where, desc, m_state.m_platformDesc.m_directSize);
}

void Output::buildDirectSlowPatch(uintptr_t where)
{
    PatchDesc desc = { PatchType::DirectSlow };
    buildPatchCommon(constInt64(where), desc, m_state.m_platformDesc.m_directSize);
}

void Output::buildDirectSlowPatch(LValue where)
{
    PatchDesc desc = { PatchType::DirectSlow };
    buildPatchCommon(where, desc, m_state.m_platformDesc.m_directSize);
}

void Output::buildIndirectPatch(LValue where)
{
    PatchDesc desc = { PatchType::Indirect };
    buildPatchCommon(where, desc, m_state.m_platformDesc.m_indirectSize);
}

void Output::buildAssistPatch(LValue where)
{
    PatchDesc desc = { PatchType::Assist };
    buildPatchCommon(where, desc, m_state.m_platformDesc.m_assistSize);
}

void Output::buildPatchCommon(LValue where, const PatchDesc& desc, size_t patchSize)
{
    LValue constIndex[] = { constInt32(0), constInt32(m_state.m_platformDesc.m_pcFieldOffset / sizeof(intptr_t)) };
    buildStore(where, llvmAPI->BuildInBoundsGEP(m_builder, m_arg, constIndex, 2, ""));
    LValue call = buildCall(repo().patchpointInt64Intrinsic(), constInt32(m_stackMapsId), constInt32(patchSize), constNull(repo().ref8), constInt32(0));
    llvmAPI->SetInstructionCallConv(call, LLVMAnyRegCallConv);
    buildUnreachable(m_builder);
    // record the stack map info
    m_state.m_patchMap.insert(std::make_pair(m_stackMapsId++, desc));
}

LValue Output::buildLoadArgIndex(int index)
{
    LValue constIndex[] = { constInt32(0), constInt32(index) };
    return buildLoad(llvmAPI->BuildInBoundsGEP(m_builder, m_arg, constIndex, 2, ""));
}

LValue Output::buildStoreArgIndex(LValue val, int index)
{
    LValue constIndex[] = { constInt32(0), constInt32(index) };
    return buildStore(val, llvmAPI->BuildInBoundsGEP(m_builder, m_arg, constIndex, 2, ""));
}

LValue Output::buildArgBytePointer()
{
    LValue casted = buildCast(LLVMBitCast, m_arg, repo().ref8);
    return casted;
}

LValue Output::buildSelect(LValue condition, LValue taken, LValue notTaken)
{
    return jit::buildSelect(m_builder, condition, taken, notTaken);
}

LValue Output::buildICmp(LIntPredicate cond, LValue left, LValue right)
{
    return jit::buildICmp(m_builder, cond, left, right);
}

LValue Output::buildAtomicCmpXchg(LValue addr, LValue cmp, LValue val)
{
    llvm::IRBuilder<>* builder = llvm::unwrap(m_builder);
    LValue pair = llvm::wrap(builder->CreateAtomicCmpXchg(llvm::unwrap(addr), llvm::unwrap(cmp), llvm::unwrap(val), llvm::SequentiallyConsistent, llvm::SequentiallyConsistent));
    return llvmAPI->BuildExtractValue(m_builder, pair, 0, "");
}

LType Output::typeOf(LValue val)
{
    return llvmAPI->TypeOf(val);
}
}
