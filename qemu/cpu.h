#ifndef CPU_H
#define CPU_H
#include "tgtypes.h"
#include "ghash.h"

typedef struct float_status {
    signed char float_detect_tininess;
    signed char float_rounding_mode;
    signed char float_exception_flags;
    signed char floatx80_rounding_precision;
    /* should denormalised results go to zero and set the inexact flag? */
    flag flush_to_zero;
    /* should denormalised inputs go to zero and set the input_denormal flag? */
    flag flush_inputs_to_zero;
    flag default_nan_mode;
} float_status;

typedef struct CPUARMState {
    /* Regs for current mode.  */
    uint32_t regs[16];

    /* 32/64 switch only happens when taking and returning from
     * exceptions so the overlap semantics are taken care of then
     * instead of having a complicated union.
     */
    /* Regs for A64 mode.  */
    uint64_t xregs[32];
    uint64_t pc;
    /* PSTATE isn't an architectural register for ARMv8. However, it is
     * convenient for us to assemble the underlying state into a 32 bit format
     * identical to the architectural format used for the SPSR. (This is also
     * what the Linux kernel's 'pstate' field in signal handlers and KVM's
     * 'pstate' register are.) Of the PSTATE bits:
     *  NZCV are kept in the split out env->CF/VF/NF/ZF, (which have the same
     *    semantics as for AArch32, as described in the comments on each field)
     *  nRW (also known as M[4]) is kept, inverted, in env->aarch64
     *  DAIF (exception masks) are kept in env->daif
     *  all other bits are stored in their correct places in env->pstate
     */
    uint32_t pstate;
    uint32_t aarch64; /* 1 if CPU is in aarch64 state; inverse of PSTATE.nRW */

    /* Frequently accessed CPSR bits are stored separately for efficiency.
       This contains all the other bits.  Use cpsr_{read,write} to access
       the whole CPSR.  */
    uint32_t uncached_cpsr;
    uint32_t spsr;

    /* Banked registers.  */
    uint64_t banked_spsr[8];
    uint32_t banked_r13[8];
    uint32_t banked_r14[8];

    /* These hold r8-r12.  */
    uint32_t usr_regs[5];
    uint32_t fiq_regs[5];

    /* cpsr flag cache for faster execution */
    uint32_t CF; /* 0 or 1 */
    uint32_t VF; /* V is the bit 31. All other bits are undefined */
    uint32_t NF; /* N is bit 31. All other bits are undefined.  */
    uint32_t ZF; /* Z set if zero.  */
    uint32_t QF; /* 0 or 1 */
    uint32_t GE; /* cpsr[19:16] */
    uint32_t thumb; /* cpsr[5]. 0 = arm mode, 1 = thumb mode. */
    uint32_t condexec_bits; /* IT bits.  cpsr[15:10,26:25].  */
    uint64_t daif; /* exception masks, in the bits they are in in PSTATE */

    uint64_t elr_el[4]; /* AArch64 exception link regs  */
    uint64_t sp_el[4]; /* AArch64 banked stack pointers */

    /* System control coprocessor (cp15) */
    struct {
        uint32_t c0_cpuid;
        uint64_t c0_cssel; /* Cache size selection.  */
        uint64_t c1_sys; /* System control register.  */
        uint64_t c1_coproc; /* Coprocessor access register.  */
        uint32_t c1_xscaleauxcr; /* XScale auxiliary control register.  */
        uint64_t ttbr0_el1; /* MMU translation table base 0. */
        uint64_t ttbr1_el1; /* MMU translation table base 1. */
        uint64_t c2_control; /* MMU translation table base control.  */
        uint32_t c2_mask; /* MMU translation table base selection mask.  */
        uint32_t c2_base_mask; /* MMU translation table base 0 mask. */
        uint32_t c2_data; /* MPU data cachable bits.  */
        uint32_t c2_insn; /* MPU instruction cachable bits.  */
        uint32_t c3; /* MMU domain access control register
                        MPU write buffer control.  */
        uint32_t pmsav5_data_ap; /* PMSAv5 MPU data access permissions */
        uint32_t pmsav5_insn_ap; /* PMSAv5 MPU insn access permissions */
        uint64_t hcr_el2; /* Hypervisor configuration register */
        uint64_t scr_el3; /* Secure configuration register.  */
        uint32_t ifsr_el2; /* Fault status registers.  */
        uint64_t esr_el[4];
        uint32_t c6_region[8]; /* MPU base/size registers.  */
        uint64_t far_el[4]; /* Fault address registers.  */
        uint64_t par_el1;  /* Translation result. */
        uint32_t c9_insn; /* Cache lockdown registers.  */
        uint32_t c9_data;
        uint64_t c9_pmcr; /* performance monitor control register */
        uint64_t c9_pmcnten; /* perf monitor counter enables */
        uint32_t c9_pmovsr; /* perf monitor overflow status */
        uint32_t c9_pmxevtyper; /* perf monitor event type */
        uint32_t c9_pmuserenr; /* perf monitor user enable */
        uint32_t c9_pminten; /* perf monitor interrupt enables */
        uint64_t mair_el1;
        uint64_t vbar_el[4]; /* vector base address register */
        uint32_t c13_fcse; /* FCSE PID.  */
        uint64_t contextidr_el1; /* Context ID.  */
        uint64_t tpidr_el0; /* User RW Thread register.  */
        uint64_t tpidrro_el0; /* User RO Thread register.  */
        uint64_t tpidr_el1; /* Privileged Thread register.  */
        uint64_t c14_cntfrq; /* Counter Frequency register */
        uint64_t c14_cntkctl; /* Timer Control register */
        uint32_t c15_cpar; /* XScale Coprocessor Access Register */
        uint32_t c15_ticonfig; /* TI925T configuration byte.  */
        uint32_t c15_i_max; /* Maximum D-cache dirty line index.  */
        uint32_t c15_i_min; /* Minimum D-cache dirty line index.  */
        uint32_t c15_threadid; /* TI debugger thread-ID.  */
        uint32_t c15_config_base_address; /* SCU base address.  */
        uint32_t c15_diagnostic; /* diagnostic register */
        uint32_t c15_power_diagnostic;
        uint32_t c15_power_control; /* power control */
        uint64_t dbgbvr[16]; /* breakpoint value registers */
        uint64_t dbgbcr[16]; /* breakpoint control registers */
        uint64_t dbgwvr[16]; /* watchpoint value registers */
        uint64_t dbgwcr[16]; /* watchpoint control registers */
        uint64_t mdscr_el1;
        /* If the counter is enabled, this stores the last time the counter
         * was reset. Otherwise it stores the counter value
         */
        uint64_t c15_ccnt;
        uint64_t pmccfiltr_el0; /* Performance Monitor Filter Register */
    } cp15;

    struct {
        uint32_t other_sp;
        uint32_t vecbase;
        uint32_t basepri;
        uint32_t control;
        int current_sp;
        int exception;
        int pending_exception;
    } v7m;

    /* Information associated with an exception about to be taken:
     * code which raises an exception must set cs->exception_index and
     * the relevant parts of this structure; the cpu_do_interrupt function
     * will then set the guest-visible registers as part of the exception
     * entry process.
     */
    struct {
        uint32_t syndrome; /* AArch64 format syndrome register */
        uint32_t fsr; /* AArch32 format fault status register info */
        uint64_t vaddress; /* virtual addr associated with exception, if any */
        /* If we implement EL2 we will also need to store information
         * about the intermediate physical address for stage 2 faults.
         */
    } exception;

    /* Thumb-2 EE state.  */
    uint32_t teecr;
    uint32_t teehbr;

    /* VFP coprocessor state.  */
    struct {
        /* VFP/Neon register state. Note that the mapping between S, D and Q
         * views of the register bank differs between AArch64 and AArch32:
         * In AArch32:
         *  Qn = regs[2n+1]:regs[2n]
         *  Dn = regs[n]
         *  Sn = regs[n/2] bits 31..0 for even n, and bits 63..32 for odd n
         * (and regs[32] to regs[63] are inaccessible)
         * In AArch64:
         *  Qn = regs[2n+1]:regs[2n]
         *  Dn = regs[2n]
         *  Sn = regs[2n] bits 31..0
         * This corresponds to the architecturally defined mapping between
         * the two execution states, and means we do not need to explicitly
         * map these registers when changing states.
         */
        double regs[64];

        uint32_t xregs[16];
        /* We store these fpcsr fields separately for convenience.  */
        int vec_len;
        int vec_stride;

        /* scratch space when Tn are not sufficient.  */
        uint32_t scratch[8];

        /* fp_status is the "normal" fp status. standard_fp_status retains
         * values corresponding to the ARM "Standard FPSCR Value", ie
         * default-NaN, flush-to-zero, round-to-nearest and is used by
         * any operations (generally Neon) which the architecture defines
         * as controlled by the standard FPSCR value rather than the FPSCR.
         *
         * To avoid having to transfer exception bits around, we simply
         * say that the FPSCR cumulative exception flags are the logical
         * OR of the flags in the two fp statuses. This relies on the
         * only thing which needs to read the exception flags being
         * an explicit FPSCR read.
         */
        float_status fp_status;
        float_status standard_fp_status;
    } vfp;
    uint64_t exclusive_addr;
    uint64_t exclusive_val;
    uint64_t exclusive_high;
    uint64_t exclusive_test;
    uint32_t exclusive_info;

    /* iwMMXt coprocessor state.  */
    struct {
        uint64_t regs[16];
        uint64_t val;

        uint32_t cregs[16];
    } iwmmxt;

    /* For mixed endian mode.  */
    bool bswap_code;

    /* For usermode syscall translation.  */
    int eabi;

    struct CPUBreakpoint *cpu_breakpoint[16];
    struct CPUWatchpoint *cpu_watchpoint[16];


    /* These fields after the common ones so they are preserved on reset.  */

    /* Internal CPU feature flags.  */
    uint64_t features;

    void *nvic;
} CPUARMState;

