# Unit testing strategy

## Initial implementation

The first host test target uses CMake + CTest and a small assertion runner. Boot logic depends on callback interfaces rather than direct flash, UART, crypto or RTOS calls. `tests/mocks/mock_swap_backend.*` provides a fake with per-sector slot state, a verification that rejects mixed pairs, corrupt-image flags and power-cut injection at any numbered side-effecting operation (journal write, sector swap, commit).

The slot-swap tests cut power at every operation of the forward swap, the revert and the confirmation, reboot, and assert that the result is always a whole pair: committed or trialled new pair, or reverted previous pair, never a mixed one.

The logging tests cover the record format (timestamp, one line per record, truncation), level filtering, immediate and deferred backends, ring-buffer overflow and its report, file rotation, the boot counter, write failures and the dump, using the in-memory file system `tests/mocks/fake_log_fs.*`. `scripts/rt1180_logs.py selftest` checks the PC-side UART and littlefs parsers.

CI builds and runs the host tests with GCC and Clang, with ASan/UBSan, and with gcov instrumentation. The `host-coverage` artifact contains the gcovr report. No coverage threshold is enforced yet.

Run:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

On a machine without gcc or clang (for example the Windows development PC), install the developer tools into the project `.venv` and use the helper, which falls back to `zig cc`:

```sh
.venv/Scripts/python -m pip install -r requirements-dev.txt
python scripts/host_tests.py
```

`requirements-dev.txt` also pins `clang-format`. A single style, defined by `.clang-format` (Linux brace style), applies to all project C code, examples included; CI fails on unformatted C files. In Claude Code sessions the project hooks (`.claude/settings.json`) format every edited C file and re-run the host tests automatically.

## Recommended next tools

- **Unity + CMock + Ceedling**: recommended default for embedded C unit tests. Unity provides a small C assertion framework; CMock generates mocks from C headers; Ceedling coordinates build, test discovery, fixtures and reports.
- **CMake + CTest**: retain as the top-level build/test entry point, including host tests and target test registration.
- **Fake Function Framework (FFF)**: alternative lightweight hand-written/generated fakes when a full Ceedling workflow is not desired.
- **gcovr**: line/branch coverage for host-testable logic; publish Cobertura XML and HTML reports.
- **clang-tidy / clang-format**: static checks and formatting in CI.
- **AddressSanitizer + UndefinedBehaviorSanitizer**: host test builds to expose memory and undefined-behavior defects.
- **Renode or QEMU**: useful only where the RT118x peripheral/core model is sufficiently complete; do not treat simulation as hardware qualification.
- **Hardware-in-the-loop**: board tests for FlexSPI NOR erase/program/power interruption, TRDC, TCM ECC, CM7 startup, watchdog and dual-core rollback.

## CI stages

1. Configure/build host test target with GCC and Clang.
2. Run unit tests and publish JUnit/CTest results.
3. Run ASan/UBSan build.
4. Run coverage and enforce a gradually increasing threshold for pure logic.
5. Run formatting, static analysis and dependency/security scans.
6. Build each board/core/configuration when the cross toolchain and pinned MCUXpresso SDK are integrated.
7. Run HIL tests on dedicated hardware; keep signing keys off generic hosted runners.

## Test design rules

- Test pure functions directly.
- Inject interfaces for flash, crypto verification, clocks, IPC, UART and persistent storage.
- Provide fakes for normal behavior and fault injection for timeout, I/O failure, corrupt data, power loss and partial writes.
- Test every state transition and every recovery point.
- Never claim flash power-fail safety based only on host mocks; hardware power-cut testing is mandatory.
