#ifndef IRCONTEXTINTERNAL_H
#define IRCONTEXTINTERNAL_H
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <stdint.h>
#include "IRContext.h"
#include "RegisterInit.h"

struct IRContextInternal : IRContext {
    typedef std::vector<RegisterInit> RegisterInitVector;
    typedef std::vector<std::unique_ptr<class Check> > CheckVector;
    RegisterInitVector m_registerInit;
    CheckVector m_checks;
    bool m_thumb;
    IRContextInternal();
};

#endif /* IRCONTEXTINTERNAL_H */
