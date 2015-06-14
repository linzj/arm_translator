#ifndef OUTPUT_H
#define OUTPUT_H
#include "IntrinsicRepository.h"
namespace jit {
class CompilerState;
class Output {
public:
    Output(CompilerState& state);
    ~Output();
    LBasicBlock appendBasicBlock(const char* name = nullptr);
    void positionToBBEnd(LBasicBlock);
    LValue constInt1(int);
    LValue constInt8(int);
    LValue constInt16(int);
    LValue constInt32(int);
    LValue constInt64(long long);
    LValue constInt128(long long);
    LValue constIntPtr(uintptr_t);
    LValue constFloat(double);
    LValue constDouble(double);
    LValue constV128(unsigned short);
    LValue constV256(unsigned);
    LValue buildStructGEP(LValue structVal, unsigned field);
    LValue buildLoad(LValue toLoad);
    LValue buildStore(LValue val, LValue pointer);
    LValue buildAdd(LValue lhs, LValue rhs);
    LValue buildAnd(LValue lhs, LValue rhs);
    LValue buildOr(LValue left, LValue right);
    LValue buildShl(LValue lhs, LValue rhs);
    LValue buildLShr(LValue lhs, LValue rhs);
    LValue buildBr(LBasicBlock bb);
    LValue buildCondBr(LValue condition, LBasicBlock taken, LBasicBlock notTaken);
    LValue buildRet(LValue ret);
    LValue buildRetVoid(void);
    LValue buildLoadArgIndex(int index);
    LValue buildStoreArgIndex(LValue val, int index);
    LValue buildArgBytePointer();
    LValue buildSelect(LValue condition, LValue taken, LValue notTaken);
    LValue buildICmp(LIntPredicate cond, LValue left, LValue right);
    LValue buildAtomicCmpXchg(LValue addr, LValue cmp, LValue val);

    inline LValue buildCall(LValue function, const LValue* args, unsigned numArgs)
    {
        return llvmAPI->BuildCall(m_builder, function, const_cast<LValue*>(args), numArgs, "");
    }

    template <typename VectorType>
    inline LValue buildCall(LValue function, const VectorType& vector)
    {
        return buildCall(function, vector.begin(), vector.size());
    }
    inline LValue buildCall(LValue function)
    {
        return buildCall(function, nullptr, 0U);
    }
    inline LValue buildCall(LValue function, LValue arg1)
    {
        return buildCall(function, &arg1, 1);
    }
    template <typename... Args>
    LValue buildCall(LValue function, LValue arg1, Args... args)
    {
        LValue argsArray[] = { arg1, args... };
        return buildCall(function, argsArray, sizeof(argsArray) / sizeof(LValue));
    }

    LValue buildCast(LLVMOpcode Op, LLVMValueRef Val, LLVMTypeRef DestTy);
    LValue buildPointerCast(LLVMValueRef Val, LLVMTypeRef DestTy);
    LValue buildPhi(LType type);

    void buildDirectPatch(uintptr_t where);
    void buildDirectPatch(LValue where);
    void buildDirectSlowPatch(uintptr_t where);
    void buildDirectSlowPatch(LValue where);
    void buildIndirectPatch(LValue where);
    void buildAssistPatch(LValue where);

    inline IntrinsicRepository& repo() { return m_repo; }
    inline LType argType() const { return m_argType; }
    inline LBasicBlock prologue() const { return m_prologue; }
    inline LBasicBlock current() const { return m_current; }
    inline LValue arg() const { return m_arg; }
    LType typeOf(LValue val) __attribute__((pure));

private:
    void buildGetArg();
    void buildPatchCommon(LValue where, const struct PatchDesc& desc, size_t patchSize);

    CompilerState& m_state;
    IntrinsicRepository m_repo;
    LBuilder m_builder;
    LType m_argType;
    LBasicBlock m_prologue;
    LBasicBlock m_current;
    LValue m_arg;
    uint32_t m_stackMapsId;
};
}
#endif /* OUTPUT_H */
