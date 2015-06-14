#ifndef LINK_H
#define LINK_H
namespace jit {
struct LinkDesc {
    void* m_opaque;
    void* m_dispDirect;
    void* m_dispDirectSlow;
    void* m_dispIndirect;
    void* m_dispAssist;
    void (*m_patchPrologue)(void* opaque, uint8_t* start);
    void (*m_patchDirect)(void* opaque, uint8_t* toFill, void*);
    void (*m_patchIndirect)(void* opaque, uint8_t* toFill, void*);
    void (*m_patchAssist)(void* opaque, uint8_t* toFill, void*);
};

void link(CompilerState& state, const LinkDesc& desc);
}
#endif /* LINK_H */