#define EXCP_UDEF            1   /* undefined instruction */
#define EXCP_SWI             2   /* software interrupt */
#define EXCP_PREFETCH_ABORT  3
#define EXCP_DATA_ABORT      4
#define EXCP_IRQ             5
#define EXCP_FIQ             6
#define EXCP_BKPT            7
#define EXCP_EXCEPTION_EXIT  8   /* Return from v7M exception.  */
#define EXCP_KERNEL_TRAP     9   /* Jumped to kernel code page.  */
#define EXCP_STREX          10
#define EXCP_HVC            11   /* HyperVisor Call */
#define EXCP_HYP_TRAP       12
#define EXCP_SMC            13   /* Secure Monitor Call */
#define EXCP_VIRQ           14
#define EXCP_VFIQ           15

enum arm_features {
    ARM_FEATURE_VFP,
    ARM_FEATURE_AUXCR,  /* ARM1026 Auxiliary control register.  */
    ARM_FEATURE_XSCALE, /* Intel XScale extensions.  */
    ARM_FEATURE_IWMMXT, /* Intel iwMMXt extension.  */
    ARM_FEATURE_V6,
    ARM_FEATURE_V6K,
    ARM_FEATURE_V7,
    ARM_FEATURE_THUMB2,
    ARM_FEATURE_MPU,    /* Only has Memory Protection Unit, not full MMU.  */
    ARM_FEATURE_VFP3,
    ARM_FEATURE_VFP_FP16,
    ARM_FEATURE_NEON,
    ARM_FEATURE_THUMB_DIV, /* divide supported in Thumb encoding */
    ARM_FEATURE_M, /* Microcontroller profile.  */
    ARM_FEATURE_OMAPCP, /* OMAP specific CP15 ops handling.  */
    ARM_FEATURE_THUMB2EE,
    ARM_FEATURE_V7MP,    /* v7 Multiprocessing Extensions */
    ARM_FEATURE_V4T,
    ARM_FEATURE_V5,
    ARM_FEATURE_STRONGARM,
    ARM_FEATURE_VAPA, /* cp15 VA to PA lookups */
    ARM_FEATURE_ARM_DIV, /* divide supported in ARM encoding */
    ARM_FEATURE_VFP4, /* VFPv4 (implies that NEON is v2) */
    ARM_FEATURE_GENERIC_TIMER,
    ARM_FEATURE_MVFR, /* Media and VFP Feature Registers 0 and 1 */
    ARM_FEATURE_DUMMY_C15_REGS, /* RAZ/WI all of cp15 crn=15 */
    ARM_FEATURE_CACHE_TEST_CLEAN, /* 926/1026 style test-and-clean ops */
    ARM_FEATURE_CACHE_DIRTY_REG, /* 1136/1176 cache dirty status register */
    ARM_FEATURE_CACHE_BLOCK_OPS, /* v6 optional cache block operations */
    ARM_FEATURE_MPIDR, /* has cp15 MPIDR */
    ARM_FEATURE_PXN, /* has Privileged Execute Never bit */
    ARM_FEATURE_LPAE, /* has Large Physical Address Extension */
    ARM_FEATURE_V8,
    ARM_FEATURE_AARCH64, /* supports 64 bit mode */
    ARM_FEATURE_V8_AES, /* implements AES part of v8 Crypto Extensions */
    ARM_FEATURE_CBAR, /* has cp15 CBAR */
    ARM_FEATURE_CRC, /* ARMv8 CRC instructions */
    ARM_FEATURE_CBAR_RO, /* has cp15 CBAR and it is read-only */
    ARM_FEATURE_EL2, /* has EL2 Virtualization support */
    ARM_FEATURE_EL3, /* has EL3 Secure monitor support */
    ARM_FEATURE_V8_SHA1, /* implements SHA1 part of v8 Crypto Extensions */
    ARM_FEATURE_V8_SHA256, /* implements SHA256 part of v8 Crypto Extensions */
    ARM_FEATURE_V8_PMULL, /* implements PMULL part of v8 Crypto Extensions */
};

