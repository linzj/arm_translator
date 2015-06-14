#include <unordered_map>
#include <memory>
#include "InitializeLLVM.h"
#include "CompilerState.h"
#include "Compile.h"
#include "Link.h"
#include "VexTranslator.h"
#include "Registers.h"
#include "Output.h"
#include "log.h"

namespace jit {

VexTranslator::VexTranslator()
    : m_code(nullptr)
    , m_codeSize(0)
{
}

VexTranslator::~VexTranslator()
{
}

bool VexTranslator::init()
{
    initLLVM();
    return true;
}
}

namespace {
using namespace jit;
static inline UChar clearWBit(UChar rex)
{
    return toUChar(rex & ~(1 << 3));
}

inline static uint8_t rexAMode_R__wrk(unsigned gregEnc3210, unsigned eregEnc3210)
{
    uint8_t W = 1; /* we want 64-bit mode */
    uint8_t R = (gregEnc3210 >> 3) & 1;
    uint8_t X = 0; /* not relevant */
    uint8_t B = (eregEnc3210 >> 3) & 1;
    return 0x40 + ((W << 3) | (R << 2) | (X << 1) | (B << 0));
}

static inline unsigned iregEnc3210(unsigned in)
{
    return in;
}

static uint8_t rexAMode_R(unsigned greg, unsigned ereg)
{
    return rexAMode_R__wrk(iregEnc3210(greg), iregEnc3210(ereg));
}

inline static uint8_t mkModRegRM(unsigned mod, unsigned reg, unsigned regmem)
{
    return (uint8_t)(((mod & 3) << 6) | ((reg & 7) << 3) | (regmem & 7));
}

inline static uint8_t* doAMode_R__wrk(uint8_t* p, unsigned gregEnc3210, unsigned eregEnc3210)
{
    *p++ = mkModRegRM(3, gregEnc3210 & 7, eregEnc3210 & 7);
    return p;
}

static uint8_t* doAMode_R(uint8_t* p, unsigned greg, unsigned ereg)
{
    return doAMode_R__wrk(p, iregEnc3210(greg), iregEnc3210(ereg));
}

static uint8_t* emit64(uint8_t* p, uint64_t w64)
{
    *reinterpret_cast<uint64_t*>(p) = w64;
    return p + sizeof(w64);
}

typedef jit::CompilerState State;

class VexTranslatorImpl : public jit::VexTranslator {
public:
    VexTranslatorImpl();

private:
    virtual bool translate(IRSB*, const VexTranslatorEnv& env) override;
    bool translateStmt(IRStmt* stmt);
    bool translateNext();

    jit::LValue genGuestArrayOffset(IRRegArray* descr,
        IRExpr* off, Int bias);
    bool translatePut(IRStmt* stmt);
    bool translatePutI(IRStmt* stmt);
    bool translateWrTmp(IRStmt* stmt);
    bool translateExit(IRStmt* stmt);
    bool translateStore(IRStmt* stmt);
    bool translateStoreG(IRStmt* stmt);
    bool translateLoadG(IRStmt* stmt);
    bool translateCAS(IRStmt* stmt);

    jit::LValue translateExpr(IRExpr* expr);
    jit::LValue translateRdTmp(IRExpr* expr);
    jit::LValue translateConst(IRExpr* expr);
    jit::LValue translateGet(IRExpr* expr);
    jit::LValue translateLoad(IRExpr* expr);
    jit::LValue translateBinop(IRExpr* expr);
    inline void _ensureType(jit::LValue val, IRType type) __attribute__((pure));
#define ensureType(val, type) \
    _ensureType(val, type)

    LValue castToPointer(IRType type, LValue p);
    LValue two32to64(LValue low, LValue hi);
    void _64totwo32(LValue val64, LValue& low, LValue& hi);

    static void patchProloge(void*, uint8_t* start);
    static void patchDirect(void*, uint8_t* p, void*);
    static void patchIndirect(void*, uint8_t* p, void*);
    static void patchAssist(void*, uint8_t* p, void* entry);
    typedef std::unordered_map<IRTemp, jit::LValue> TmpValMap;
    jit::Output* m_output;
    IRSB* m_bb;
    const VexTranslatorEnv* m_env;
    TmpValMap m_tmpValMap;
    jit::ByteBuffer m_code;
    std::unique_ptr<State> m_state;
    bool m_chainingAllow;
};

void VexTranslatorImpl::patchProloge(void*, uint8_t* start)
{
    uint8_t* p = start;
    static const uint8_t evCheck[] = {
        0xff,
        0x4d,
        0x08,
        0x79,
        0x03,
        0xff,
        0x65,
        0x00,
    };
    // 4:	ff 4d 08             	decl   0x8(%rbp)
    // 7:	79 03                	jns    c <nofail>
    // 9:	ff 65 00             	jmpq   *0x0(%rbp)
    memcpy(p, evCheck, sizeof(evCheck));
    p += sizeof(evCheck);
    *p++ = rexAMode_R(jit::RBP,
        jit::RDI);
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RDI);
}

