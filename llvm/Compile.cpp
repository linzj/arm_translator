#include <assert.h>
#include <string.h>
#include "log.h"
#include "LLVMAPI.h"
#include "CompilerState.h"
#include "Compile.h"
#define SECTION_NAME_PREFIX "."
#define SECTION_NAME(NAME) (SECTION_NAME_PREFIX NAME)

namespace jit {
typedef CompilerState State;

static inline size_t round_up(size_t s, unsigned alignment)
{
    return (s + alignment - 1) & ~(alignment - 1);
}

static uint8_t* mmAllocateCodeSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned, const char* sectionName)
{
    State& state = *static_cast<State*>(opaqueState);

    state.m_codeSectionList.push_back(jit::ByteBuffer());
    state.m_codeSectionNames.push_back(sectionName);

    jit::ByteBuffer& bb(state.m_codeSectionList.back());
    size_t additionSize = state.m_platformDesc.m_prologueSize;
    size += additionSize;
    bb.resize(size);
    assert((reinterpret_cast<uintptr_t>(bb.data()) & (alignment - 1)) == 0);

    return const_cast<uint8_t*>(bb.data() + additionSize);
}

static uint8_t* mmAllocateDataSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned,
    const char* sectionName, LLVMBool)
{
    State& state = *static_cast<State*>(opaqueState);

    state.m_dataSectionList.push_back(jit::ByteBuffer());
    state.m_dataSectionNames.push_back(sectionName);

    jit::ByteBuffer& bb(state.m_dataSectionList.back());
    bb.resize(size);
    assert((reinterpret_cast<uintptr_t>(bb.data()) & (alignment - 1)) == 0);
    if (!strcmp(sectionName, SECTION_NAME("llvm_stackmaps"))) {
        state.m_stackMapsSection = &bb;
    }

    return const_cast<uint8_t*>(bb.data());
}

static LLVMBool mmApplyPermissions(void*, char**)
{
    return false;
}

static void mmDestroy(void*)
{
}

void compile(State& state)
{
    LLVMMCJITCompilerOptions options;
    llvmAPI->InitializeMCJITCompilerOptions(&options, sizeof(options));
    options.OptLevel = 2;
    LLVMExecutionEngineRef engine;
    char* error = 0;
    options.MCJMM = llvmAPI->CreateSimpleMCJITMemoryManager(
        &state, mmAllocateCodeSection, mmAllocateDataSection, mmApplyPermissions, mmDestroy);
    if (llvmAPI->CreateMCJITCompilerForModule(&engine, state.m_module, &options, sizeof(options), &error)) {
        LOGE("FATAL: Could not create LLVM execution engine: %s", error);
        assert(false);
    }
    LLVMModuleRef module = state.m_module;
    LLVMPassManagerRef functionPasses = 0;
    LLVMPassManagerRef modulePasses;
    LLVMTargetDataRef targetData = llvmAPI->GetExecutionEngineTargetData(engine);
    char* stringRepOfTargetData = llvmAPI->CopyStringRepOfTargetData(targetData);
    llvmAPI->SetDataLayout(module, stringRepOfTargetData);
    free(stringRepOfTargetData);

    LLVMPassManagerBuilderRef passBuilder = llvmAPI->PassManagerBuilderCreate();
    llvmAPI->PassManagerBuilderSetOptLevel(passBuilder, 2);
    llvmAPI->PassManagerBuilderUseInlinerWithThreshold(passBuilder, 275);
    llvmAPI->PassManagerBuilderSetSizeLevel(passBuilder, 0);

    functionPasses = llvmAPI->CreateFunctionPassManagerForModule(module);
    modulePasses = llvmAPI->CreatePassManager();

    llvmAPI->AddTargetData(llvmAPI->GetExecutionEngineTargetData(engine), modulePasses);

    llvmAPI->PassManagerBuilderPopulateFunctionPassManager(passBuilder, functionPasses);
    llvmAPI->PassManagerBuilderPopulateModulePassManager(passBuilder, modulePasses);

    llvmAPI->PassManagerBuilderDispose(passBuilder);

    llvmAPI->InitializeFunctionPassManager(functionPasses);
    for (LLVMValueRef function = llvmAPI->GetFirstFunction(module); function; function = llvmAPI->GetNextFunction(function))
        llvmAPI->RunFunctionPassManager(functionPasses, function);
    llvmAPI->FinalizeFunctionPassManager(functionPasses);

    llvmAPI->RunPassManager(modulePasses, module);
    state.m_entryPoint = reinterpret_cast<void*>(llvmAPI->GetPointerToGlobal(engine, state.m_function));

    if (functionPasses)
        llvmAPI->DisposePassManager(functionPasses);
    llvmAPI->DisposePassManager(modulePasses);
    llvmAPI->DisposeExecutionEngine(engine);
}
}
