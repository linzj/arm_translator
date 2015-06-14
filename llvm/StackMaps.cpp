#include "StackMaps.h"
namespace jit {

Reg DWARFRegister::reg() const
{
#if __x86_64__
    if (m_dwarfRegNum >= 0 && m_dwarfRegNum < 16) {
        switch (dwarfRegNum()) {
        case 0:
            return AMD64::RAX;
        case 1:
            return AMD64::RDX;
        case 2:
            return AMD64::RCX;
        case 3:
            return AMD64::RBX;
        case 4:
            return AMD64::RSI;
        case 5:
            return AMD64::RDI;
        case 6:
            return AMD64::RBP;
        case 7:
            return AMD64::RSP;
        default:
            // Registers r8..r15 are numbered sensibly.
            return static_cast<Reg>(m_dwarfRegNum);
        }
    }
    if (m_dwarfRegNum >= 17 && m_dwarfRegNum <= 32)
        return static_cast<FPRReg>(m_dwarfRegNum - 17);
    return Reg();
#else
#error unsupported arch.
#endif
}

template <typename T>
T readObject(StackMaps::ParseContext& context)
{
    T result;
    result.parse(context);
    return result;
}

void StackMaps::Constant::parse(StackMaps::ParseContext& context)
{
    integer = context.view->read<int64_t>(context.offset, true);
}

void StackMaps::StackSize::parse(StackMaps::ParseContext& context)
{
    switch (context.version) {
    case 0:
        functionOffset = context.view->read<uint32_t>(context.offset, true);
        size = context.view->read<uint32_t>(context.offset, true);
        break;

    default:
        functionOffset = context.view->read<uint64_t>(context.offset, true);
        size = context.view->read<uint64_t>(context.offset, true);
        break;
    }
}

void StackMaps::Location::parse(StackMaps::ParseContext& context)
{
    kind = static_cast<Kind>(context.view->read<uint8_t>(context.offset, true));
    size = context.view->read<uint8_t>(context.offset, true);
    dwarfReg = DWARFRegister(context.view->read<uint16_t>(context.offset, true));
    this->offset = context.view->read<int32_t>(context.offset, true);
}

void StackMaps::LiveOut::parse(StackMaps::ParseContext& context)
{
    dwarfReg = DWARFRegister(context.view->read<uint16_t>(context.offset, true)); // regnum
    context.view->read<uint8_t>(context.offset, true); // reserved
    size = context.view->read<uint8_t>(context.offset, true); // size in bytes
}

bool StackMaps::Record::parse(StackMaps::ParseContext& context)
{
    int64_t id = context.view->read<int64_t>(context.offset, true);
    assert(static_cast<int32_t>(id) == id);
    patchpointID = static_cast<uint32_t>(id);
    if (static_cast<int32_t>(patchpointID) < 0)
        return false;

    instructionOffset = context.view->read<uint32_t>(context.offset, true);
    flags = context.view->read<uint16_t>(context.offset, true);

    unsigned length = context.view->read<uint16_t>(context.offset, true);
    while (length--)
        locations.push_back(readObject<Location>(context));

    if (context.version >= 1)
        context.view->read<uint16_t>(context.offset, true); // padding

    unsigned numLiveOuts = context.view->read<uint16_t>(context.offset, true);
    while (numLiveOuts--)
        liveOuts.push_back(readObject<LiveOut>(context));

    if (context.version >= 1) {
        if (context.offset & 7) {
            assert(!(context.offset & 3));
            context.view->read<uint32_t>(context.offset, true); // padding
        }
    }

    return true;
}

RegisterSet StackMaps::Record::locationSet() const
{
    RegisterSet result;
    for (unsigned i = locations.size(); i--;) {
        Reg reg = locations[i].dwarfReg.reg();
        result.set(reg.val() << (reg.isFloat() ? 32 : 0));
    }
    return result;
}

RegisterSet StackMaps::Record::liveOutsSet() const
{
    RegisterSet result;
    for (unsigned i = liveOuts.size(); i--;) {
        LiveOut liveOut = liveOuts[i];
        Reg reg = liveOut.dwarfReg.reg();
        // FIXME: Either assert that size is not greater than sizeof(pointer), or actually
        // save the high bits of registers.
        // https://bugs.webkit.org/show_bug.cgi?id=130885
        result.set(reg.val() << (reg.isFloat() ? 32 : 0));
    }
    return result;
}

static void merge(RegisterSet& dst, const RegisterSet& input)
{
    for (size_t i = 0; i < input.size(); ++i) {
        dst.set(i, dst.test(i) ^ input.test(i) ^ dst.test(i));
    }
}

RegisterSet StackMaps::Record::usedRegisterSet() const
{
    RegisterSet result;
    merge(result, locationSet());
    merge(result, liveOutsSet());
    return result;
}

bool StackMaps::parse(DataView* view)
{
    ParseContext context;
    context.offset = 0;
    context.view = view;

    version = context.version = context.view->read<uint8_t>(context.offset, true);

    context.view->read<uint8_t>(context.offset, true); // Reserved
    context.view->read<uint8_t>(context.offset, true); // Reserved
    context.view->read<uint8_t>(context.offset, true); // Reserved

    uint32_t numFunctions = 0;
    uint32_t numConstants = 0;
    uint32_t numRecords = 0;

    numFunctions = context.view->read<uint32_t>(context.offset, true);
    if (context.version >= 1) {
        numConstants = context.view->read<uint32_t>(context.offset, true);
        numRecords = context.view->read<uint32_t>(context.offset, true);
    }
    while (numFunctions--)
        stackSizes.push_back(readObject<StackSize>(context));

    if (!context.version)
        numConstants = context.view->read<uint32_t>(context.offset, true);
    while (numConstants--)
        constants.push_back(readObject<Constant>(context));

    if (!context.version)
        numRecords = context.view->read<uint32_t>(context.offset, true);
    while (numRecords--) {
        Record record;
        if (!record.parse(context))
            return false;
        records.push_back(record);
    }

    return true;
}

StackMaps::RecordMap StackMaps::computeRecordMap() const
{
    RecordMap result;
    for (unsigned i = records.size(); i--;)
        result.insert(std::make_pair(records[i].patchpointID, std::vector<Record>())).first->second.push_back(records[i]);
    return result;
}

unsigned StackMaps::stackSize() const
{
    assert(stackSizes.size() == 1);

    return stackSizes[0].size;
}
}
