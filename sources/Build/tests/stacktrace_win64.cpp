#include <cassert>
#include <sstream>
#include "Stacktrace.h"

int main()
{
    static_assert(sizeof(void*) == 8, "Run from the AMD64 native tools prompt");
    RuntimeStack stack;
    std::ostringstream output;
    output << stack;
    assert(output.str().find("0x") != std::string::npos);
    // The formatter must retain at least one full process address above 4 GiB.
    const auto address = output.str().find("0x");
    const auto end = output.str().find_first_not_of("0123456789abcdefABCDEF", address + 2);
    assert((end == std::string::npos ? output.str().size() : end) - address > 10);
    TraceableException error("stacktrace regression test");
    assert(std::string(error.what()).find("stacktrace regression test") != std::string::npos);
}