void VexTranslatorImpl::patchDirect(void*, uint8_t* p, void* entry)
{
    // epilogue

    // 3 bytes
    *p++ = rexAMode_R(jit::RBP,
        jit::RDI);
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RSP);
    // 1 bytes pop rbp
    *p++ = 0x5d;

    /* 10 bytes: movabsq $target, %r11 */
    *p++ = 0x49;
    *p++ = 0xBB;
    p = emit64(p, reinterpret_cast<uintptr_t>(entry));
    /* movq %r11, RIP(%rbp) */

    /* 3 bytes: call*%r11 */
    *p++ = 0x41;
    *p++ = 0xFF;
    *p++ = 0xD3;
}

void VexTranslatorImpl::patchIndirect(void*, uint8_t* p, void* entry)
{
    // epilogue
    // 3 bytes
    *p++ = rexAMode_R(jit::RBP,
        jit::RDI);
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RSP);
    // 1 bytes pop rbp
    *p++ = 0x5d;

    /* 10 bytes: movabsq $target, %r11 */
    *p++ = 0x49;
    *p++ = 0xBB;
    p = emit64(p, reinterpret_cast<uintptr_t>(entry));
    /* movq %r11, RIP(%rbp) */

    /* 3 bytes: jmp *%r11 */
    *p++ = 0x41;
    *p++ = 0xFF;
    *p++ = 0xE3;
}

void VexTranslatorImpl::patchAssist(void*, uint8_t* p, void* entry)
{
    // epilogue

    // 3 bytes
    *p++ = rexAMode_R(jit::RBP,
        jit::RDI);
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RSP);
    // 1 bytes pop rbp
    *p++ = 0x5d;

    /* 10 bytes: movabsq $target, %r11 */
    *p++ = 0x49;
    *p++ = 0xBB;
    p = emit64(p, reinterpret_cast<uintptr_t>(entry));
    /* movq %r11, RIP(%rbp) */

    /* 3 bytes: jmp *%r11 */
    *p++ = 0x41;
    *p++ = 0xFF;
    *p++ = 0xE3;
}

VexTranslatorImpl::VexTranslatorImpl()
    : m_output(nullptr)
    , m_bb(nullptr)
    , m_env(nullptr)
    , m_chainingAllow(false)
{
}

bool VexTranslatorImpl::translate(IRSB* bb, const VexTranslatorEnv& env)
{
    PlatformDesc desc = {
        env.m_contextSize,
        static_cast<size_t>(bb->offsIP), /* offset of pc */
        11, /* prologue size */
        17, /* direct size */
        17, /* indirect size */
        17, /* assist size */
    };
    m_bb = bb;

    m_state.reset(new State("llvm", desc));
    State& state = *m_state;
    Output output(state);
    m_output = &output;
    m_env = &env;
    if (env.m_dispDirect && env.m_dispDirectSlow && env.m_dispIndirect) {
        m_chainingAllow = true;
    }

    for (int i = 0; i < bb->stmts_used; i++)
        if (bb->stmts[i]) {
            bool r = translateStmt(bb->stmts[i]);
            if (!r) {
                return false;
            }
        }
    if (!translateNext()) {
        return false;
    }

    dumpModule(state.m_module);
    compile(state);
    void (*m_patchPrologue)(void* opaque, uint8_t* start);
    void (*m_patchDirect)(void* opaque, uint8_t* toFill, void*);
    void (*m_patchIndirect)(void* opaque, uint8_t* toFill, void*);
    void (*m_patchAssist)(void* opaque, uint8_t* toFill, void*);
    LinkDesc linkDesc = {
        this,
        env.m_dispDirect,
        env.m_dispDirectSlow,
        env.m_dispIndirect,
        env.m_dispAssist,
        patchProloge,
        patchDirect,
        patchIndirect,
        patchAssist
    };
    link(state, linkDesc);

    m_bb = nullptr;
    m_output = nullptr;
    m_env = nullptr;
    setCode(state.m_codeSectionList.front().data());
    setCodeSize(state.m_codeSectionList.front().size());
    return true;
}

bool VexTranslatorImpl::translateStmt(IRStmt* stmt)
{
    switch (stmt->tag) {
    case Ist_Put: {
        return translatePut(stmt);
    } break;
    case Ist_PutI: {
        return translatePutI(stmt);
    } break;
    case Ist_WrTmp: {
        return translateWrTmp(stmt);
    } break;
    case Ist_Exit: {
        return translateExit(stmt);
    }
    case Ist_Store: {
        return translateStore(stmt);
    }
    case Ist_StoreG: {
        return translateStoreG(stmt);
    }
    case Ist_LoadG: {
        return translateLoadG(stmt);
    }
    case Ist_CAS: {
        return translateCAS(stmt);
    }
    default:
        EMASSERT("not yet implement" && false);
        return false;
    }
    return false;
}

