#pragma once

//forces malloc to use 16 byte alignment
class BASE_EXPORT AlignedAlloc : public Alloc
{
public:
    virtual void * __restrict _Allocate(size_t dwSize);

    virtual void * _ReAllocate(LPVOID lpData, size_t dwSize);

    virtual void   _Free(LPVOID lpData);

    virtual void   ErrorTermination();
};