typedef union {
    double d;
    struct {
        uint32_t lower;
        uint32_t upper;
    } l;
    uint64_t ll;
} CPU_DoubleU;

#define ARM_IWMMXT_wCID		0
#define ARM_IWMMXT_wCon		1
#define ARM_IWMMXT_wCSSF	2
#define ARM_IWMMXT_wCASF	3
#define ARM_IWMMXT_wCGR0	8
#define ARM_IWMMXT_wCGR1	9
#define ARM_IWMMXT_wCGR2	10
#define ARM_IWMMXT_wCGR3	11

#define ARM_VFP_FPSID   0
#define ARM_VFP_FPSCR   1
#define ARM_VFP_MVFR2   5
#define ARM_VFP_MVFR1   6
#define ARM_VFP_MVFR0   7
#define ARM_VFP_FPEXC   8
#define ARM_VFP_FPINST  9
#define ARM_VFP_FPINST2 10

enum arm_fprounding {
    FPROUNDING_TIEEVEN,
    FPROUNDING_POSINF,
    FPROUNDING_NEGINF,
    FPROUNDING_ZERO,
    FPROUNDING_TIEAWAY,
    FPROUNDING_ODD
};

enum {
    float_round_nearest_even = 0,
    float_round_down         = 1,
    float_round_up           = 2,
    float_round_to_zero      = 3,
    float_round_ties_away    = 4,
};