jit::LValue VexTranslatorImpl::genGuestArrayOffset(IRRegArray* descr,
    IRExpr* off, Int bias)
{
    LValue roff = translateExpr(off);
    LValue tmp = roff;
    Int elemSz = sizeofIRType(descr->elemTy);
    if (bias != 0) {
        /* Make sure the bias is sane, in the sense that there are
           no significant bits above bit 30 in it. */
        EMASSERT(-10000 < bias && bias < 10000);
        tmp = m_output->buildAdd(m_output->constInt32(bias), tmp);
    }
    tmp = m_output->buildAnd(m_output->constInt32(7), tmp);
    EMASSERT(elemSz == 1 || elemSz == 8);
    tmp = m_output->buildShl(tmp, m_output->constInt32(elemSz == 8 ? 3 : 0));
    tmp = m_output->buildAdd(tmp, m_output->constInt32(descr->base));
    return tmp;
}

bool VexTranslatorImpl::translatePut(IRStmt* stmt)
{
    LValue expr = translateExpr(stmt->Ist.Put.data);
    m_output->buildStoreArgIndex(expr, stmt->Ist.Put.offset / sizeof(intptr_t));
    return true;
}

bool VexTranslatorImpl::translatePutI(IRStmt* stmt)
{
    IRPutI* puti = stmt->Ist.PutI.details;
    LValue offset = genGuestArrayOffset(puti->descr,
        puti->ix, puti->bias);
    IRType ty = typeOfIRExpr(m_bb->tyenv, puti->data);
    LValue val = translateExpr(puti->data);
    LValue pointer = m_output->buildArgBytePointer();
    LValue afterOffset = m_output->buildAdd(pointer, offset);
    ensureType(val, ty);
    switch (ty) {
    case Ity_F64: {
        LValue casted = m_output->buildCast(LLVMBitCast, afterOffset, m_output->repo().refDouble);
        m_output->buildStore(val, casted);
    } break;
    case Ity_I8: {
        LValue casted = m_output->buildCast(LLVMBitCast, afterOffset, m_output->repo().ref8);
        m_output->buildStore(val, casted);
    } break;
    case Ity_I64: {
        LValue casted = m_output->buildCast(LLVMBitCast, afterOffset, m_output->repo().ref64);
        m_output->buildStore(val, casted);
    } break;
    default:
        EMASSERT("not implement type yet" && false);
        break;
    }
    return true;
}

bool VexTranslatorImpl::translateWrTmp(IRStmt* stmt)
{
    IRTemp tmp = stmt->Ist.WrTmp.tmp;
    LValue val = translateExpr(stmt->Ist.WrTmp.data);
    ensureType(val, typeOfIRExpr(m_bb->tyenv, stmt->Ist.WrTmp.data));
    ensureType(val, typeOfIRTemp(m_bb->tyenv, tmp));
    m_tmpValMap[tmp] = val;
    return true;
}

bool VexTranslatorImpl::translateExit(IRStmt* stmt)
{
    LValue guard = translateExpr(stmt->Ist.Exit.guard);
    LBasicBlock bbt = m_output->appendBasicBlock("taken");
    LBasicBlock bbnt = m_output->appendBasicBlock("not_taken");
    m_output->buildCondBr(guard, bbt, bbnt);
    m_output->positionToBBEnd(bbt);
    IRConst* cdst = stmt->Ist.Exit.dst;
    LValue val = m_output->constIntPtr(static_cast<uint64_t>(cdst->Ico.U64));

    if (stmt->Ist.Exit.jk == Ijk_Boring) {
        if (m_chainingAllow) {
            bool toFastEP
                = ((Addr64)cdst->Ico.U64) > m_env->m_maxga;
            if (toFastEP)
                m_output->buildDirectPatch(val);
            else
                m_output->buildDirectSlowPatch(val);
        }
        else
            m_output->buildAssistPatch(val);
        goto end;
    }

    /* Case: assisted transfer to arbitrary address */
    switch (stmt->Ist.Exit.jk) {
    /* Keep this list in sync with that in iselNext below */
    case Ijk_ClientReq:
    case Ijk_EmWarn:
    case Ijk_NoDecode:
    case Ijk_NoRedir:
    case Ijk_SigSEGV:
    case Ijk_SigTRAP:
    case Ijk_Sys_syscall:
    case Ijk_InvalICache:
    case Ijk_Yield: {
        m_output->buildAssistPatch(val);
        goto end;
    }
    default:
        break;
    }
    EMASSERT("unsurpported IREXIT" && false);
end:
    m_output->positionToBBEnd(bbnt);
    return true;
}

bool VexTranslatorImpl::translateStore(IRStmt* stmt)
{
    auto&& store = stmt->Ist.Store;
    EMASSERT(store.end == Iend_LE);
    LValue addr = translateExpr(store.addr);
    LValue data = translateExpr(store.data);
    LValue castAddr = m_output->buildCast(LLVMBitCast, addr, jit::pointerType(jit::typeOf(data)));
    m_output->buildStore(data, castAddr);
    return true;
}

