#include <unordered_map>
#include "TcgGenerator.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "Output.h"
#include "cpu.h"

namespace jit {

static inline CompilerState& state()
{
    PlatformDesc desc = {
        sizeof(CPUARMState),
        static_cast<size_t>(offsetof(CPUARMState, regs[15])), /* offset of pc */
        11, /* prologue size */
        17, /* direct size */
        17, /* indirect size */
        17, /* assist size */
    };
    static CompilerState mystate("qemu", desc);
    return mystate;
}

static inline IntrinsicRepository& repo()
{
    CompilerState& mystate = state();
    static IntrinsicRepository myrepo(mystate.m_context, mystate.m_module);
    return myrepo;
}

static inline LBuilder builder()
{
    static LBuilder mybuilder = llvmAPI->CreateBuilderInContext(state().m_context);
    return mybuilder;
}

static LType argType()
{
    static LType myargType = pointerType(arrayType(repo().int8, state().m_platformDesc.m_contextSize));
    return myargType;
}

static LValue g_function;
static LBasicBlock g_currentBB;
static LValue g_arg;

static inline LBasicBlock appendBasicBlock(const char* name)
{
    return jit::appendBasicBlock(state().m_context, state().m_function, name);
}

static inline void Output::positionToBBEnd(LBasicBlock bb)
{
    g_currentBB = bb;
    PositionBuilderAtEnd(builder(), bb);
}

void llvm_tcg_init(void)
{
    CompilerState& mystate = state();
    m_function = addFunction(
        state.m_module, "main", functionType(repo().int64, argType()));

    m_prologue = appendBasicBlock("");
    positionToBBEnd(m_prologue);
    g_arg = llvmAPI->GetParam(g_function, 0);
}

void llvm_tcg_deinit(void)
{
    llvmAPI->DeleteFunction(g_function);
    g_function = nullptr;
    g_currentBB = nullptr;
    g_arg = nullptr;
}
}

using namespace jit;
TCGv_i64 tcg_global_mem_new_i64(int reg, intptr_t offset, const char* name)
{
    LValue v = jit::buildAdd(g_arg, offset);
    LValue v2 = jit::buildPointerCast(v, repo().ref64);
    return reinterpret_cast<TCGv_i64>(v2);
}
