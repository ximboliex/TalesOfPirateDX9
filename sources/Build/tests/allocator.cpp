#include <cassert>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include "pi_Alloc.h"

pi_LeakReporter pi_leakReporter;
struct Throws { Throws() { throw std::runtime_error("expected"); } };
struct Tracked
{
    IMPLEMENT_CDELETE(Tracked)
    static int destructions;
    ~Tracked() { ++destructions; }
};
int Tracked::destructions = 0;

int main()
{
    void* address = pi_Alloc::allocate(37, nullptr, __FILE__, __LINE__);
    assert(reinterpret_cast<uintptr_t>(address) > UINT32_MAX);
    assert(reinterpret_cast<uintptr_t>(address) % alignof(std::max_align_t) == 0);
    assert(pi_Alloc::numBytes() == 37 && pi_Alloc::numBlocks() == 1);
    assert(pi_leakReporter.pi_leakList.front().addr == address);
    pi_Alloc::deallocate(address);
    assert(pi_leakReporter.pi_leakList.empty());
    assert(pi_Alloc::numBytes() == 0 && pi_Alloc::numBlocks() == 0);
    try { pi_Alloc::allocate((std::numeric_limits<size_t>::max)(), nullptr, __FILE__, __LINE__); assert(false); }
    catch (const std::bad_alloc&) {}
    try { cnew Throws; assert(false); }
    catch (const std::runtime_error&) {}
    assert(pi_Alloc::numBlocks() == 0 && pi_leakReporter.pi_leakList.empty());
    Tracked* tracked = cnew Tracked;
    cdestroy(Tracked, tracked);
    assert(Tracked::destructions == 1 && pi_Alloc::numBlocks() == 0);
}