bool VexTranslatorImpl::translateStoreG(IRStmt* stmt)
{
    IRStoreG* details = stmt->Ist.StoreG.details;
    EMASSERT(details->end == Iend_LE);
    LValue addr = translateExpr(details->addr);
    LValue data = translateExpr(details->data);
    LValue castAddr = m_output->buildCast(LLVMBitCast, addr, jit::pointerType(jit::typeOf(data)));
    LValue guard = translateExpr(details->guard);
    LBasicBlock bbt = m_output->appendBasicBlock("taken");
    LBasicBlock bbnt = m_output->appendBasicBlock("not_taken");
    m_output->buildCondBr(guard, bbt, bbnt);
    m_output->positionToBBEnd(bbt);
    m_output->buildStore(data, castAddr);
    m_output->buildBr(bbnt);
    m_output->positionToBBEnd(bbnt);
    return true;
}

bool VexTranslatorImpl::translateLoadG(IRStmt* stmt)
{
    IRLoadG* details = stmt->Ist.LoadG.details;
    EMASSERT(details->end == Iend_LE);
    LValue addr = translateExpr(details->addr);
    LValue alt = translateExpr(details->alt);
    LValue guard = translateExpr(details->guard);
    LBasicBlock bbt = m_output->appendBasicBlock("taken");
    LBasicBlock bbnt = m_output->appendBasicBlock("not_taken");
    m_output->buildCondBr(guard, bbt, bbnt);
    LBasicBlock original = m_output->current();
    m_output->positionToBBEnd(bbt);
    LType pointerType;
    switch (details->cvt) {
    case ILGop_Ident32:
        pointerType = m_output->repo().ref32;
        break;
    case ILGop_16Sto32:
    case ILGop_16Uto32:
        pointerType = m_output->repo().ref16;
        break;
    case ILGop_8Uto32:
    case ILGop_8Sto32:
        pointerType = m_output->repo().ref8;
        break;
    }
    LValue dataBeforCast = m_output->buildLoad(m_output->buildCast(LLVMBitCast, addr, pointerType));
    LValue dataAfterCast;
    // do casting
    switch (details->cvt) {
    case ILGop_Ident32:
        EMASSERT(typeOf(dataBeforCast) == m_output->repo().int32);
        dataAfterCast = dataBeforCast;
        break;
    case ILGop_16Sto32:
    case ILGop_16Uto32:
        EMASSERT(typeOf(dataBeforCast) == m_output->repo().int16);
        dataAfterCast = m_output->buildCast(details->cvt == ILGop_16Uto32 ? LLVMZExt : LLVMSExt, dataBeforCast, m_output->repo().int32);
        break;
    case ILGop_8Uto32:
    case ILGop_8Sto32:
        EMASSERT(typeOf(dataBeforCast) == m_output->repo().int8);
        dataAfterCast = m_output->buildCast(details->cvt == ILGop_8Uto32 ? LLVMZExt : LLVMSExt, dataBeforCast, m_output->repo().int32);
        break;
    }
    m_output->buildBr(bbnt);
    m_output->positionToBBEnd(bbnt);
    LValue phi = m_output->buildPhi(m_output->repo().int32);
    jit::addIncoming(phi, &dataAfterCast, &bbt, 1);
    jit::addIncoming(phi, &alt, &original, 1);
    m_tmpValMap[details->dst] = phi;
    return true;
}

bool VexTranslatorImpl::translateCAS(IRStmt* stmt)
{
    IRCAS* cas = stmt->Ist.CAS.details;
    LValue addr = translateExpr(cas->addr);
    LValue expectedData, newData;
    if (!cas->expdHi) {
        // non 64 bit
        EMASSERT(cas->dataHi == nullptr);
        addr = m_output->buildCast(LLVMBitCast, addr, m_output->repo().ref32);
        expectedData = translateExpr(cas->expdLo);
        newData = translateExpr(cas->dataLo);
    }
    else {
        EMASSERT(cas->dataHi != nullptr);
        addr = m_output->buildCast(LLVMBitCast, addr, m_output->repo().ref64);
        expectedData = translateExpr(cas->expdLo);
        LValue expectedDataHi = translateExpr(cas->expdHi);
        expectedData = two32to64(expectedData, expectedDataHi);

        newData = translateExpr(cas->dataLo);
        LValue newDataHi = translateExpr(cas->dataHi);
        newData = two32to64(newData, newDataHi);
    }
    LValue v = m_output->buildAtomicCmpXchg(addr, expectedData, newData);
    if (!cas->expdHi) {
        // non 64 bit
        EMASSERT(cas->oldHi == IRTemp_INVALID);
        m_tmpValMap[cas->oldLo] = v;
    }
    else {
        EMASSERT(cas->oldHi == IRTemp_INVALID);
        LValue hi, lo;
        _64totwo32(v, lo, hi);
        m_tmpValMap[cas->oldLo] = lo;
        m_tmpValMap[cas->oldHi] = hi;
    }
    return true;
}

