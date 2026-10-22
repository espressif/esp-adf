本目录聚焦于与云端智能体建立网络连接的代码实现，如果你想快速与云端建联并且移植到新的平台上，请参考以下教程：

### 1 工程依赖与前期准备简介
与云端建联的方案（高性能方案），依赖于以下基础库：
  1. mbedtls（第三方开源库）
  2. cjson（第三方开源库，但是本仓库在`src/util/cJSON`放置了一份源码）
  3. 系统抽象层代码（需要自己实现，接口头文件在`osal/inc`，已经实现的平台代码在`src`，供其他平台参考）
  4. 火山引擎volc_rtc_engine库（放置在`src/transports/high_quality/third_party/volc_rtc_engine_lite`目录下）
  5. 其他连接实现层代码，我们在`common.cmake`里已经归类好了，对于平台移植而言，直接使用即可，无需关注细节

### 2 构建自己的cmake脚本
参考`linux.cmake`脚本，逐一引用上述5个部分，构建一个静态的`volc_conv_ai_a`库即可；脚本内有注释，可供参考。
> 注意：对于具体的平台，需要修改使用平台自己的toolchain

### 3 sample编译运行简介（方便验证）
对于transport层，我们有简单的sample做以验证。
1. 修改`sample.c`中这一组账号：
   ```c
   #define CONFIG_VOLC_BOT_ID "xxxx"
   #define CONFIG_VOLC_INSTANCE_ID "xxxx"
   #define CONFIG_VOLC_PRODUCT_KEY "xxxx"
   #define CONFIG_VOLC_PRODUCT_SECRET "xxxx"
   #define CONFIG_VOLC_DEVICE_NAME "xxxx"
2. 编译 (Linux 环境下) 运行:
   ```bash
    # 编译
    mkdir build
    cd build
    cmake ..
    make

    # 运行
    ./test_sample
如果运行成功有如下打印:
   ```text
    Volc Engine connected 
    [INF|volc_rtc.c:184]remote user joined xxxxx elapsed 0 ms
    Volc Engine event: 0 
    火山引擎测试。 
    火山引擎测试。 
    火山引擎测试。 
    火山引擎测试。
   ```
对于其他平台，可以参考修改CMakeLists.txt 中test_sample相关部分，进行编译运行；