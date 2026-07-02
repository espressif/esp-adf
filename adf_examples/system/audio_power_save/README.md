# Audio Power Save and Wakeup

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example demonstrates automatic light sleep with Wi-Fi and MQTT keepalive, plus UART, MQTT, GPIO, and timer wakeup handling. Prompt tones can play before entering idle and after wakeup.

- After Wi-Fi connects, MQTT keepalive runs under automatic light sleep
- Supports UART, MQTT, GPIO, and timer wakeup
- Two sleep–wakeup validation rounds; the device resumes normal operation after each wakeup

### Typical Scenarios

This example is suitable for voice or audio devices that stay connected while idle and support local or remote wakeup.

### Prompt Tone Resources

Prompt tone files are stored under `tone/`. At build time, `littlefs_create_partition_image()` packs them into a LittleFS partition named `storage`, mounted at `/littlefs`. To replace the prompt tones, update the files under `tone/` and rebuild and flash the firmware.

## Environment Setup

### Hardware Required

- Audio DAC and speaker when prompt playback is enabled; disable `Enable prompt tone playback` to test low power and wakeup only
- Wi-Fi network that can access the MQTT broker
- Serial input for UART wakeup; if GPIO wakeup is enabled, connect a controllable GPIO input

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

- Default MQTT broker: `mqtt://broker.emqx.io`
- Default MQTT wakeup topic: `/gmf/audio_power_save/wakeup`

## Build and Flash

### Build Preparation

Before building this example, ensure the ESP-IDF environment is set up. If it is already set up, skip this paragraph and go to the project directory. If not, run the following in the ESP-IDF root directory to complete the environment setup. For full steps, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/index.html).

```
./install.sh
. ./export.sh
```

Short steps:

- Go to this example's project directory:

```
cd adf_examples/system/audio_power_save
```

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager) to manage board-level resources. The [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) helper tool is recommended as the default entry point.

- Install once in your activated ESP-IDF Python environment:

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # run this command when an update is requested
```

- List the currently visible boards:

```bash
idf.py bmgr -l
```

Example output:

```text
ℹ️  Board Components:
  espressif/esp_boards:
    [1] esp32_c3_lyra
    [2] esp32_lyrat_4_3
    [3] esp32_lyrat_mini_1_1
    [4] esp32_p4_eye
    [5] esp32_p4_function_ev_board
    [6] esp32_s31_function_coreboard_1
    [7] esp32_s31_korvo_1
    [8] esp32_s3_box_3
    [9] esp32_s3_box_lite
    [10] esp32_s3_korvo_2_3
    [11] esp32_s3_lcd_ev_board
    [12] esp_vocat_1_0
    [13] esp_vocat_1_2