void VexTranslatorImpl::_ensureType(jit::LValue val, IRType type)
{
    switch (type) {
    case Ity_I1:
        EMASSERT(m_output->typeOf(val) == m_output->repo().int1);
    case Ity_I8:
        EMASSERT(m_output->typeOf(val) == m_output->repo().int8);
    case Ity_I16:
        EMASSERT(m_output->typeOf(val) == m_output->repo().int16);
    case Ity_I32:
        EMASSERT(m_output->typeOf(val) == m_output->repo().int32);
    case Ity_I64:
        EMASSERT(m_output->typeOf(val) == m_output->repo().int64);
    case Ity_I128:
        EMASSERT(m_output->typeOf(val) == m_output->repo().int128);
    case Ity_D32:
    case Ity_F32:
        EMASSERT(m_output->typeOf(val) == m_output->repo().floatType);
    case Ity_D64:
    case Ity_F64:
        EMASSERT(m_output->typeOf(val) == m_output->repo().doubleType);
    case Ity_D128:
    case Ity_F128:
        EMASSERT("AMD64 not support F128 D128." && false);
    case Ity_V128:
        EMASSERT(m_output->typeOf(val) == m_output->repo().v128Type);
    case Ity_V256:
        EMASSERT(m_output->typeOf(val) == m_output->repo().v256Type);
    default:
        EMASSERT("unknow type" && false);
    }
}

bool VexTranslatorImpl::translateNext()
{
    IRExpr* next = m_bb->next;
    IRJumpKind jk = m_bb->jumpkind;
    /* Case: boring transfer to known address */
    LValue val = translateExpr(next);
    if (next->tag == Iex_Const) {
        IRConst* cdst = next->Iex.Const.con;
        if (m_chainingAllow) {

            bool toFastEP
                = ((Addr64)cdst->Ico.U64) > m_env->m_maxga;
            if (toFastEP)
                m_output->buildDirectPatch(val);
            else
                m_output->buildDirectSlowPatch(val);
        }
        else
            m_output->buildAssistPatch(val);
        return true;
    }

    /* Case: call/return (==boring) transfer to any address */
    switch (jk) {
    case Ijk_Boring:
    case Ijk_Ret:
    case Ijk_Call: {
        if (m_chainingAllow) {
            m_output->buildIndirectPatch(val);
        }
        else {
            m_output->buildAssistPatch(val);
        }
        return true;
    }
    default:
        break;
    }

    /* Case: assisted transfer to arbitrary address */
    switch (jk) {
    /* Keep this list in sync with that for Ist_Exit above */
    case Ijk_ClientReq:
    case Ijk_EmWarn:
    case Ijk_NoDecode:
    case Ijk_NoRedir:
    case Ijk_SigSEGV:
    case Ijk_SigTRAP:
    case Ijk_Sys_syscall:
    case Ijk_InvalICache:
    case Ijk_Yield: {
        m_output->buildAssistPatch(val);
        return true;
    }
    default:
        break;
    }

    vex_printf("\n-- PUT(%d) = ", m_bb->offsIP);
    ppIRExpr(next);
    vex_printf("; exit-");
    ppIRJumpKind(jk);
    vex_printf("\n");
    vassert(0); // are we expecting any other kind?
}

jit::LValue VexTranslatorImpl::translateExpr(IRExpr* expr)
{
    switch (expr->tag) {
    case Iex_RdTmp: {
        return translateRdTmp(expr);
    }
    case Iex_Const: {
        return translateConst(expr);
    }
    case Iex_Get: {
        return translateGet(expr);
    }
    case Iex_Load: {
        return translateLoad(expr);
    }
    case Iex_Binop: {
        return translateBinop(expr);
    }
    }
    EMASSERT("not supported expr" && false);
}

jit::LValue VexTranslatorImpl::translateRdTmp(IRExpr* expr)
{
    auto found = m_tmpValMap.find(expr->Iex.RdTmp.tmp);
    EMASSERT(found != m_tmpValMap.end());
    return found->second;
}

jit::LValue VexTranslatorImpl::translateConst(IRExpr* expr)
{
    IRConst* myconst = expr->Iex.Const.con;
    switch (myconst->tag) {
    case Ico_U1:
        return m_output->constInt1(myconst->Ico.U1);
    case Ico_U8:
        return m_output->constInt8(myconst->Ico.U8);
    case Ico_U16:
        return m_output->constInt16(myconst->Ico.U16);
    case Ico_U32:
        return m_output->constInt32(myconst->Ico.U32);
    case Ico_U64:
        return m_output->constInt64(myconst->Ico.U64);
    case Ico_F32:
        return m_output->constFloat(myconst->Ico.F32);
    case Ico_F64:
        return m_output->constFloat(myconst->Ico.F64);
    case Ico_F32i: {
        jit::LValue val = m_output->constInt32(myconst->Ico.F32i);
        return m_output->buildCast(LLVMBitCast, val, m_output->repo().floatType);
    }
    case Ico_F64i: {
        jit::LValue val = m_output->constInt64(myconst->Ico.F64i);
        return m_output->buildCast(LLVMBitCast, val, m_output->repo().doubleType);
    }
    case Ico_V128:
        return m_output->constV128(myconst->Ico.V128);

    case Ico_V256:
        return m_output->constV256(myconst->Ico.V256);
    }
    EMASSERT("not supported constant" && false);
}

