#ifndef EXECUTABLEMEMORYALLOCATOR_H
#define EXECUTABLEMEMORYALLOCATOR_H
namespace jit {
class ExecutableMemoryAllocator {
public:
    ExecutableMemoryAllocator() {}
    virtual ~ExecutableMemoryAllocator() {}
    ExecutableMemoryAllocator(const ExecutableMemoryAllocator&) = delete;
    ExecutableMemoryAllocator& operator=(const ExecutableMemoryAllocator&) = delete;
    virtual void* allocate(int size, int align) = 0;
};
}
#endif /* EXECUTABLEMEMORYALLOCATOR_H */
