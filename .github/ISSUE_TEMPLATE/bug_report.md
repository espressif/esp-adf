---
name: Bug report
about: ESP-ADF crashes, produces incorrect output, or has incorrect behavior
title: ''
labels: ''
assignees: ''

---

----------------------------- Delete below -----------------------------

**Reminder: If your issue is a general question, start similar to "How do I..". If it is related to 3rd party development kits/libs, please discuss this on our community forum at https://esp32.com instead.**

INSTRUCTIONS
============

Before submitting a new issue, please follow the checklist and try to find the answer.

- [ ] I have read the documentation [Espressif Audio Development Guide](https://docs.espressif.com/projects/esp-adf/en/latest/index.html) and the issue is not addressed there.
- [ ] I have updated my ADF and IDF branch (master or release) to the latest version and checked that the issue is present there.
- [ ] I have searched the issue tracker for a similar issue and not found a similar issue.

If the issue cannot be solved after the steps above, please follow these instructions so we can get the needed information to help you quickly and effectively.

1. Fill in all the fields under **Environment** marked with [ ] by picking the correct option for you in each case and deleting the others.
2. Describe your issues.
3. Include [debug logs from the "monitor" tool](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/tools/idf-monitor.html), or [coredumps](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/core_dump.html).
4. Providing as much information as possible under **Other Items If Possible** will help us locate and fix the problem.
5. Use [Markdown](https://guides.github.com/features/mastering-markdown/) (see formatting buttons above) and the `Preview` tab to check what the issue will look like.
6. Delete these instructions from the above to the below marker lines before submitting this issue.

**IMPORTANT: Please follow the above instructions and provide as many details as possible. It will save a lot of communication time and improve the efficiency of problem solving. The more details you provide, the faster we may be able to reproduce and resolve the issue. Thanks!**

----------------------------- Delete above -----------------------------

## Environment

- Audio development kit:      [ESP32-LyraT|ESP32-LyraT-Mini|ESP32-LyraTD-MSC|ESP32-S2-Kaluga-1|ESP32-Korvo-DU1906|ESP32-S3-Korvo-2|ESP32-S3-Korvo-2-LCD|none]
- Audio kit version (for ESP32-LyraT/ESP32-LyraT-Mini/ESP32-S3-Korvo-2): [v1|v2|v3|v4]
- [Required] Module or chip used:  [ESP32-WROOM-32E|ESP32-WROVER-E|ESP32-S2-WROVER|ESP32-S3-WROOM-1]
- [Required] IDF version (run ``git describe --tags`` in $IDF_PATH folder to find it):
    // v4.4-191-g83daa6dabb
- [Required] ADF version (run ``git describe --tags`` in $ADF_PATH folder to find it):
    // v2.3-171-gaac5912
- Build system:         [Make|CMake|idf.py]
- [Required] Running log:         All logs from power-on to problem recurrence
- Compiler version (run ``xtensa-esp32-elf-gcc --version`` in your project folder to find it):
    // 1.22.0-80-g6c4433a
- Operating system:     [Windows|Linux|macOS]
- (Windows only) Environment type: [MSYS2 mingw32|ESP Command Prompt|Plain Command Prompt|PowerShell]
- Using an IDE?: [No|Yes (please give details)]
- Power supply:         [USB|external 5V|external 3.3V|Battery]


## Problem Description

// Detailed problem description goes here.

### Expected Behavior

### Actual Behavior

### Steps to Reproduce

1. step1
2. ...

// If possible, attach a picture of your setup/wiring here.


### Code to Reproduce This Issue

```cpp
// the code should be wrapped in the ```cpp tag so that it will be displayed better.
#include "esp_log.h"

void app_main()
{

}

```
// If your code is longer than 30 lines, [GIST](https://gist.github.com) is preferred.

## Debug Logs

```
Debug log goes here. It should contain the backtrace, as well as the reset source if it is a crash.
Please copy the plaintext here for us to search the error log. Or attach the complete logs and leave the main part here if the log is *too* long.
```

## Other Items If Possible

- [ ] sdkconfig file (Attach the sdkconfig file from your project folder)
- [ ] elf file in the ``build`` folder (**Note this may contain all the code details and symbols of your project.**)
- [ ] coredump (This provides stacks of tasks.)