#define TARGET_PAGE_BITS 12
#define TARGET_PAGE_SIZE (1 << TARGET_PAGE_BITS)
#define TARGET_PAGE_MASK ~(TARGET_PAGE_SIZE - 1)
#define TARGET_PAGE_ALIGN(addr) (((addr) + TARGET_PAGE_SIZE - 1) & TARGET_PAGE_MASK)

#define CPSR_M (0x1fU)
#define CPSR_T (1U << 5)
#define CPSR_F (1U << 6)
#define CPSR_I (1U << 7)
#define CPSR_A (1U << 8)
#define CPSR_E (1U << 9)
#define CPSR_IT_2_7 (0xfc00U)
#define CPSR_GE (0xfU << 16)
#define CPSR_IL (1U << 20)
#define CPSR_RESERVED (0x7U << 21)
#define CPSR_J (1U << 24)
#define CPSR_IT_0_1 (3U << 25)
#define CPSR_Q (1U << 27)
#define CPSR_V (1U << 28)
#define CPSR_C (1U << 29)
#define CPSR_Z (1U << 30)
#define CPSR_N (1U << 31)
#define CPSR_NZCV (CPSR_N | CPSR_Z | CPSR_C | CPSR_V)
#define CPSR_AIF (CPSR_A | CPSR_I | CPSR_F)
#define CPSR_IT (CPSR_IT_0_1 | CPSR_IT_2_7)
#define CACHED_CPSR_BITS (CPSR_T | CPSR_AIF | CPSR_GE | CPSR_IT | CPSR_Q \
    | CPSR_NZCV)

