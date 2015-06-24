#ifndef VEC_H
#define VEC_H
#include <vector>
#include <memory>
#include <iosfwd>

typedef std::vector<unsigned long long> IntVec;
struct NumberVector {
    NumberVector() = delete;
    NumberVector(IntVec* vec, int type);
    std::unique_ptr<IntVec> m_intVec;
    int m_type;
};

NumberVector* createNumberVec(IntVec* intVec, int type);
IntVec* appendIntVec(IntVec* intVec, unsigned long long val);
IntVec* createIntVec(unsigned long long val);
std::ostream& operator<<(std::ostream& out, const NumberVector& vec);
#endif /* VEC_H */
