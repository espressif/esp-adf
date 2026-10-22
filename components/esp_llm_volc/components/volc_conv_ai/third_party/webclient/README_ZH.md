# WebClient

中文页 | [英文页](README.md)

## 1、介绍

WebClient 软件包是 RT-Thread 自主研发的，基于 HTTP 协议的客户端的实现，它提供设备与 HTTP Server 的通讯的基本功能。

WebClient 软件包功能特点如下：

- 支持 IPV4/IPV6 地址；
- 支持 GET/POST 请求方法；
- 支持文件的上传和下载功能；
- 支持 HTTPS 加密传输；
- 完善的头部数据添加和处理方式。

更多软件包介绍请查看 [详细介绍](docs/introduction.md)。

### 1.1 目录结构

WebClient 软件包目录结构如下所示：

``` 
webclient
├───docs 
│   └───figures                     // 文档使用图片
│   │   api.md                      // API 使用说明
│   │   introduction.md             // 介绍文档
│   │   principle.md                // 实现原理
│   │   README.md                   // 文档结构说明  
│   │   samples.md                  // 软件包示例
│   │   user-guide.md               // 使用说明
│   └───version.md                  // 版本
├───inc                             // 头文件
├───src                             // 源文件				
├───samples                         // 示例代码
|   |   webclient_get_sample        // GET 请求示例代码
│   └───webclient_post_sample       // POST 请求示例代码
│   LICENSE                         // 软件包许可证
│   README.md                       // 软件包使用说明
└───SConscript                      // RT-Thread 默认的构建脚本
```

### 1.2 许可证

WebClient 软件包遵循 Apache-2.0 许可，详见 LICENSE 文件。

### 1.3 依赖

- RT_Thread 3.0+

- [mbedtls 软件包](https://github.com/RT-Thread-packages/mbedtls)（如果开启 HTTPS 支持）

## 2、获取软件包

使用 WebClient 软件包需要在 RT-Thread 的包管理中选中它，具体路径如下：

```
RT-Thread online packages
    IoT - internet of things  --->
         [*] WebClient: A HTTP/HTTPS Client for RT-Thread
         [ ]   Enable webclient GET/POST/SHARD samples
               Select TLS mode (Not support)  --->
                   (x) Not support
                   ( ) SAL TLS support
                   ( ) MbedTLS support
               Version (latest)  --->
```

**Enable webclient GET/POST samples** ：添加示例代码；

**Select TLS mode** ：配置开启 HTTPS 支持，选择支持的模式；

- **Not support**：不支持 TLS 功能；
- **SAL TLS support**：配置 SAL 组件中 TLS 功能支持，SAL 组件中抽象 TLS 操作，用户还需要**手动配置开启使用的 TLS 软件包类型**（目前只支持 MbedTLS 软件包）；
- **MbedTLS support**：配置 MbedTLS 功能支持；

**Version** ：配置软件包版本。

配置完成后让 RT-Thread 的包管理器自动更新，或者使用 pkgs --update 命令更新包到 BSP 中。

## 3、使用 WebClient 软件包
- 软件包详细介绍，请参考 [软件包介绍](docs/introduction.md)

- 详细的示例介绍，请参考 [示例文档](docs/samples.md) 

- 如何从零开始使用，请参考 [用户指南](docs/user-guide.md)

- 完整的 API 文档，请参考 [API 手册](docs/api.md)

- 软件包工作原理，请参考 [工作原理](docs/principle.md) 

- 更多**详细介绍文档**位于 [`/docs`](/docs) 文件夹下，**使用软件包进行开发前请务必查看**。

## 4、注意事项

 - WebClient 软件包连接 HTTPS 服务器时需要开启 WebClient 中对 TLS 功能的支持。
 - WebClient 软件包版本更新（`V1.0.0 -> 当前最新版 V2.0.0`）后软件包中函数接口和使用流程都有所变化，若开发者代码中使用之前接口，可以适配最新版本接口，或者在版本号配置中选择 `V1.0.0` 版本，具体改动方式可参考软件包 [迁移指南](docs/migration-guide.md)。


## 5、联系方式 & 感谢

- 维护：RT-Thread 开发团队
- 主页：https://github.com/RT-Thread-packages/webclient