#define CPSR_EXEC (CPSR_T | CPSR_IT | CPSR_J | CPSR_IL)
/* Mask of bits which may be set by exception return copying them from SPSR */
#define CPSR_ERET_MASK (~CPSR_RESERVED)
#define CPSR_USER (CPSR_NZCV | CPSR_Q | CPSR_GE)
typedef enum CPAccessResult {
    /* Access is permitted */
    CP_ACCESS_OK = 0,
    /* Access fails due to a configurable trap or enable which would
     * result in a categorized exception syndrome giving information about
     * the failing instruction (ie syndrome category 0x3, 0x4, 0x5, 0x6,
     * 0xc or 0x18).
     */
    CP_ACCESS_TRAP = 1,
    /* Access fails and results in an exception syndrome 0x0 ("uncategorized").
     * Note that this is not a catch-all case -- the set of cases which may
     * result in this failure is specifically defined by the architecture.
     */
    CP_ACCESS_TRAP_UNCATEGORIZED = 2,
} CPAccessResult;

typedef struct ARMCPRegInfo ARMCPRegInfo;

typedef uint64_t CPReadFn(CPUARMState *env, const ARMCPRegInfo *opaque);
typedef void CPWriteFn(CPUARMState *env, const ARMCPRegInfo *opaque,
                       uint64_t value);
/* Access permission check functions for coprocessor registers. */
typedef CPAccessResult CPAccessFn(CPUARMState *env, const ARMCPRegInfo *opaque);
/* Hook function for register reset */
typedef void CPResetFn(CPUARMState *env, const ARMCPRegInfo *opaque);

struct ARMCPRegInfo {
    /* Name of register (useful mainly for debugging, need not be unique) */
    const char *name;
    /* Location of register: coprocessor number and (crn,crm,opc1,opc2)
     * tuple. Any of crm, opc1 and opc2 may be CP_ANY to indicate a
     * 'wildcard' field -- any value of that field in the MRC/MCR insn
     * will be decoded to this register. The register read and write
     * callbacks will be passed an ARMCPRegInfo with the crn/crm/opc1/opc2
     * used by the program, so it is possible to register a wildcard and
     * then behave differently on read/write if necessary.
     * For 64 bit registers, only crm and opc1 are relevant; crn and opc2
     * must both be zero.
     * For AArch64-visible registers, opc0 is also used.
     * Since there are no "coprocessors" in AArch64, cp is purely used as a
     * way to distinguish (for KVM's benefit) guest-visible system registers
     * from demuxed ones provided to preserve the "no side effects on
     * KVM register read/write from QEMU" semantics. cp==0x13 is guest
     * visible (to match KVM's encoding); cp==0 will be converted to
     * cp==0x13 when the ARMCPRegInfo is registered, for convenience.
     */
    uint8_t cp;
    uint8_t crn;
    uint8_t crm;
    uint8_t opc0;
    uint8_t opc1;
    uint8_t opc2;
    /* Execution state in which this register is visible: ARM_CP_STATE_* */
    int state;
    /* Register type: ARM_CP_* bits/values */
    int type;
    /* Access rights: PL*_[RW] */
    int access;
    /* The opaque pointer passed to define_arm_cp_regs_with_opaque() when
     * this register was defined: can be used to hand data through to the
     * register read/write functions, since they are passed the ARMCPRegInfo*.
     */
    void *opaque;
    /* Value of this register, if it is ARM_CP_CONST. Otherwise, if
     * fieldoffset is non-zero, the reset value of the register.
     */
    uint64_t resetvalue;
    /* Offset of the field in CPUARMState for this register. This is not
     * needed if either:
     *  1. type is ARM_CP_CONST or one of the ARM_CP_SPECIALs
     *  2. both readfn and writefn are specified
     */
    ptrdiff_t fieldoffset; /* offsetof(CPUARMState, field) */
    /* Function for making any access checks for this register in addition to
     * those specified by the 'access' permissions bits. If NULL, no extra
     * checks required. The access check is performed at runtime, not at
     * translate time.
     */
    CPAccessFn *accessfn;
    /* Function for handling reads of this register. If NULL, then reads
     * will be done by loading from the offset into CPUARMState specified
     * by fieldoffset.
     */
    CPReadFn *readfn;
    /* Function for handling writes of this register. If NULL, then writes
     * will be done by writing to the offset into CPUARMState specified
     * by fieldoffset.
     */
    CPWriteFn *writefn;
    /* Function for doing a "raw" read; used when we need to copy
     * coprocessor state to the kernel for KVM or out for
     * migration. This only needs to be provided if there is also a
     * readfn and it has side effects (for instance clear-on-read bits).
     */
    CPReadFn *raw_readfn;
    /* Function for doing a "raw" write; used when we need to copy KVM
     * kernel coprocessor state into userspace, or for inbound
     * migration. This only needs to be provided if there is also a
     * writefn and it masks out "unwritable" bits or has write-one-to-clear
     * or similar behaviour.
     */
    CPWriteFn *raw_writefn;
    /* Function for resetting the register. If NULL, then reset will be done
     * by writing resetvalue to the field specified in fieldoffset. If
     * fieldoffset is 0 then no reset will be done.
     */
    CPResetFn *resetfn;
};
/* ARMCPRegInfo type field bits. If the SPECIAL bit is set this is a
 * special-behaviour cp reg and bits [15..8] indicate what behaviour
 * it has. Otherwise it is a simple cp reg, where CONST indicates that
 * TCG can assume the value to be constant (ie load at translate time)
 * and 64BIT indicates a 64 bit wide coprocessor register. SUPPRESS_TB_END
 * indicates that the TB should not be ended after a write to this register
 * (the default is that the TB ends after cp writes). OVERRIDE permits
 * a register definition to override a previous definition for the
 * same (cp, is64, crn, crm, opc1, opc2) tuple: either the new or the
 * old must have the OVERRIDE bit set.
 * NO_MIGRATE indicates that this register should be ignored for migration;
 * (eg because any state is accessed via some other coprocessor register).
 * IO indicates that this register does I/O and therefore its accesses
 * need to be surrounded by gen_io_start()/gen_io_end(). In particular,
 * registers which implement clocks or timers require this.
 */
