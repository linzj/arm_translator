#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <memory>
#include "VexHeaders.h"
#include "IRContextInternal.h"
#include "RegisterInit.h"
#include "RegisterAssign.h"
#include "Check.h"
#include "VexTranslator.h"
#include "log.h"

extern "C" {
void yyparse(IRContext*);
typedef void* yyscan_t;
int yylex_init(yyscan_t* scanner);

int yylex_init_extra(struct IRContext* user_defined, yyscan_t* scanner);

int yylex_destroy(yyscan_t yyscanner);
void vexSetAllocModeTEMP_and_clear(void);
extern AMD64Instr* AMD64Instr_EvCheck(AMD64AMode* amCounter,
    AMD64AMode* amFailAddr);
extern AMD64Instr* AMD64Instr_ProfInc(void);

extern void ppAMD64Instr(AMD64Instr*, Bool);

/* Some functions that insulate the register allocator from details
   of the underlying instruction set. */
extern void getRegUsage_AMD64Instr(HRegUsage*, AMD64Instr*, Bool);
extern void mapRegs_AMD64Instr(HRegRemap*, AMD64Instr*, Bool);
extern Bool isMove_AMD64Instr(AMD64Instr*, HReg*, HReg*);
extern Int emit_AMD64Instr(/*MB_MOD*/ Bool* is_profInc,
    UChar* buf, Int nbuf,
    AMD64Instr* i,
    Bool mode64,
    VexEndness endness_host,
    void* disp_cp_chain_me_to_slowEP,
    void* disp_cp_chain_me_to_fastEP,
    void* disp_cp_xindir,
    void* disp_cp_xassisted);

extern void genSpill_AMD64(/*OUT*/ HInstr** i1, /*OUT*/ HInstr** i2,
    HReg rreg, Int offset, Bool);
extern void genReload_AMD64(/*OUT*/ HInstr** i1, /*OUT*/ HInstr** i2,
    HReg rreg, Int offset, Bool);

extern void getAllocableRegs_AMD64(Int*, HReg**);
extern HInstrArray* iselSB_AMD64(IRSB*,
    VexArch,
    VexArchInfo*,
    VexAbiInfo*,
    Int offs_Host_EvC_Counter,
    Int offs_Host_EvC_FailAddr,
    Bool chainingAllowed,
    Bool addProfInc,
    Addr64 max_ga);

/* How big is an event check?  This is kind of a kludge because it
   depends on the offsets of host_EvC_FAILADDR and host_EvC_COUNTER,
   and so assumes that they are both <= 128, and so can use the short
   offset encoding.  This is all checked with assertions, so in the
   worst case we will merely assert at startup. */
extern Int evCheckSzB_AMD64(VexEndness endness_host);

/* Perform a chaining and unchaining of an XDirect jump. */
extern VexInvalRange chainXDirect_AMD64(VexEndness endness_host,
    void* place_to_chain,
    void* disp_cp_chain_me_EXPECTED,
    void* place_to_jump_to);

extern VexInvalRange unchainXDirect_AMD64(VexEndness endness_host,
    void* place_to_unchain,
    void* place_to_jump_to_EXPECTED,
    void* disp_cp_chain_me);

/* Patch the counter location into an existing ProfInc point. */
extern VexInvalRange patchProfInc_AMD64(VexEndness endness_host,
    void* place_to_patch,
    ULong* location_of_counter);
void vex_disp_run_translations(uintptr_t* two_words,
    void* guest_state,
    Addr64 host_addr);
void vex_disp_cp_chain_me_to_slowEP(void);
void vex_disp_cp_chain_me_to_fastEP(void);
void vex_disp_cp_xindir(void);
void vex_disp_cp_xassisted(void);
void vex_disp_cp_evcheck_fail(void);
}
static void failure_exit(void) __attribute__((noreturn));

static void log_bytes(HChar*, Int nbytes);

void failure_exit(void)
{
    LOGE("failure and exit.\n");
    exit(1);
}

void log_bytes(HChar* bytes, Int nbytes)
{
    static std::string s;
    s.append(bytes, nbytes);
    size_t found;
    while ((found = s.find('\n')) != std::string::npos) {
        LOGE("%s.\n", s.substr(0, found).c_str());
        s = s.substr(found + 1);
    }
}

