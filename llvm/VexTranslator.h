#ifndef VEXTRANSLATOR_H
#define VEXTRANSLATOR_H
#include "VexHeaders.h"

namespace jit {

struct VexTranslatorEnv {
    void* m_dispDirect;
    void* m_dispDirectSlow;
    void* m_dispIndirect;
    void* m_dispAssist;
    uintptr_t m_maxga;
    size_t m_contextSize;
};

class VexTranslator {
public:
    explicit VexTranslator();
    virtual ~VexTranslator();

    VexTranslator(const VexTranslator&) = delete;
    const VexTranslator& operator=(const VexTranslator&) = delete;

    virtual bool translate(IRSB*, const VexTranslatorEnv& env) = 0;
    inline const void* code() const { return m_code; }
    inline size_t codeSize() const { return m_codeSize; }
    static bool init();
    static VexTranslator* create();

protected:
    inline void setCode(const void* code) { m_code = code; }
    inline void setCodeSize(size_t size) { m_codeSize = size; }

private:
    const void* m_code;
    size_t m_codeSize;
};
}
#endif /* VEXTRANSLATOR_H */
