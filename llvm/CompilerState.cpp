#include "LLVMAPI.h"
#include "CompilerState.h"

namespace jit {

CompilerState::CompilerState(const char* moduleName, const PlatformDesc& desc)
    : m_stackMapsSection(nullptr)
    , m_module(nullptr)
    , m_function(nullptr)
    , m_context(nullptr)
    , m_entryPoint(nullptr)
    , m_platformDesc(desc)
{
    m_context = llvmAPI->ContextCreate();
    m_module = llvmAPI->ModuleCreateWithNameInContext("test", m_context);
}

CompilerState::~CompilerState()
{
    llvmAPI->ContextDispose(m_context);
}
}