static size_t genVex(IRSB* irsb, HChar* buffer, size_t len)
{
    VexArchInfo archinfo = { 0, VexEndnessLE };
    VexAbiInfo abiinfo = { 0 };
    Bool (*isMove)(HInstr*, HReg*, HReg*);
    void (*getRegUsage)(HRegUsage*, HInstr*, Bool);
    void (*mapRegs)(HRegRemap*, HInstr*, Bool);
    void (*genSpill)(HInstr**, HInstr**, HReg, Int, Bool);
    void (*genReload)(HInstr**, HInstr**, HReg, Int, Bool);
    HInstr* (*directReload)(HInstr*, HReg, Short);
    void (*ppInstr)(HInstr*, Bool);
    void (*ppReg)(HReg);
    HInstrArray* (*iselSB)(IRSB*, VexArch, VexArchInfo*, VexAbiInfo*,
        Int, Int, Bool, Bool, Addr64);
    Int (*emit)(/*MB_MOD*/ Bool*,
        UChar*, Int, HInstr*, Bool, VexEndness,
        void*, void*, void*, void*);
    IRExpr* (*specHelper)(const HChar*, IRExpr**, IRStmt**, Int);
    Bool (*preciseMemExnsFn)(Int, Int);
    Bool mode64 = True;
    HReg* available_real_regs;
    Int n_available_real_regs;
    IRType host_word_type, guest_word_type;
    Int guest_sizeB;
    VexGuestLayout* guest_layout;
    Int offB_CMSTART, offB_CMLEN, offB_GUEST_IP, szB_GUEST_IP;
    Int offB_HOST_EvC_COUNTER, offB_HOST_EvC_FAILADDR;
    HInstrArray* vcode;
    HInstrArray* rcode;

    // host amd64
    getAllocableRegs_AMD64(&n_available_real_regs,
        &available_real_regs);
    isMove = (Bool (*)(HInstr*, HReg*, HReg*))isMove_AMD64Instr;
    getRegUsage = (void (*)(HRegUsage*, HInstr*, Bool))
        getRegUsage_AMD64Instr;
    mapRegs = (void (*)(HRegRemap*, HInstr*, Bool))mapRegs_AMD64Instr;
    genSpill = (void (*)(HInstr**, HInstr**, HReg, Int, Bool))
        genSpill_AMD64;
    genReload = (void (*)(HInstr**, HInstr**, HReg, Int, Bool))
        genReload_AMD64;
    ppInstr = (void (*)(HInstr*, Bool))ppAMD64Instr;
    ppReg = (void (*)(HReg))ppHRegAMD64;
    iselSB = iselSB_AMD64;
    emit = (Int (*)(Bool*, UChar*, Int, HInstr*, Bool, VexEndness,
        void*, void*, void*, void*))
        emit_AMD64Instr;
    host_word_type = Ity_I64;
    // guest amd64
    preciseMemExnsFn = guest_amd64_state_requires_precise_mem_exns;
    specHelper = guest_amd64_spechelper;
    guest_sizeB = sizeof(VexGuestState);
    guest_word_type = Ity_I64;
    guest_layout = &amd64guest_layout;
    offB_CMSTART = offsetof(VexGuestState, guest_CMSTART);
    offB_CMLEN = offsetof(VexGuestState, guest_CMLEN);
    offB_GUEST_IP = offsetof(VexGuestState, guest_RIP);
    szB_GUEST_IP = sizeof(((VexGuestState*)0)->guest_RIP);
    offB_HOST_EvC_COUNTER = offsetof(VexGuestState, host_EvC_COUNTER);
    offB_HOST_EvC_FAILADDR = offsetof(VexGuestState, host_EvC_FAILADDR);

    vcode = iselSB(irsb, VexArchAMD64,
        &archinfo,
        &abiinfo,
        offB_HOST_EvC_COUNTER,
        offB_HOST_EvC_FAILADDR,
        True,
        False,
        0);
    rcode = doRegisterAllocation(vcode, available_real_regs,
        n_available_real_regs,
        isMove, getRegUsage, mapRegs,
        genSpill, genReload, directReload,
        guest_sizeB,
        ppInstr, ppReg, mode64);
    size_t out_used = 0; /* tracks along the host_bytes array */
    for (int i = 0; i < rcode->arr_used; i++) {
        HInstr* hi = rcode->arr[i];
        Bool hi_isProfInc = False;
        UChar insn_bytes[128];

        int j = emit(&hi_isProfInc,
            insn_bytes, sizeof insn_bytes, hi,
            mode64, VexEndnessLE,
            reinterpret_cast<void*>(vex_disp_cp_chain_me_to_slowEP),
            reinterpret_cast<void*>(vex_disp_cp_chain_me_to_fastEP),
            reinterpret_cast<void*>(vex_disp_cp_xindir),
            reinterpret_cast<void*>(vex_disp_cp_xassisted));
        EMASSERT(out_used + j <= len);
        EMASSERT(!hi_isProfInc);
        {
            HChar* dst = &buffer[out_used];
            for (int k = 0; k < j; k++) {
                dst[k] = insn_bytes[k];
            }
            out_used += j;
        }
    }
    return out_used;
}

