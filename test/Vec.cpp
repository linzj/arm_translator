#include <iostream>
#include <iomanip>
#include "Vec.h"

IntVec* createIntVec(unsigned long long val)
{
    IntVec* vec = new IntVec;
    vec->push_back(val);
    return vec;
}

IntVec* appendIntVec(IntVec* intVec, unsigned long long val)
{
    intVec->push_back(val);
    return intVec;
}

NumberVector* createNumberVec(IntVec* intVec, int type)
{
    return new NumberVector(static_cast<IntVec*>(intVec), type);
}

NumberVector::NumberVector(IntVec* vec, int type)
    : m_intVec(vec)
    , m_type(type)
{
}

std::ostream& operator<<(std::ostream& out, const NumberVector& vec)
{
    int i;
    out << " { ";
    for (i = 0; i < static_cast<int>(vec.m_intVec->size() - 1); ++i) {
        out << std::hex << (*vec.m_intVec)[i] << ", ";
    }
    if (i < vec.m_intVec->size()) {
        out << std::hex << (*vec.m_intVec)[i];
        out << " }";
    }
    else {
        out << "}";
    }
    return out;
}