#define ARM_CP_SPECIAL 1
#define ARM_CP_CONST 2
#define ARM_CP_64BIT 4
#define ARM_CP_SUPPRESS_TB_END 8
#define ARM_CP_OVERRIDE 16
#define ARM_CP_NO_MIGRATE 32
#define ARM_CP_IO 64
#define ARM_CP_NOP (ARM_CP_SPECIAL | (1 << 8))
#define ARM_CP_WFI (ARM_CP_SPECIAL | (2 << 8))
#define ARM_CP_NZCV (ARM_CP_SPECIAL | (3 << 8))
#define ARM_CP_CURRENTEL (ARM_CP_SPECIAL | (4 << 8))
#define ARM_CP_DC_ZVA (ARM_CP_SPECIAL | (5 << 8))
#define ARM_LAST_SPECIAL ARM_CP_DC_ZVA
/* Used only as a terminator for ARMCPRegInfo lists */
#define ARM_CP_SENTINEL 0xffff
#define ARM_CP_FLAG_MASK 0x7f
#define MMU_USER_IDX 0

typedef struct
{
    GHashTable* cp_regs;
    CPUARMState env;
} CPU;
#define EXCP_INTERRUPT 	0x10000 /* async interruption */
#define EXCP_HLT        0x10001 /* hlt instruction reached */
#define EXCP_DEBUG      0x10002 /* cpu stopped after a breakpoint or singlestep */
#define EXCP_HALTED     0x10003 /* cpu is halted (waiting for external event) */
#define EXCP_YIELD      0x10004 /* cpu wants to yield timeslice to another */
enum CPUDumpFlags {
    CPU_DUMP_CODE = 0x00010000,
    CPU_DUMP_FPU  = 0x00020000,
    CPU_DUMP_CCOP = 0x00040000,
};

#define ENCODE_CP_REG(cp, is64, crn, crm, opc1, opc2)   \
    (((cp) << 16) | ((is64) << 15) | ((crn) << 11) |    \
     ((crm) << 7) | ((opc1) << 3) | (opc2))
#endif /* CPU_H */
