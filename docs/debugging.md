# Windows debugging walkthrough

Build Debug with MSVC and open the generated solution in Visual Studio. Set breakpoints in `submit`, `run` and `close`. Run the tests and inspect Threads/Parallel Stacks while the bounded-queue test deliberately waits on its promise.

Confirm that the callback runs outside the mutex and that `close` wakes the consumer, drains accepted tasks and joins it before the owner reads results. The exception-isolation test intentionally throws; enabling break-on-throw helps inspect that path.

For a real crash dump, preserve the exact EXE, DLLs and matching PDBs from the failing build. Open the dump in WinDbg, configure the build's symbol directory, run `.reload`, inspect `lm` for loaded modules/symbol status, then `!analyze -v`, `.ecxr`, `k` and `~* kb`. A source line alone is not a root cause: check object lifetime, ownership and other threads against the exception context.

This repository does not ship a crash dump or claim to reproduce a client incident. It provides a safe lifecycle example and a debugging procedure. Never publish dumps containing client memory or secrets.
