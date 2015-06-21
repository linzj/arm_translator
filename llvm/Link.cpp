#include <bitset>
#include "X86Assembler.h"
#include "StackMaps.h"
#include "CompilerState.h"
#include "Abbreviations.h"
#include "Link.h"
#include "log.h"

namespace jit {
#define CALL_USED_REGISTERS					\
/*ax,dx,cx,bx,si,di,bp,sp,st,st1,st2,st3,st4,st5,st6,st7*/	\
{  1, 1, 1, 0, 4, 4, 0, 1, 1,  1,  1,  1,  1,  1,  1,  1,	\


static void pushOnStack(JSC::X86Assembler& assembler, const StackMaps::Location& location)
{
    switch (location.kind) {
    case StackMaps::Location::Register: {
        assembler.push_r(static_cast<JSC::X86Registers::RegisterID>(location.dwarfReg.dwarfRegNum()));
    } break;
    case StackMaps::Location::Direct: {
        assembler.push_r(static_cast<JSC::X86Registers::RegisterID>(location.dwarfReg.dwarfRegNum()));
        assembler.addl_im(location.offset, 0, JSC::X86Registers::esp);
    } break;
    case StackMaps::Location::Indirect: {
        assembler.push_m(location.offset, static_cast<JSC::X86Registers::RegisterID>(location.dwarfReg.dwarfRegNum()));
    } break;
    case StackMaps::Location::Constant: {
        assembler.push_i32(location.offset);
    } break;
    default:
        EMASSERT("unsupported location" && false);
    }
}

// this function return a filtered liveouts, with all not CALL USED
// registers filtered out.
// This is because the CALL USED registers will not be saved by
// the helper functions, but the not CALL USED will.
static std::bitset<8> filterOutputLiveouts(const std::vector<StackMaps::LiveOut>& liveouts)
{
    std::bitset<8> bs;
    for (auto& liveout : liveouts) {
        int num = liveout.dwarfReg.dwarfRegNum();
        if (num >= 8) {
            EMASSERT("not supported register" && false);
        }
        switch (num) {
            case JSC::X86Registers::ebp:
            case JSC::X86Registers::ebx:
            case JSC::X86Registers::esi:
            case JSC::X86Registers::edi:
                continue;
            default:
                bs.set(num);
        }
    }
    return bs;
}

static void handleTcgHelper(const LinkDesc& desc, const StackMaps::Record& record, uint8_t* body, bool is64Ret)
{
    uint8_t* p = body + record.instructionOffset;
    JSC::X86Assembler assembler;
    // calculate align values
    // argumentCount = argument
    int argumentCount = record.locations.size() - 2;
    auto liveoutsBitset = filterOutputLiveouts(record.liveOuts);
    int liveOutCount = liveoutsBitset.count();
    // add one more for return address.
    int saveStackSize = (liveOutCount + argumentCount + 1) * sizeof(intptr_t);
    const int alignment = 16;
    int saveStackSizeRound = (saveStackSize + alignment - 1) & (~(alignment - 1));
    int subtraction = saveStackSizeRound - saveStackSize;
    if (subtraction) {
        assembler.subl_ir(subtraction, JSC::X86Registers::esp);
    }
    // push the live outs
    for (int i = static_cast<int>(liveoutsBitset.size()) - 1; i >= 0; --i) {
        if (liveoutsBitset.test(i)) {
            assembler.push_r(static_cast<JSC::X86Registers::RegisterID>(i));
        }
    }
    // push the return value pointer
    pushOnStack(assembler, record.locations[1]);
    // push the argument
    for (int i = static_cast<int>(record.locations.size()) - 1; i >= 2; --i) {
        auto& location = record.locations[i];
        pushOnStack(assembler, location);
    }
    // call the function
    auto& callLocation = record.locations[0];
    switch (callLocation.kind) {
    case StackMaps::Location::Register: {
        assembler.call(static_cast<JSC::X86Registers::RegisterID>(callLocation.dwarfReg.dwarfRegNum()));
    } break;
    case StackMaps::Location::Indirect: {
        assembler.call_m(callLocation.offset, static_cast<JSC::X86Registers::RegisterID>(callLocation.dwarfReg.dwarfRegNum()));
    } break;
    default:
        EMASSERT("unsupported location" && false);
    }
    // add esp back
    assembler.addl_ir(argumentCount * sizeof(intptr_t), JSC::X86Registers::esp);
    // pop the return address pointer to ecx
    assembler.pop_r(JSC::X86Registers::ecx);
    assembler.movl_rm(JSC::X86Registers::eax, 0, JSC::X86Registers::ecx);
    if (is64Ret) {
        assembler.movl_rm(JSC::X86Registers::edx, sizeof(intptr_t), JSC::X86Registers::ecx);
    }
    // pop the live outs
    for (int i = 0; i < static_cast<int>(liveoutsBitset.size()); ++i) {
        if (liveoutsBitset.test(i)) {
            assembler.pop_r(static_cast<JSC::X86Registers::RegisterID>(i));
        }
    }
    if (subtraction) {
        assembler.addl_ir(subtraction, JSC::X86Registers::esp);
    }
    // save the returns
    auto& returnStorageLocation = record.locations[1];
    EMASSERT(returnStorageLocation.kind == StackMaps::Location::Register);
    static const int patchMax = 32;
    EMASSERT(assembler.buffer().codeSize() <= patchMax);
    // fill the code.
    memcpy(p, assembler.buffer().data(), assembler.buffer().codeSize());
    p += assembler.buffer().codeSize();
    // fill the rest will nops
    auto nopsSize = std::distance(p, body + record.instructionOffset + patchMax);
    JSC::X86Assembler::fillNops(p, nopsSize);
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
        case PatchType::TcgHelper64: {
            handleTcgHelper(desc, record.second[0], body, true);
        } break;
        case PatchType::TcgHelper32: {
            handleTcgHelper(desc, record.second[0], body, false);
        } break;
        default:
            __builtin_unreachable();
        }
    }
}
}
