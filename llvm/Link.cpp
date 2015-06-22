#include <bitset>
#include "X86Assembler.h"
#include "StackMaps.h"
#include "CompilerState.h"
#include "Abbreviations.h"
#include "Link.h"
#include "log.h"

namespace jit {

static void handleTcgHelper(const LinkDesc& desc, const StackMaps::Record& record, uint8_t* body, void* target)
{
    uint8_t* p = body + record.instructionOffset;
    *p++ = 0xb8;
    *reinterpret_cast<uint32_t*>(p) = reinterpret_cast<uintptr_t>(target);
    p += sizeof(uint32_t);

    *p++ = 0xff;
    *p++ = 0xd0;
}

void link(CompilerState& state, const LinkDesc& desc)
{
    StackMaps sm;
    DataView dv(state.m_stackMapsSection->data());
    sm.parse(&dv);
    auto rm = sm.computeRecordMap();
    EMASSERT(state.m_codeSectionList.size() == 1);
    uint8_t* prologue = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(state.m_codeSectionList.front().data()));
    uint8_t* body = static_cast<uint8_t*>(state.m_entryPoint);
    desc.m_patchPrologue(desc.m_opaque, prologue);
    for (auto& record : rm) {
        EMASSERT(record.second.size() == 1);
        auto found = state.m_patchMap.find(record.first);
        EMASSERT(found != state.m_patchMap.end());
        PatchDesc& patchDesc = found->second;
        switch (patchDesc.m_type) {
        case PatchType::Assist: {
            desc.m_patchAssist(desc.m_opaque, body + record.second[0].instructionOffset, desc.m_dispAssist);
        } break;
        case PatchType::TcgDirect: {
            auto& recordUnit = record.second[0];
            desc.m_patchTcgDirect(desc.m_opaque, body + recordUnit.instructionOffset, desc.m_dispTcgDirect);
        } break;
        case PatchType::TcgIndirect: {
            auto& recordUnit = record.second[0];
            desc.m_patchTcgIndirect(desc.m_opaque, body + recordUnit.instructionOffset, desc.m_dispTcgIndirect);
        } break;
        case PatchType::TcgHelperNotReturn: {
        } break;
        case PatchType::TcgHelper: {
            handleTcgHelper(desc, record.second[0], body, patchDesc.m_function);
        } break;
        default:
            __builtin_unreachable();
        }
    }
}
}
