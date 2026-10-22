// AMD64 stack walking uses the 64-bit DbgHelp API and the captured exception
// context. The original x86 walker remains available for Win32 builds.
#if defined(_WIN64)
#include "Stacktrace.h"
#include <dbghelp.h>
#include <iomanip>
#include <mutex>

#pragma comment(lib, "dbghelp.lib")

void printStacktrace(EXCEPTION_POINTERS* exception, std::ostream& output)
{
    if (!exception || !exception->ContextRecord) return;

    // DbgHelp is single threaded even when looking up different addresses.
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    const HANDLE process = GetCurrentProcess();
    static const bool initialized = SymInitialize(process, NULL, TRUE) == TRUE;
    const std::ios::fmtflags flags = output.flags();
    CONTEXT context = *exception->ContextRecord;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;

    for (unsigned index = 0; index < 64 && frame.AddrPC.Offset; ++index)
    {
        const DWORD64 pc = frame.AddrPC.Offset;
        const DWORD64 sp = frame.AddrStack.Offset;
        output << "0x" << std::hex << pc;
        if (initialized)
        {
            alignas(SYMBOL_INFO) unsigned char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            if (SymFromAddr(process, pc, &displacement, symbol))
                output << " " << symbol->Name << "+0x" << displacement;
            IMAGEHLP_LINE64 line = {};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, pc, &lineDisplacement, &line))
                output << " (" << line.FileName << ':' << std::dec << line.LineNumber << ')';
        }
        output << '\n';
        if (!initialized || !StackWalk64(IMAGE_FILE_MACHINE_AMD64, process,
                GetCurrentThread(), &frame, &context, NULL,
                SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
        if (frame.AddrPC.Offset == pc && frame.AddrStack.Offset == sp) break;
    }
    output.flags(flags);
}
#endif
