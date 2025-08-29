# adf_event_hub Test Apps

[中文版](./README_CN.md)

Unified unit-test harness. A single set of test bodies in [`common/`](common/)
is compiled twice: as an ESP target firmware (this directory is an IDF
`test_apps` project whose entry lives in [`main/`](main/)) and as a native PC
executable ([`pc/`](pc/) uses `FetchContent` to pull FreeRTOS POSIX + Unity).
Benchmarks live in [`pc/bench/`](pc/bench/) as a separate executable.

## Build and Run

### PC

```bash
cd test_apps/pc
cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build --output-on-failure --output-junit test-results.xml
```

Expected: `67 Tests 0 Failures 0 Ignored`. JUnit XML at `build/test-results.xml`.

Run directly for the full Unity log:

```bash
./build/test_event_hub
```

### PC with sanitizer / coverage (PC-only)

```bash
# Sanitizer: values = address | undefined | thread
cmake -S . -B build_asan -DADF_EH_SANITIZER=address && cmake --build build_asan
ctest --test-dir build_asan

# Coverage (gcov)
cmake -S . -B build_cov  -DADF_EH_COVERAGE=ON && cmake --build build_cov
./build_cov/test_event_hub
lcov --capture --directory build_cov --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

Baseline: ~65 % line coverage on `adf_event_hub.c` from the 67 functional tests alone. IDF target build is never instrumented.

### PC bench (explicit, off by default)

```bash
ctest --test-dir build -C Bench            # or: ./build/bench/bench_event_hub
```

### ESP target

```bash
. $IDF_PATH/export.sh
cd test_apps
idf.py set-target esp32s3          # any supported target
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

The Unity task emits `<<< ADF_EVENT_HUB_UT_DONE >>>` when all cases complete.

## Layout

```
test_apps/
├── CMakeLists.txt      # IDF project root
├── main/               # IDF entry: app_main.c + component registration
├── pc/                 # PC build: main.c, FreeRTOSConfig.h, test_compat.h, bench/
└── common/             # Shared test bodies + test_port.h + fixtures + threading helpers
```

## What Is Tested

67 Unity cases across 13 groups, all tagged `[adf_event_hub]`:

| File                    | Focus                                                                                |
|-------------------------|--------------------------------------------------------------------------------------|
| `test_create_destroy.c` | `adf_event_hub_create` / `destroy` argument validation and lifecycle                 |
| `test_subscribe.c`      | `adf_event_hub_subscribe` argument boundaries and target-domain auto-create          |
| `test_callback.c`       | Synchronous callback delivery, event-id filter, multi-subscriber fan-out             |
| `test_queue.c`          | Async queue delivery, payload release on last reference, mixed callback + queue      |
| `test_release_cb.c`     | `release_cb` lifecycle: immediate vs deferred to last `delivery_done`                |
| `test_unsubscribe.c`    | Subscription removal (exact match, wildcard, error paths)                            |
| `test_publish.c`        | `adf_event_hub_publish` argument boundaries                                          |
| `test_queue_full.c`     | Full-queue drop semantics; isolation between subscribers                             |
| `test_multi_domain.c`   | Per-domain isolation, safe destruction of one domain                                 |
| `test_stress.c`         | Single-threaded stress: high subscriber/event counts, envelope pool saturation       |
| `test_reentrant.c`      | Multi-task reentrancy, concurrent subscribe/publish, release-callback races          |
| `test_order.c`          | Startup ordering (subscribe-before-create, publish-before-subscribe, races)          |
| `test_snap.c`           | Snapshot scalability for high subscriber counts and concurrent publishers            |

Plus eight benchmarks in `pc/bench/bench_event_hub.c` covering publish latency vs subscriber count, wildcard ratio, domain-find cost, envelope round-trip, multi-domain lock overhead, stack footprint, free-list scalability, and early-out speedup. These are printed as comparison tables without absolute-time assertions.

## Adding a Test Case

Drop a `common/test_<topic>.c` including `test_port.h` (picks FreeRTOS + Unity headers for IDF vs PC) and use `TEST_CASE("description", TEST_TAG) { ... }`. Both builds glob `test_*.c`, so no CMake edits needed.
