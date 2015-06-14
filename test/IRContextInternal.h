#ifndef IRCONTEXTINTERNAL_H
#define IRCONTEXTINTERNAL_H
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <stdint.h>
#include "IRContext.h"
#include "RegisterInit.h"
#include "VexHeaders.h"

struct IRContextInternal : IRContext {
    typedef std::vector<RegisterInit> RegisterInitVector;
    typedef std::vector<std::unique_ptr<class Check> > CheckVector;
    typedef std::unordered_map<std::string, IRTemp> TmpMap;
    typedef std::unique_ptr<TmpMap> TempMapPtr;
    RegisterInitVector m_registerInit;
    CheckVector m_checks;
    TempMapPtr m_tempMap;
    IRSB* m_irsb;
    bool m_novex;

    IRContextInternal();
    inline TmpMap& getTempMap()
    {
        if (!m_tempMap) {
            m_tempMap.reset(new TmpMap);
        }
        return *m_tempMap;
    }
};

#endif /* IRCONTEXTINTERNAL_H */
