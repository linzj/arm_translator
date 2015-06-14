#include "IntrinsicRepository.h"

namespace jit {

IntrinsicRepository::IntrinsicRepository(LContext context, LModule module)
    : CommonValues(context)
#define INTRINSIC_INITIALIZATION(ourName, llvmName, type) , m_##ourName(0)
          FOR_EACH_FTL_INTRINSIC(INTRINSIC_INITIALIZATION)
#undef INTRINSIC_INITIALIZATION
{
    initialize(module);
}

#define INTRINSIC_GETTER_SLOW_DEFINITION(ourName, llvmName, type)  \
    LValue IntrinsicRepository::ourName##IntrinsicSlow()           \
    {                                                              \
        m_##ourName = addExternFunction(m_module, llvmName, type); \
        return m_##ourName;                                        \
    }
FOR_EACH_FTL_INTRINSIC(INTRINSIC_GETTER_SLOW_DEFINITION)
#undef INTRINSIC_GETTER
}
