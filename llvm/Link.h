#ifndef LINK_H
#define LINK_H
namespace jit {
struct LinkDesc {
    void* m_opaque;
    void* m_dispTcgDirect;
    void* m_dispTcgIndirect;
    void (*m_patchPrologue)(void* opaque, uint8_t* start);
    void (*m_patchAssist)(void* opaque, uint8_t* toFill, void*);
    void (*m_patchTcgDirect)(void* opaque, uint8_t* toFill, void*);
    void (*m_patchTcgIndirect)(void* opaque, uint8_t* toFill, void*);
    uint8_t* (*m_patchMovRegToMem)(void* opaque, uint8_t* toFill);
    uint8_t* (*m_patchMovMemToMem)(void* opaque, uint8_t* toFill);
};

void link(CompilerState& state, const LinkDesc& desc);
}
#endif /* LINK_H */
