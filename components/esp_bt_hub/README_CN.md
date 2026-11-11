# ESP 蓝牙 Hub

- [English](./README.md)

`esp_bt_hub` 集中管理 ESP 蓝牙控制器和一个选定的主机协议栈，主机协议栈可以是 Bluedroid 或 NimBLE。组件统一管理 hub 生命周期、Generic Access Profile（GAP）事件分发、配对策略、设备发现、扫描、广播和资源仲裁，避免多个蓝牙应用模块分别初始化或控制同一个控制器。

应用程序只初始化该组件一次。蓝牙音频、PAN 等应用模块分别打开具名 client，并按需取得租约（lease）。

## 功能特性

- 将原生协议栈初始化与运行阶段的启动和停止分离。
- 通过 `ESP_BT_HUB_STATE_*` 状态表示 hub 生命周期。
- 将协议栈唯一的 Classic 和蓝牙低功耗（Bluetooth Low Energy，LE）GAP 回调同步分发给多个订阅者。
- client handle 采用 generation 校验，并在 handle 有效期间阻止 hub 反初始化。
- 使用强类型租约仲裁 Classic inquiry、LE 扫描、传统广播和扩展广播实例。
- 合并各 client 的 Classic 可见性请求：可连接请求按或运算合并，可发现请求采用最高等级。
- 支持自动回复配对请求，也支持应用程序通过 GAP 订阅自行回复。
- 将 Classic inquiry 结果和 LE 扩展广播报告转换为统一结构。
- 提供 LE 绑定查询、安全回复、隐私配置、连接控制和断开连接操作。
- 在所选主机和 ESP-IDF 配置支持时，提供扩展扫描、扩展广播、周期广播、周期同步和 Periodic Advertising Sync Transfer（PAST）。

## 生命周期与资源所有权

应用程序拥有 hub 生命周期：

1. 初始化非易失性存储（NVS），并调用 `esp_bt_hub_init()`。
2. 每个应用模块打开 client、订阅 GAP 事件，并向原生协议栈注册。
3. 所有应用模块准备完成后调用 `esp_bt_hub_start()`。
4. 仅在 hub 运行期间取得和释放设备发现、扫描或广播租约。
5. 全部租约释放后调用 `esp_bt_hub_stop()`。
6. 移除 GAP 订阅、关闭 client，并注销原生 profile。
7. 调用 `esp_bt_hub_deinit()`。

client handle 标识共享 hub 的一个应用模块。租约授予该 client 对一项控制器资源的独占权。handle 释放后即失效；即使内部 slot 随后被复用，旧 handle 仍然无效。

GAP 订阅接收所选主机协议栈的原生事件 ID 和参数类型。应用程序可访问全部协议栈事件，但回调代码会与所选后端和 ESP-IDF 版本相关。

## 公开头文件

| 头文件 | 用途 |
| --- | --- |
| `esp_bt_hub.h` | hub 生命周期、client、GAP 订阅、本地身份和配对配置 |
| `esp_bt_hub_classic.h` | Classic 可见性投票、inquiry 租约和 inquiry 结果解析 |
| `esp_bt_hub_le.h` | LE 扫描、广播、安全、隐私、连接、绑定和报告解析 |

## 配置

调用 Hub API 前，需要启用蓝牙并选择 Bluedroid 或 NimBLE 主机协议栈。

Bluedroid Classic 与 LE 配置：

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_CLASSIC_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
```

NimBLE 配置：

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
```

扩展 LE 操作还依赖相应的 ESP-IDF BLE 5.0 或 NimBLE 扩展广播选项。应用程序可使用能力宏 `ESP_BT_HUB_CLASSIC_SUPPORTED`、`ESP_BT_HUB_LE_SUPPORTED`、`ESP_BT_HUB_LE_LEGACY_SUPPORTED`、`ESP_BT_HUB_LE_EXT_SUPPORTED` 和 `ESP_BT_HUB_LE_PAST_SUPPORTED` 对可选代码进行条件编译。

`ESP_BT_HUB_CONFIG_DEFAULT()` 生成如下默认配置：

- 设备名称 `ESP-BT`
- 启用绑定和安全连接
- 关闭中间人（MITM）保护
- 输入/输出（I/O）能力为 `ESP_BT_HUB_IO_CAP_NONE`
- 启用自动配对回复
- Classic PIN 为 `0000`

自动回复会接受 Classic PIN、Classic 数值确认、LE 安全请求和 LE 数值比较，并拒绝 passkey 输入请求。应用程序自行处理配对事件时，将 `pairing_auto_reply` 设为 `false`。