jit::LValue VexTranslatorImpl::translateGet(IRExpr* expr)
{
    EMASSERT(expr->Iex.Get.ty == Ity_I64);
    LValue beforeCast = m_output->buildLoadArgIndex(expr->Iex.Get.offset / sizeof(intptr_t));
    return beforeCast;
}

jit::LValue VexTranslatorImpl::translateLoad(IRExpr* expr)
{
    auto&& load = expr->Iex.Load;
    LValue addr = translateExpr(load.addr);
    LValue pointer = castToPointer(load.ty, addr);
    return m_output->buildLoad(pointer);
}

jit::LValue VexTranslatorImpl::translateBinop(IRExpr* expr)
{
    auto&& binop = expr->Iex.Binop;
    LValue arg1 = translateExpr(binop.arg1);
    LValue arg2 = translateExpr(binop.arg2);
    switch (op) {
      Iop_Add8,  Iop_Add16,  Iop_Add32,  Iop_Add64,
      Iop_Sub8,  Iop_Sub16,  Iop_Sub32,  Iop_Sub64,
      Iop_Mul8,  Iop_Mul16,  Iop_Mul32,  Iop_Mul64,
      Iop_Or8,   Iop_Or16,   Iop_Or32,   Iop_Or64,
      Iop_And8,  Iop_And16,  Iop_And32,  Iop_And64,
      Iop_Xor8,  Iop_Xor16,  Iop_Xor32,  Iop_Xor64,
      Iop_Shl8,  Iop_Shl16,  Iop_Shl32,  Iop_Shl64,
      Iop_Shr8,  Iop_Shr16,  Iop_Shr32,  Iop_Shr64,
      Iop_Sar8,  Iop_Sar16,  Iop_Sar32,  Iop_Sar64,
      Iop_CmpEQ8,  Iop_CmpEQ16,  Iop_CmpEQ32,  Iop_CmpEQ64,
      Iop_CmpNE8,  Iop_CmpNE16,  Iop_CmpNE32,  Iop_CmpNE64,
      Iop_CasCmpEQ8, Iop_CasCmpEQ16, Iop_CasCmpEQ32, Iop_CasCmpEQ64,
      Iop_CasCmpNE8, Iop_CasCmpNE16, Iop_CasCmpNE32, Iop_CasCmpNE64,
      Iop_ExpCmpNE8, Iop_ExpCmpNE16, Iop_ExpCmpNE32, Iop_ExpCmpNE64,
      Iop_MullS8, Iop_MullS16, Iop_MullS32, Iop_MullS64,
      Iop_MullU8, Iop_MullU16, Iop_MullU32, Iop_MullU64,
      Iop_CmpLT32S, Iop_CmpLT64S,
      Iop_CmpLE32S, Iop_CmpLE64S,
      Iop_CmpLT32U, Iop_CmpLT64U,
      Iop_CmpLE32U, Iop_CmpLE64U,
      Iop_CmpNEZ8, Iop_CmpNEZ16,  Iop_CmpNEZ32,  Iop_CmpNEZ64,
      Iop_CmpwNEZ32, Iop_CmpwNEZ64, 
      Iop_Left8, Iop_Left16, Iop_Left32, Iop_Left64, 
      Iop_Max32U, 
      Iop_CmpORD32U, Iop_CmpORD64U,
      Iop_CmpORD32S, Iop_CmpORD64S,
      Iop_DivU32,
      Iop_DivS32,
      Iop_DivU64,
      Iop_DivS64,
      Iop_DivU64E,
                 
      Iop_DivS64E,
      Iop_DivU32E,
                  
      Iop_DivS32E, 

      Iop_DivModU64to32,
                       
      Iop_DivModS64to32,

      Iop_DivModU128to64,
                        
      Iop_DivModS128to64,

      Iop_DivModS64to64,
      Iop_SqrtF64,

      Iop_SqrtF32,
      Iop_CmpF64,
      Iop_CmpF32,
      Iop_CmpF128,
      Iop_F64toI16S,
      Iop_F64toI32S,
      Iop_F64toI64S,
      Iop_F64toI64U,

      Iop_F64toI32U,

      Iop_I32StoF64,
      Iop_I64StoF64,
      Iop_I64UtoF64,
      Iop_I64UtoF32,

      Iop_I32UtoF32,
      Iop_I32UtoF64,

      Iop_F32toI32S,
      Iop_F32toI64S,
      Iop_F32toI32U,
      Iop_F32toI64U,

      Iop_I32StoF32,
      Iop_I64StoF32,

      Iop_F32toF64,
      Iop_F64toF32,
                       
      Iop_F64HLtoF128,
      Iop_F128HItoF64,
      Iop_F128LOtoF64,
Iop_SqrtF128,
      Iop_F128toI32S,
      Iop_F128toI64S,
      Iop_F128toI32U,
      Iop_F128toI64U,
      Iop_F128toF64,
      Iop_F128toF32,
      Iop_AtanF64,
      Iop_Yl2xF64,
      Iop_Yl2xp1F64,
      Iop_PRemF64,
      Iop_PRemC3210F64,
      Iop_PRem1F64,
      Iop_PRem1C3210F64,
      Iop_ScaleF64,
      Iop_SinF64,
      Iop_CosF64,
      Iop_TanF64,
      Iop_2xm1F64,
      Iop_RoundF64toInt,
      Iop_RoundF32toInt,
      Iop_RoundF64toF32,
      Iop_QAdd32S,
      Iop_QSub32S,
      Iop_Add16x2, Iop_Sub16x2,
      Iop_QAdd16Sx2, Iop_QAdd16Ux2,
      Iop_QSub16Sx2, Iop_QSub16Ux2,
      Iop_HAdd16Ux2, Iop_HAdd16Sx2,
      Iop_HSub16Ux2, Iop_HSub16Sx2,
      Iop_Add8x4, Iop_Sub8x4,
      Iop_QAdd8Sx4, Iop_QAdd8Ux4,
      Iop_QSub8Sx4, Iop_QSub8Ux4,
      Iop_HAdd8Ux4, Iop_HAdd8Sx4,
      Iop_HSub8Ux4, Iop_HSub8Sx4,
      Iop_Sad8Ux4,
      Iop_F32ToFixed32Ux2_RZ,
      Iop_Fixed32UToF32x2_RN,
      Iop_Max32Fx2,      Iop_Min32Fx2,
      Iop_PwMax32Fx2,    Iop_PwMin32Fx2,
      Iop_CmpEQ32Fx2, Iop_CmpGT32Fx2, Iop_CmpGE32Fx2,
      Iop_RecipStep32Fx2,
      Iop_RSqrtStep32Fx2,
      Iop_Add8x8,   Iop_Add16x4,   Iop_Add32x2,
      Iop_QAdd8Ux8, Iop_QAdd16Ux4, Iop_QAdd32Ux2, Iop_QAdd64Ux1,
      Iop_QAdd8Sx8, Iop_QAdd16Sx4, Iop_QAdd32Sx2, Iop_QAdd64Sx1,
      Iop_PwAdd8x8,  Iop_PwAdd16x4,  Iop_PwAdd32x2,
      Iop_PwMax8Sx8, Iop_PwMax16Sx4, Iop_PwMax32Sx2,
      Iop_PwMax8Ux8, Iop_PwMax16Ux4, Iop_PwMax32Ux2,
      Iop_PwMin8Sx8, Iop_PwMin16Sx4, Iop_PwMin32Sx2,
      Iop_PwMin8Ux8, Iop_PwMin16Ux4, Iop_PwMin32Ux2,
      Iop_Sub8x8,   Iop_Sub16x4,   Iop_Sub32x2,
      Iop_QSub8Ux8, Iop_QSub16Ux4, Iop_QSub32Ux2, Iop_QSub64Ux1,
      Iop_QSub8Sx8, Iop_QSub16Sx4, Iop_QSub32Sx2, Iop_QSub64Sx1,
      Iop_Mul8x8, Iop_Mul16x4, Iop_Mul32x2,
      Iop_Mul32Fx2,
      Iop_MulHi16Ux4,
      Iop_MulHi16Sx4,
      Iop_PolynomialMul8x8,
      Iop_QDMulHi16Sx4, Iop_QDMulHi32Sx2,
      Iop_QRDMulHi16Sx4, Iop_QRDMulHi32Sx2,
      Iop_Avg8Ux8,
      Iop_Avg16Ux4,
      Iop_Max8Sx8, Iop_Max16Sx4, Iop_Max32Sx2,
      Iop_Max8Ux8, Iop_Max16Ux4, Iop_Max32Ux2,
      Iop_Min8Sx8, Iop_Min16Sx4, Iop_Min32Sx2,
      Iop_Min8Ux8, Iop_Min16Ux4, Iop_Min32Ux2,
      Iop_CmpEQ8x8,  Iop_CmpEQ16x4,  Iop_CmpEQ32x2,
      Iop_CmpGT8Ux8, Iop_CmpGT16Ux4, Iop_CmpGT32Ux2,
      Iop_CmpGT8Sx8, Iop_CmpGT16Sx4, Iop_CmpGT32Sx2,
      Iop_Shl8x8, Iop_Shl16x4, Iop_Shl32x2,
      Iop_Shr8x8, Iop_Shr16x4, Iop_Shr32x2,
      Iop_Sar8x8, Iop_Sar16x4, Iop_Sar32x2,
      Iop_Sal8x8, Iop_Sal16x4, Iop_Sal32x2, Iop_Sal64x1,
      Iop_ShlN8x8, Iop_ShlN16x4, Iop_ShlN32x2,
      Iop_ShrN8x8, Iop_ShrN16x4, Iop_ShrN32x2,
      Iop_SarN8x8, Iop_SarN16x4, Iop_SarN32x2,
      Iop_QShl8x8, Iop_QShl16x4, Iop_QShl32x2, Iop_QShl64x1,
      Iop_QSal8x8, Iop_QSal16x4, Iop_QSal32x2, Iop_QSal64x1,
      Iop_QShlNsatSU8x8,  Iop_QShlNsatSU16x4,
      Iop_QShlNsatSU32x2, Iop_QShlNsatSU64x1,
      Iop_QShlNsatUU8x8,  Iop_QShlNsatUU16x4,
      Iop_QShlNsatUU32x2, Iop_QShlNsatUU64x1,
      Iop_QShlNsatSS8x8,  Iop_QShlNsatSS16x4,
      Iop_QShlNsatSS32x2, Iop_QShlNsatSS64x1,
      Iop_QNarrowBin16Sto8Ux8,
      Iop_QNarrowBin16Sto8Sx8, Iop_QNarrowBin32Sto16Sx4,
      Iop_NarrowBin16to8x8,    Iop_NarrowBin32to16x4,
      Iop_InterleaveHI8x8, Iop_InterleaveHI16x4, Iop_InterleaveHI32x2,
      Iop_InterleaveLO8x8, Iop_InterleaveLO16x4, Iop_InterleaveLO32x2,
      Iop_InterleaveOddLanes8x8, Iop_InterleaveEvenLanes8x8,
      Iop_InterleaveOddLanes16x4, Iop_InterleaveEvenLanes16x4,
      Iop_CatOddLanes8x8, Iop_CatOddLanes16x4,
      Iop_CatEvenLanes8x8, Iop_CatEvenLanes16x4,
      Iop_GetElem8x8, Iop_GetElem16x4, Iop_GetElem32x2,
      Iop_Perm8x8,
      Iop_ShlD64, Iop_ShrD64,
      Iop_ShlD128, Iop_ShrD128,
      Iop_D64toD32,
      Iop_D128toD64,
      Iop_I64StoD64,
      Iop_I64UtoD64,
      Iop_D64toI32S,
      Iop_D64toI32U,
      Iop_D64toI64S,
      Iop_D64toI64U,
      Iop_D128toI32S,
      Iop_D128toI32U,
      Iop_D128toI64S,
      Iop_D128toI64U,
      Iop_F32toD32,
      Iop_F32toD64,
      Iop_F32toD128,
      Iop_F64toD32,

      Iop_F64toD64,

      Iop_F64toD128,

      Iop_F128toD32,

      Iop_F128toD64,

      Iop_F128toD128,

      Iop_D32toF32,

      Iop_D32toF64,

      Iop_D32toF128,

      Iop_D64toF32,

      Iop_D64toF64,

      Iop_D64toF128,

      Iop_D128toF32,

      Iop_D128toF64,

      Iop_D128toF128,
      Iop_InsertExpD64,

      Iop_InsertExpD128,
      Iop_D64HLtoD128, Iop_D128HItoD64, Iop_D128LOtoD64,
      Iop_Max32Fx4, Iop_Min32Fx4,
      Iop_Add32Fx2, Iop_Sub32Fx2,
      Iop_CmpEQ32Fx4, Iop_CmpLT32Fx4, Iop_CmpLE32Fx4, Iop_CmpUN32Fx4,
      Iop_CmpGT32Fx4, Iop_CmpGE32Fx4,
      Iop_PwMax32Fx4, Iop_PwMin32Fx4,
      Iop_RecipStep32Fx4,
      Iop_F32ToFixed32Ux4_RZ, Iop_F32ToFixed32Sx4_RZ,
      Iop_Fixed32UToF32x4_RN, Iop_Fixed32SToF32x4_RN,

    }
}

