#include "Check.h"
#include "IRContextInternal.h"

IRContextInternal::IRContextInternal()
    : m_novex(false)
{
    m_irsb = emptyIRSB();
}