## 快速开始

在 ESP-IDF 项目中添加该组件：

```yaml
dependencies:
  espressif/esp_bt_hub: "^0.1"
```

蓝牙 hub 初始化前需先初始化 NVS。以下示例展示必要的所有权与释放顺序：

```c
#include "esp_bt_hub.h"
#include "nvs_flash.h"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();
    esp_bt_hub_client_handle_t client = NULL;

    ESP_ERROR_CHECK(esp_bt_hub_init(&config));
    ESP_ERROR_CHECK(esp_bt_hub_client_open("app", &client));
    ESP_ERROR_CHECK(esp_bt_hub_start());

    /* 在此处订阅事件，或取得设备发现、扫描和广播租约。 */

    ESP_ERROR_CHECK(esp_bt_hub_stop());
    ESP_ERROR_CHECK(esp_bt_hub_client_close(client));
    ESP_ERROR_CHECK(esp_bt_hub_deinit());
}
```

`examples/bt_hub` 中的交互式控制台演示生命周期、GAP 分发、Classic inquiry、LE 扫描、广播和资源冲突处理。

## 使用注意事项

- GAP 订阅者在 BTC 任务或 NimBLE host 任务中同步执行。回调不得阻塞、反初始化 hub，或调用会等待 GAP 操作完成的 API。
- 原生 GAP 参数和 `esp_bt_hub_le_adv_report_parse()` 返回的广播数据只在回调返回前有效。需要保留的数据必须先复制。
- 仍有运行时资源租约有效时，`esp_bt_hub_stop()` 返回 `ESP_ERR_INVALID_STATE`。
- hub 尚未停止，或仍有 client、订阅有效时，`esp_bt_hub_deinit()` 返回 `ESP_ERR_INVALID_STATE`。
- 停止或反初始化失败会将生命周期切换到 `ESP_BT_HUB_STATE_FAULT`，应重试失败的操作。
- `esp_bt_hub_le_ext_scan_stop()` 会停止扫描，但保留扫描租约。外部租约需显式释放；由传统扫描 API 创建的租约则通过 `esp_bt_hub_le_scan_stop()` 释放。释放外部扫描租约前，必须先停止经 `esp_bt_hub_le_ext_scan_start()` 启动的扩展扫描，扫描未停止时 hub 会拒绝释放；通过原生 stack API 启动的扫描不由 hub 追踪，仍由调用者负责停止。
- 扩展广播可因有限时长、事件数上限或连接建立而结束。周期广播不会自动结束，释放实例前必须先停止。
- 在 Bluedroid 上，`esp_bt_hub_le_privacy_enable()` 将已绑定身份装入解析列表；在 NimBLE 上，隐私行为由主机配置决定。
- 在 Bluedroid 上，中心设备连接尝试的所有者必须在每个终止连接事件中调用
  `esp_bt_hub_le_connect_complete()`，无论事件报告成功还是失败。

## SoC 与主机兼容性

蓝牙能力同时取决于目标 SoC 和 ESP-IDF 配置：

- Classic API 仅在启用 Classic 蓝牙且控制器不是 BLE-only 模式时可用。
- NimBLE 只提供 LE API，不提供 Classic inquiry 或 Classic 可见性投票。
- 传统 LE 扫描和广播要求 NimBLE 或 Bluedroid BLE 4.2 支持。
- 扩展 LE 操作要求 NimBLE 扩展广播或 Bluedroid BLE 5.0 支持。
- PAST 要求扩展 LE 支持；使用 Bluedroid 时还需启用 `CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER`。

## FAQ

**为什么 `esp_bt_hub_client_close()` 返回 `ESP_ERR_INVALID_STATE`？**

该 client 仍持有设备发现、扫描或广播租约。关闭 client 前需先停止或释放对应资源。

**为什么 `esp_bt_hub_deinit()` 返回 `ESP_ERR_INVALID_STATE`？**

hub 仍在运行，或者仍有 client 或 GAP 订阅有效。先调用 `esp_bt_hub_stop()`，注销原生 profile，再重试反初始化。

**为什么第二个设备发现、扫描或传统广播请求失败？**

这些资源具有独占性。当前所有者停止或释放租约后，其他 client 才能取得该资源。

**多个蓝牙应用模块能否共享该组件？**

可以。hub 只初始化一次，每个应用模块使用独立的具名 client。各 client 共享 GAP 事件，并通过租约避免控制器操作冲突。
