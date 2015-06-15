#include <unordered_map>
#include <vector>
#include "TcgGenerator.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "Output.h"
#include "cpu.h"

struct TCGCommonStruct {
    jit::LValue m_value;
};

struct TCGv_i32__ : public TCGCommonStruct {
};
struct TCGv_i64__ : public TCGCommonStruct {
};
struct TCGv_ptr__ : public TCGCommonStruct {
};
struct TCGv__ : public TCGCommonStruct {
};

namespace jit {

const static size_t allocate_unit = 4096 * 16;
typedef std::vector<void*> TcgBufferList;
static TcgBufferList g_bufferList;
uint8_t* g_currentBufferPointer;
uint8_t* g_currentBufferEnd;

static LBasicBlock g_currentBB;
static CompilerState* g_state;
static Output* g_output;

static PlatformDesc g_desc = {
    sizeof(CPUARMState),
    static_cast<size_t>(offsetof(CPUARMState, regs[15])), /* offset of pc */
    11, /* prologue size */
    17, /* direct size */
    17, /* indirect size */
    17, /* assist size */
};

template <typename Type>
Type allocateTcg()
{
    if (g_currentBufferPointer >= g_currentBufferEnd) {
        g_currentBufferPointer = static_cast<uint8_t*>(malloc(allocate_unit));
        g_currentBufferEnd = g_currentBufferPointer + allocate_unit;
        g_bufferList.push_back(g_currentBufferPointer);
    }
    Type r = reinterpret_cast<Type>(g_currentBufferPointer);
    g_currentBufferPointer += sizeof(*r);
    return r;
}

void clearTcgBuffer()
{
    for (void* b : g_bufferList) {
        free(b);
    }
    g_bufferList.clear();
    g_currentBufferPointer = nullptr;
    g_currentBufferEnd = nullptr;
}

static LType argType()
{
    LType globalInt8Type = llvmAPI->Int8Type();
    static LType myargType = pointerType(arrayType(globalInt8Type, g_desc.m_contextSize));
    return myargType;
}

static inline LBasicBlock appendBasicBlock(const char* name)
{
    return jit::appendBasicBlock(g_state->m_context, g_state->m_function, name);
}

void llvm_tcg_init(void)
{
    g_state = new CompilerState("qemu", g_desc);
    g_output = new Output(*g_state);
}

void llvm_tcg_deinit(void)
{
    llvmAPI->DeleteFunction(g_state->m_function);
    delete g_output;
    g_output = nullptr;
    delete g_state;
    g_state = nullptr;
    clearTcgBuffer();
}
}

using namespace jit;

TCGv_i64 tcg_global_mem_new_i64(int reg, intptr_t offset, const char* name)
{
    LValue v = g_output->buildAdd(g_output->arg(), g_output->constInt32(offset));
    LValue v2 = g_output->buildPointerCast(v, g_output->repo().ref64);
    TCGv_i64 ret = allocateTcg<TCGv_i64>();
    ret->m_value = v2;
    return ret;
}
