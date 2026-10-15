// x64 implementation for RuntimeStack; the legacy symbol engine uses DWORD addresses.
#include <dbghelp.h>
#include <iomanip>
#include <mutex>
#pragma comment(lib, "dbghelp.lib")

void printStacktrace(EXCEPTION_POINTERS* ep, std::ostream& os)
{
    if (!ep || !ep->ContextRecord) return;
    static std::mutex mutex;
    const std::lock_guard<std::mutex> lock(mutex);
    const auto process = GetCurrentProcess();
    // DbgHelp may already be initialized by another diagnostic component.
    static const bool initialized = SymInitialize(process, nullptr, TRUE) != FALSE;
    (void)initialized;
    CONTEXT context = *ep->ContextRecord;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
    const auto oldFlags = os.flags();
    for (unsigned depth = 0; depth < 128 && frame.AddrPC.Offset; ++depth) {
        os << "0x" << std::hex << frame.AddrPC.Offset << '\n';
        const auto oldPC = frame.AddrPC.Offset;
        const auto oldSP = frame.AddrStack.Offset;
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(),
                         &frame, &context, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)) break;
        if (frame.AddrPC.Offset == oldPC && frame.AddrStack.Offset == oldSP) break;
    }
    os.flags(oldFlags);
}
