// Synthetic Windows lifetime bug. --crash is deliberately unsafe and opt-in.
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include "worker.hpp"
#include <future>
#include <iostream>
#include <string_view>

LONG WINAPI capture_dump(EXCEPTION_POINTERS* exception) {
    HANDLE file = CreateFileW(L"lifetime-crash.dmp", GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
        const BOOL saved = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                                             MiniDumpNormal, &info, nullptr, nullptr);
        CloseHandle(file);
        if (saved) std::cerr << "Saved lifetime-crash.dmp\n";
        else std::cerr << "Dump failed: " << GetLastError() << '\n';
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

__declspec(noinline) void acquire_sample(volatile LONG* sample) {
    // In --crash mode the page has been released before the gate opens.
    *sample = 42;
}

int main(int argc, char** argv) {
    const bool crash = argc == 2 && std::string_view(argv[1]) == "--crash";
    if (argc != 2 || (!crash && std::string_view(argv[1]) != "--fixed")) {
        std::cerr << "Usage: lifetime_repro --fixed | --crash\n";
        return 2;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(capture_dump);
    auto* sample = static_cast<volatile LONG*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!sample) return 3;
    std::promise<void> release;
    auto gate = release.get_future().share();
    demo::Worker worker(1);
    worker.submit([sample, gate] { gate.wait(); acquire_sample(sample); });
    if (crash) {
        // Deliberately invert the ownership order; the gate makes this deterministic.
        VirtualFree(const_cast<LONG*>(sample), 0, MEM_RELEASE);
        release.set_value();
        worker.close(); // worker faults before it can join
        return 4;
    }
    release.set_value();
    worker.close(); // drain and join while the captured storage remains valid
    const bool correct = *sample == 42;
    VirtualFree(const_cast<LONG*>(sample), 0, MEM_RELEASE);
    std::cout << "Joined before release; sample=" << (correct ? 42 : 0) << '\n';
    return correct ? 0 : 5;
}