LValue VexTranslatorImpl::castToPointer(IRType irtype, LValue p)
{
    LType type;
    switch (irtype) {
    case Ity_I8:
        type = m_output->repo().ref8;
        break;
    case Ity_I16:
        type = m_output->repo().ref16;
        break;
    case Ity_I32:
        type = m_output->repo().ref32;
        break;
    case Ity_I64:
        type = m_output->repo().ref64;
        break;
    default:
        EMASSERT("unsupported type.");
        EMUNREACHABLE();
    }
    return m_output->buildCast(LLVMBitCast, p, type);
}

LValue VexTranslatorImpl::two32to64(LValue low, LValue hi)
{
    LValue low64 = m_output->buildCast(LLVMZExt, low, m_output->repo().int64);
    LValue hi64 = m_output->buildCast(LLVMZExt, hi, m_output->repo().int64);
    LValue hiShifted = m_output->buildShl(hi64, m_output->constInt32(32));
    return m_output->buildAnd(hiShifted, low64);
}

void VexTranslatorImpl::_64totwo32(LValue val64, LValue& low, LValue& hi)
{
    low = m_output->buildCast(LLVMTrunc, val64, m_output->repo().int32);
    hi = m_output->buildCast(LLVMTrunc, m_output->buildLShr(val64, m_output->constInt32(32)), m_output->repo().int32);
}
}

namespace jit {
VexTranslator* VexTranslator::create()
{
    return new VexTranslatorImpl;
}
}
