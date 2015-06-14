#include <libvex.h>
#include <libvex_guest_amd64.h>
#include "LLVMAPI.h"
#include "InitializeLLVM.h"
#include "CompilerState.h"
#include "Output.h"
#include "Compile.h"
#include "Link.h"
#include "Registers.h"
#include "log.h"

extern "C" void* llvm_codegen_new(void);

extern "C" void * iselSB_LLVM              ( IRSB*, 
                                             VexArch,
                                             VexArchInfo*,
                                             VexAbiInfo*,
                                             Int offs_Host_EvC_Counter,
                                             Int offs_Host_EvC_FailAddr,
                                             Bool chainingAllowed,
                                             Bool addProfInc,
                                             Addr64 max_ga );


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

static void patchProloge(void*, uint8_t* start, uint8_t* end)
{
    uint8_t* p = start;
    *p++ = rexAMode_R(jit::RBP,
        jit::RDI);
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RDI);
}

static void patchDirect(void*, uint8_t* p, void* disp)
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
    p = emit64(p, reinterpret_cast<uintptr_t>(disp));
    /* movq %r11, RIP(%rbp) */

    /* 3 bytes: call*%r11 */
    *p++ = 0x41;
    *p++ = 0xFF;
    *p++ = 0xD3;
}

static void patchIndirect(void*, uint8_t* p, void* disp)
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
    p = emit64(p, reinterpret_cast<uintptr_t>(disp));
    /* movq %r11, RIP(%rbp) */

    /* 3 bytes: jmp *%r11 */
    *p++ = 0x41;
    *p++ = 0xFF;
    *p++ = 0xE3;
}

static void patchAssist(void*, uint8_t* p, void* disp)
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
    p = emit64(p, reinterpret_cast<uintptr_t>(disp));
    /* movq %r11, RIP(%rbp) */

    /* 3 bytes: jmp *%r11 */
    *p++ = 0x41;
    *p++ = 0xFF;
    *p++ = 0xE3;
}

void * iselSB_LLVM              ( IRSB* block, 
                                             VexArch arch,
                                             VexArchInfo* archInfo,
                                             VexAbiInfo* abiInfo,
                                             Int offs_Host_EvC_Counter,
                                             Int offs_Host_EvC_FailAddr,
                                             Bool chainingAllowed,
                                             Bool addProfInc,
                                             Addr64 max_ga )
{
    static bool hasInitLLVM = false;
    if (!hasInitLLVM) {
        initLLVM();
        hasInitLLVM = true;
    }
    using namespace jit;
    PlatformDesc desc = {
        sizeof(VexGuestAMD64State),
        offsetof(VexGuestAMD64State,guest_RIP), /* offset of pc */
        3, /* prologue size */
        17, /* direct size */
        17, /* indirect size */
        17, /* assist size */
    };
    CompilerState* state = new CompilerState("default", desc);
    return state;
}
