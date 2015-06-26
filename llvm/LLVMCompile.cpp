#include <string.h>
#include "log.h"
#include "LLVMAPI.h"
#include "CompilerState.h"
#include "ExecutableMemoryAllocator.h"
#include "LLVMDisasContext.h"
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

    size_t additionSize = state.m_platformDesc.m_prologueSize;
    size += additionSize;
    uint8_t* buffer = static_cast<uint8_t*>(state.m_executableMemAllocator->allocate(size, alignment));
    state.m_codeSectionList.push_back(buffer);

    return const_cast<uint8_t*>(buffer + additionSize);
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
    EMASSERT((reinterpret_cast<uintptr_t>(bb.data()) & (alignment - 1)) == 0);
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

void LLVMDisasContext::compile()
{
#ifdef ENABLE_DUMP_LLVM_MODULE
    dumpModule(state()->m_module);
#endif // ENABLE_DUMP_LLVM_MODULE
    LLVMMCJITCompilerOptions options;
    llvmAPI->InitializeMCJITCompilerOptions(&options, sizeof(options));
    options.OptLevel = 2;
    LLVMExecutionEngineRef engine;
    char* error = 0;
    options.MCJMM = llvmAPI->CreateSimpleMCJITMemoryManager(
        state(), mmAllocateCodeSection, mmAllocateDataSection, mmApplyPermissions, mmDestroy);

    if (llvmAPI->CreateMCJITCompilerForModule(&engine, state()->m_module, &options, sizeof(options), &error)) {
        LOGE("FATAL: Could not create LLVM execution engine: %s", error);
        EMASSERT(false);
    }
    LLVMModuleRef module = state()->m_module;
    LLVMPassManagerRef functionPasses = 0;
    LLVMPassManagerRef modulePasses;
    LLVMTargetDataRef targetData = llvmAPI->GetExecutionEngineTargetData(engine);
    char* stringRepOfTargetData = llvmAPI->CopyStringRepOfTargetData(targetData);
    llvmAPI->SetDataLayout(module, stringRepOfTargetData);
    free(stringRepOfTargetData);

    modulePasses = llvmAPI->CreatePassManager();
    llvmAPI->AddTargetData(targetData, modulePasses);
    llvmAPI->AddAnalysisPasses(llvmAPI->GetExecutionEngineTargetMachine(engine), modulePasses);
    llvmAPI->AddPromoteMemoryToRegisterPass(modulePasses);
    llvmAPI->AddGlobalOptimizerPass(modulePasses);
    llvmAPI->AddFunctionInliningPass(modulePasses);
    llvmAPI->AddPruneEHPass(modulePasses);
    llvmAPI->AddGlobalDCEPass(modulePasses);
    llvmAPI->AddConstantPropagationPass(modulePasses);
    llvmAPI->AddAggressiveDCEPass(modulePasses);
    llvmAPI->AddInstructionCombiningPass(modulePasses);
    // BEGIN - DO NOT CHANGE THE ORDER OF THE ALIAS ANALYSIS PASSES
    llvmAPI->AddTypeBasedAliasAnalysisPass(modulePasses);
    llvmAPI->AddBasicAliasAnalysisPass(modulePasses);
    // END - DO NOT CHANGE THE ORDER OF THE ALIAS ANALYSIS PASSES
    llvmAPI->AddGVNPass(modulePasses);
    llvmAPI->AddCFGSimplificationPass(modulePasses);
    llvmAPI->AddDeadStoreEliminationPass(modulePasses);

    llvmAPI->AddLowerSwitchPass(modulePasses);

    llvmAPI->RunPassManager(modulePasses, module);
    state()->m_entryPoint = reinterpret_cast<void*>(llvmAPI->GetPointerToGlobal(engine, state()->m_function));

    if (functionPasses)
        llvmAPI->DisposePassManager(functionPasses);
    llvmAPI->DisposePassManager(modulePasses);
    llvmAPI->DisposeExecutionEngine(engine);
}
}
