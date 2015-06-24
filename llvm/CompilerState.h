#ifndef COMPILERSTATE_H
#define COMPILERSTATE_H
#include <vector>
#include <unordered_map>
#include <list>
#include <string>
#include <stdint.h>
#include "LLVMHeaders.h"
#include "PlatformDesc.h"
namespace jit {
enum class PatchType {
    Assist,
    TcgDirect,
    TcgIndirect,
};

struct PatchDesc {
    PatchType m_type;
    void* m_function;
};

typedef std::vector<uint8_t> ByteBuffer;
typedef std::list<ByteBuffer> BufferList;
typedef std::list<uint8_t*> ExecutableBufferList;
typedef std::list<std::string> StringList;
typedef std::unordered_map<unsigned /* stackmaps id */, PatchDesc> PatchMap;

struct CompilerState {
    ExecutableBufferList m_codeSectionList;
    BufferList m_dataSectionList;
    StringList m_dataSectionNames;
    ByteBuffer* m_stackMapsSection;
    PatchMap m_patchMap;
    LLVMModuleRef m_module;
    LLVMValueRef m_function;
    LLVMContextRef m_context;
    void* m_entryPoint;
    struct PlatformDesc m_platformDesc;
    class ExecutableMemoryAllocator* m_executableMemAllocator;
    CompilerState(const char* moduleName, const PlatformDesc& desc);
    ~CompilerState();
    CompilerState(const CompilerState&) = delete;
    const CompilerState& operator=(const CompilerState&) = delete;
};
}

#endif /* COMPILERSTATE_H */
