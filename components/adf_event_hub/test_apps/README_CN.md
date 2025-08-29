# adf_event_hub 测试工程

[English](./README.md)

统一的单元测试框架。[`common/`](common/) 下的一套测试源码被编译两次：作为
ESP 目标固件（本目录即 IDF `test_apps` 工程，入口在 [`main/`](main/)）和作为
PC 本地可执行文件（[`pc/`](pc/) 通过 `FetchContent` 拉取 FreeRTOS POSIX +
Unity）。基准测试位于 [`pc/bench/`](pc/bench/)，是独立可执行文件。

## 构建与运行

### PC

```bash
cd test_apps/pc
cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build --output-on-failure --output-junit test-results.xml
```

期望结果：`67 Tests 0 Failures 0 Ignored`，JUnit XML 位于 `build/test-results.xml`。

直接运行可执行文件获取完整 Unity 日志：

```bash
./build/test_event_hub
```

### PC Sanitizer / 覆盖率（仅 PC）

```bash
# Sanitizer：可选值 address | undefined | thread
cmake -S . -B build_asan -DADF_EH_SANITIZER=address && cmake --build build_asan
ctest --test-dir build_asan

# 覆盖率（gcov）
cmake -S . -B build_cov  -DADF_EH_COVERAGE=ON && cmake --build build_cov
./build_cov/test_event_hub
lcov --capture --directory build_cov --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

参考数量级：仅运行 67 个功能用例，`adf_event_hub.c` 行覆盖率约 65%。IDF 目标构建永不插桩。

### PC 基准（默认不跑，需显式开启）

```bash
ctest --test-dir build -C Bench            # 或：./build/bench/bench_event_hub
```

### ESP 目标

```bash
. $IDF_PATH/export.sh
cd test_apps
idf.py set-target esp32s3          # 或其他支持的目标
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

所有用例跑完后 Unity 任务会打印 `<<< ADF_EVENT_HUB_UT_DONE >>>`。

## 目录结构

```
test_apps/
├── CMakeLists.txt      # IDF 工程根
├── main/               # IDF 入口：app_main.c + 组件注册
├── pc/                 # PC 构建：main.c、FreeRTOSConfig.h、test_compat.h、bench/
└── common/             # 共享测试用例 + test_port.h + fixtures + threading 辅助
```

## 测试覆盖了什么

67 个 Unity 用例，分布在 13 个分组，全部带 `[adf_event_hub]` 标签：

| 文件                    | 关注点                                                                          |
|-------------------------|--------------------------------------------------------------------------------|
| `test_create_destroy.c` | `adf_event_hub_create` / `destroy` 参数校验与生命周期                          |
| `test_subscribe.c`      | `adf_event_hub_subscribe` 参数边界，以及目标域自动创建                         |
| `test_callback.c`       | 同步回调投递、event_id 过滤、多订阅者扇出                                      |
| `test_queue.c`          | 异步队列投递、最后一次引用触发的 payload 释放、回调 + 队列混合场景             |
| `test_release_cb.c`     | `release_cb` 生命周期：立即触发 vs 延后到最后一次 `delivery_done`              |
| `test_unsubscribe.c`    | 订阅移除（精确匹配、通配、错误路径）                                           |
| `test_publish.c`        | `adf_event_hub_publish` 参数边界                                               |
| `test_queue_full.c`     | 队列满丢弃语义，以及订阅者之间的隔离                                           |
| `test_multi_domain.c`   | 跨域隔离、销毁某一个域不影响其他域                                             |
| `test_stress.c`         | 单线程压力：高订阅者数 / 高事件量 / envelope 池饱和                            |
| `test_reentrant.c`      | 多任务重入、并发 subscribe/publish、release 回调竞态                           |
| `test_order.c`          | 启动顺序（先 subscribe 后 create、先 publish 后 subscribe 等竞态）             |
| `test_snap.c`           | 高订阅者数下的快照扩展性与并发发布                                             |

另外 `pc/bench/bench_event_hub.c` 中的八项基准覆盖：发布延迟随订阅者数量、通配订阅比例、domain 查找、envelope 往返开销、多域锁开销、栈占用、free-list 扩展性以及 subscriber_head 早退速度。结果以对比表形式输出，不对绝对耗时做断言。

## 新增测试用例

在 `common/` 下新建 `test_<topic>.c`，`#include "test_port.h"`（自动按 IDF / PC 选择正确的 FreeRTOS + Unity 头文件），然后写 `TEST_CASE("description", TEST_TAG) { ... }` 即可。两边 CMake 都是 `glob test_*.c`，无需改动。