static void initGuestState(VexGuestState& state, const IRContextInternal& context)
{
    memset(&state, 0, sizeof(state));
    state.host_EvC_COUNTER = 0xffff;
    RegisterAssign assign;
    for (auto&& ri : context.m_registerInit) {
        ri.m_control->reset();
        uintptr_t val = ri.m_control->init();
        assign.assign(&state, ri.m_name, val);
    }
}

static void checkRun(const char* who, const IRContextInternal& context, const uintptr_t* twoWords, const VexGuestState& guestState)
{
    unsigned checkPassed = 0, checkFailed = 0, count = 0;
    LOGE("checking %s...\n", who);
    for (auto& c : context.m_checks) {
        std::string info;
        if (c->check(&guestState, twoWords, info)) {
            checkPassed++;
        }
        else {
            checkFailed++;
        }
        LOGE("[%u]:%s.\n", ++count, info.c_str());
    }
    LOGE("passed %u, failed %u.\n", checkPassed, checkFailed);
}

static void fillIRSB(IRSB* irsb, const IRContextInternal& context)
{
    irsb->offsIP = offsetof(VexGuestState, guest_RIP);
    irsb->next = IRExpr_Get(irsb->offsIP, Ity_I64);
}

int main()
{
    vexSetAllocModeTEMP_and_clear();
    IRContextInternal context;
    yylex_init_extra(&context, &context.m_scanner);
    yyparse(&context);
    yylex_destroy(context.m_scanner);

    // allocate executable memory
    static const size_t execMemSize = 4096;
    void* execMem = mmap(nullptr, execMemSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    EMASSERT(execMem != MAP_FAILED);

    IRSB* irsb = context.m_irsb;
    fillIRSB(irsb, context);
    VexGuestState guestState;

    VexControl clo_vex_control;
    LibVEX_default_VexControl(&clo_vex_control);
    clo_vex_control.iropt_unroll_thresh = 400;
    clo_vex_control.guest_max_insns = 100;
    clo_vex_control.guest_chase_thresh = 99;
    clo_vex_control.iropt_register_updates = VexRegUpdSpAtMemAccess;

    // use define control to init vex.
    LibVEX_Init(&failure_exit, &log_bytes,
        1, /* debug_paranoia */
        False,
        &clo_vex_control);
    uintptr_t twoWords[2];
    // run with vex;
    if (!context.m_novex) {
        size_t generatedBytes = genVex(irsb, static_cast<HChar*>(execMem), execMemSize);
        initGuestState(guestState, context);
        vex_disp_run_translations(twoWords, &guestState, reinterpret_cast<Addr64>(execMem));
        checkRun("vex", context, twoWords, guestState);
    }
    // run with llvm
    jit::VexTranslator::init();
    jit::VexTranslatorEnv env = {
        reinterpret_cast<void*>(vex_disp_cp_chain_me_to_fastEP),
        reinterpret_cast<void*>(vex_disp_cp_chain_me_to_slowEP),
        reinterpret_cast<void*>(vex_disp_cp_xindir),
        reinterpret_cast<void*>(vex_disp_cp_xassisted),
        0,
        sizeof(guestState)

    };
    std::unique_ptr<jit::VexTranslator> translator(jit::VexTranslator::create());
    translator->translate(irsb, env);
    EMASSERT(translator->code() != nullptr && translator->codeSize() != 0);
    memcpy(execMem, translator->code(), translator->codeSize());
    initGuestState(guestState, context);
    vex_disp_run_translations(twoWords, &guestState, reinterpret_cast<Addr64>(execMem));
    checkRun("llvm", context, twoWords, guestState);
    return 0;
}