```

The example output above is based on the board list and ordering from `esp_boards` 0.5.2. Different `esp_boards` versions or custom board dependencies may change the list and indexes. Use the actual output of `idf.py bmgr -l` when selecting a board.

- Select a board:

```bash
idf.py bmgr -b <board_index|board_name>
```

For example, to select `esp32_s3_korvo_2_3`:

```bash
idf.py bmgr -b 10
# or
idf.py bmgr -b esp32_s3_korvo_2_3
```

On first invocation, the component is downloaded automatically based on the `espressif/esp_board_manager` dependency declared in `main/idf_component.yml`.

> [!NOTE]
> To switch to a different board supported by `esp_board_manager`, repeat the same steps with the new board name or index.
> For a custom board, see [Creating a Board Guide](https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/index.html).
> For more information about `esp_board_manager`, see the [ESP Board Manager Getting Started Guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md).

### Project Configuration

```bash
idf.py menuconfig
```

Configure the following in menuconfig:

- `Wi-Fi Configuration` → `WiFi SSID`
- `Wi-Fi Configuration` → `WiFi Password`
- `MQTT Configuration` → `MQTT broker URI`
- `MQTT Configuration` → `MQTT wakeup topic`
- `MQTT Configuration` → `MQTT status topic`
- `MQTT Configuration` → `MQTT keepalive interval in seconds`
- `Power Management Configuration` → `Maximum CPU frequency in MHz`
- `Power Management Configuration` → `Minimum CPU frequency in MHz`
- `Enable prompt tone playback`
- `Wakeup Source Configuration` → `Timer wakeup delay in milliseconds`
- `Wakeup Source Configuration` → `Maximum wakeup wait time in milliseconds`
- `Wakeup Source Configuration` → `Enable GPIO wakeup source`
- `Wakeup Source Configuration` → `GPIO wakeup number` (enable GPIO wakeup first)
- `Wakeup Source Configuration` → `GPIO wakeup active level` (Low level / High level)
- `Wakeup Source Configuration` → `Enable UART wakeup source`

> Press `s` to save and `Esc` to exit after configuration.

### Build and Flash Commands

- Build the example:

```
idf.py build
```

- Flash the firmware and run the serial monitor (replace PORT with your port name):

```
idf.py -p PORT flash monitor
```

- To exit the monitor, use `Ctrl-]`

## How to Use the Example

### Functionality and Usage

After flash and reset, the example runs in this order:

1. Initialize prompt playback resources and configure low-power operation.
2. Connect Wi-Fi and start MQTT keepalive; wait until the MQTT client is connected.
3. Configure UART, MQTT, GPIO, and timer wakeup sources.
4. Enter two sleep–wakeup validation rounds. Each round includes:
   - Playing `enter_sleep.mp3`, then releasing prompt playback resources;
   - Entering idle low power (automatic light sleep + Wi-Fi modem sleep) and waiting for wakeup;
   - After wakeup, restoring prompt playback resources, playing `exit_sleep.mp3`, then running for about 3 seconds before the next round.
5. After both rounds complete, release all resources and exit.

After each idle low-power entry, UART, MQTT, GPIO, or the timer can trigger wakeup. When prompt playback is enabled (`CONFIG_EXAMPLE_ENABLE_PROMPT_PLAYBACK=y`), the example plays `enter_sleep.mp3` before entering idle and `exit_sleep.mp3` after wakeup.

### Log Output

Example successful run log. Automated tests (`pytest_audio_power_save.py`) trigger the second wakeup by publishing to the default MQTT topic. The log snippet below is from on-target capture where the second wakeup was GPIO (default GPIO0 / BOOT button); both behaviors are valid:

```text
I (1700) AUDIO_POWER_SAVE: [ 1 ] Initialize audio power save
I (1740) AUDIO_POWER_SAVE: [ 2 ] Connect Wi-Fi and start MQTT keepalive
W (1799) wifi:Haven't to connect to a suitable AP now!
I (1801) NETWORK_MGR: Connect Wi-Fi, ssid:ESP-Audio, listen_interval:10
W (1805) wifi:Haven't to connect to a suitable AP now!
I (1805) NETWORK_MGR: STA_CONFIG: listen_interval=10
W (1809) wifi:Password length matches WPA2 standards, authmode threshold changes from OPEN to WPA2
I (4440) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: Wi-Fi connected
I (5182) NETWORK_MGR: MQTT connected, broker=mqtt://broker.emqx.io, keepalive=30 s
I (5186) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: prepare wakeup sources
I (5187) AUDIO_POWER_SAVE: [ 3 ] Configure wakeup sources
I (5192) WAKEUP_MGR: Waiting for GPIO0 to become inactive (level=1)...
I (5198) WAKEUP_MGR: GPIO wakeup enabled, gpio=0, active level=0
I (5215) WAKEUP_MGR: UART wakeup enabled, uart=0, threshold=3
I (5216) WAKEUP_MGR: Timer wakeup configured, timeout=30000 ms
I (5216) AUDIO_POWER_SAVE: [ 4 ] Enter idle and wait for wakeup
W (5229) ESP_GMF_ASMP_DEC: Not enough memory for out, need:1152, old: 1024, new: 1152
E (6929) i2s_common: i2s_channel_disable(1262): the channel has not been enabled yet
W (6930) PERIPH_I2S: Caution: Releasing TX (0x3c1c09e0).
W (6931) PERIPH_I2S: Caution: RX (0x3c1c0b9c) forced to stop.
I (6937) AUDIO_POWER_SAVE: Enter idle and wait for wakeup
I (6942) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_MAX_MODEM, reason: enter idle low power
I (6950) WAKEUP_MGR: Player idle; waiting for wakeup (UART/MQTT/GPIO/timer) in automatic light sleep
I (10831) WAKEUP_MGR: Wakeup event: uart, event=8
I (10833) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: wakeup handled
I (10833) WAKEUP_MGR: Wakeup handled by UART
I (10836) AUDIO_POWER_SAVE: Exit idle after UART wakeup
W (10888) ESP_GMF_ASMP_DEC: Not enough memory for out, need:1152, old: 1024, new: 1152
E (17371) i2s_common: i2s_channel_disable(1262): the channel has not been enabled yet
W (17372) PERIPH_I2S: Caution: Releasing TX (0x3c1c322c).
W (17373) PERIPH_I2S: Caution: RX (0x3c1c33e8) forced to stop.
I (17379) AUDIO_POWER_SAVE: Enter idle and wait for wakeup
I (17384) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_MAX_MODEM, reason: enter idle low power
I (17392) WAKEUP_MGR: Player idle; waiting for wakeup (UART/MQTT/GPIO/timer) in automatic light sleep
I (20830) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: wakeup handled
I (20830) WAKEUP_MGR: Wakeup handled by GPIO
I (20830) AUDIO_POWER_SAVE: Exit idle after GPIO wakeup
W (20883) ESP_GMF_ASMP_DEC: Not enough memory for out, need:1152, old: 1024, new: 1152
I (25654) AUDIO_POWER_SAVE: Wakeup validation done
I (25654) AUDIO_POWER_SAVE: [ 5 ] Destroy all the resources
E (26039) i2s_common: i2s_channel_disable(1262): the channel has not been enabled yet
W (26039) PERIPH_I2S: Caution: Releasing TX (0x3c1c322c).
W (26041) PERIPH_I2S: Caution: RX (0x3c1c33e8) forced to stop.
I (26047) AUDIO_POWER_SAVE: Func:app_main, Line:76, MEM Total:8629212 Bytes, Inter:281251 Bytes, Dram:281251 Bytes

I (26056) AUDIO_POWER_SAVE: Example finished
```

## Troubleshooting

### MQTT broker connection timeout

If the following log appears, the current network cannot connect to the configured MQTT broker. Since this example waits for MQTT to be connected before entering idle low power, the wakeup flow will not continue until a reachable broker is configured:

```text
E (17164) esp-tls: [sock=54] select() timeout
E (17164) transport_base: Failed to open a new connection: 32774
E (17164) mqtt_client: Error transport connect
W (17167) NETWORK_MGR: MQTT error
I (17170) NETWORK_MGR: MQTT disconnected
```

To verify MQTT wakeup, configure `CONFIG_EXAMPLE_MQTT_BROKER_URI` to a broker reachable from the current network.

## Related References

- [Power save keepalive FAQ](https://github.com/espressif/esp-adf/blob/release/v2.x/examples/system/power_save/README.md#troubleshooting)
- [Wi-Fi low power mode](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/low-power-mode/low-power-mode-wifi.html)

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
