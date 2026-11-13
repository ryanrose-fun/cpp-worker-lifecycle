# Windows crash dump: worker outlives its resource

A deliberately broken synthetic example, not a client incident. A promise makes the failing schedule reproducible; only the `--crash` process crashes.

## Build and capture

Use Windows, Visual Studio C++ tools, CMake and WinDbg:

```powershell
cmake -S . -B build
cmake --build build --config Debug
powershell -ExecutionPolicy Bypass -File debugging/capture.ps1
ctest --test-dir build -C Debug --output-on-failure
```

The capture script copies the EXE and PDB together into `build/capture-TIMESTAMP`, runs the deliberate fault, checks for `lifetime-crash.dmp`, and writes a SHA-256 manifest. Keep the bundle intact. A same-named PDB is insufficient: the debugger checks the embedded PDB GUID and age. Hashes identify captured files; they do not independently establish symbol compatibility. Do not rebuild and replace just the PDB.

## Investigate in WinDbg

Open the captured dump and substitute the actual capture directory:

```text
.symfix C:\symbols
.sympath+ C:\path\to\build\capture-TIMESTAMP
.reload /f
lmvm lifetime_repro
!analyze -v
.ecxr
k
~* kb
```

Confirm private symbols and the correct image with `lmvm` before trusting source lines. If loading fails, enable `!sym noisy`, reload and inspect diagnostics; never force mismatched symbols. Microsoft symbols resolve OS frames; the local PDB resolves our code.

Expected finding: access violation `0xC0000005` on the write in `acquire_sample`. `.ecxr` selects the exception context, `k` shows the failing stack and `~* kb` shows other threads. Follow the pointer's owner in `debugging/lifetime_repro.cpp`. A minidump is not a full heap capture: absent memory is not evidence of its prior contents.

## Root cause and fix

The owner allocates a page and submits a callback holding its raw address. A promise blocks the callback. The broken path frees the page before opening the gate, so the callback writes into released memory. This models an acquisition callback outliving its resource; it does not implement MFC or hardware acquisition.

The fixed path opens the gate, calls `worker.close()` to drain and join, then frees the page. Required ordering: stop admission, finish accepted work, join, release dependencies. Submission is not completion. A sleep is not synchronisation. Shared ownership extends lifetime but does not itself protect concurrent access.

## Regression and verification

`lifetime_repro --fixed` checks the expected write before releasing the page. CTest runs it as `lifetime_fixed` with a timeout. Existing lifecycle tests also cover bounded admission, FIFO, multiple producers, exception isolation and destructor drain.

On 13 November 2026 the local MSVC Debug build succeeded, both CTest tests passed, and the crash generated a dump with exit `-1073741819` (`0xC0000005`). WinDbg was unavailable locally: the commands above are an investigation guide, not a fabricated debugger transcript. Dumps/PDBs remain in ignored build output; never publish confidential client memory.

See [queue and shutdown decisions](decisions.md).
