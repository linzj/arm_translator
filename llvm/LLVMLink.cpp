#include <bitset>
#include "X86Assembler.h"
#include "StackMaps.h"
#include "CompilerState.h"
#include "Abbreviations.h"
#include "LLVMDisasContext.h"
#include "log.h"

namespace jit {

struct LinkDesc {
    void* m_opaque;
    void* m_dispTcgDirect;
    void* m_dispTcgIndirect;
    void (*m_patchPrologue)(void* opaque, uint8_t* start);
    void (*m_patchTcgDirect)(void* opaque, uint8_t* toFill, void*);
    void (*m_patchTcgIndirect)(void* opaque, uint8_t* toFill, void*);
    uint8_t* (*m_patchMovRegToMem)(void* opaque, uint8_t* toFill);
    uint8_t* (*m_patchMovMemToMem)(void* opaque, uint8_t* toFill);
};

static void patchProloge(void*, uint8_t* start)
{
    JSC::X86Assembler assembler(reinterpret_cast<char*>(start), 2);
    assembler.movl_rr(JSC::X86Registers::ebp, JSC::X86Registers::ecx);
}

static void patchDirect(void*, uint8_t* p, void* entry)
{
    // epilogue
    JSC::X86Assembler assembler(reinterpret_cast<char*>(p), 10);
    assembler.movl_rr(JSC::X86Registers::ebp, JSC::X86Registers::esp);
    assembler.pop_r(JSC::X86Registers::ebp);
    assembler.movl_i32r(reinterpret_cast<int>(entry), JSC::X86Registers::eax);
    assembler.call(JSC::X86Registers::eax);
}

void patchIndirect(void*, uint8_t* p, void* entry)
{
    JSC::X86Assembler assembler(reinterpret_cast<char*>(p), 10);
    assembler.movl_rr(JSC::X86Registers::ebp, JSC::X86Registers::esp);
    assembler.pop_r(JSC::X86Registers::ebp);
    assembler.movl_i32r(reinterpret_cast<int>(entry), JSC::X86Registers::eax);
    assembler.jmp_r(JSC::X86Registers::eax);
}

void LLVMDisasContext::link()
{
    StackMaps sm;
    const LinkDesc desc = {
        nullptr,
        m_dispDirect,
        m_dispIndirect,
        patchProloge,
        patchDirect,
        patchIndirect,
    };
    DataView dv(state()->m_stackMapsSection->data());
    sm.parse(&dv);
    auto rm = sm.computeRecordMap();
    EMASSERT(state()->m_codeSectionList.size() == 1);
    uint8_t* prologue = state()->m_codeSectionList.front();
    uint8_t* body = static_cast<uint8_t*>(state()->m_entryPoint);
    desc.m_patchPrologue(desc.m_opaque, prologue);
    for (auto& record : rm) {
        EMASSERT(record.second.size() == 1);
        auto found = state()->m_patchMap.find(record.first);
        if (found == state()->m_patchMap.end()) {
            // should be the tcg helpers.
            continue;
        }
        PatchDesc& patchDesc = found->second;
        switch (patchDesc.m_type) {
        case PatchType::TcgDirect: {
            auto& recordUnit = record.second[0];
            desc.m_patchTcgDirect(desc.m_opaque, body + recordUnit.instructionOffset, desc.m_dispTcgDirect);
        } break;
        case PatchType::TcgIndirect: {
            auto& recordUnit = record.second[0];
            desc.m_patchTcgIndirect(desc.m_opaque, body + recordUnit.instructionOffset, desc.m_dispTcgIndirect);
        } break;
        default:
            EMUNREACHABLE();
        }
    }
}
}
