# WebClient

[Chinese](README_ZH.md) | English

## 1. Introduction

The WebClient software package is independently developed by RT-Thread and is based on the implementation of the HTTP protocol client. It provides the basic functions of communication between the device and the HTTP Server.

The features of the WebClient software package are as follows:

- Support IPV4/IPV6 address;
- Support GET/POST request method;
- Support file upload and download function;
- Support HTTPS encrypted transmission;
- Complete header data addition and processing methods.

For more software package introduction, please refer to [Detailed introduction](docs/introduction.md).

### 1.1 Directory structure

The directory structure of the WebClient software package is as follows:

```
webclient
├───docs 
│   └───figures                   // Documents use pictures
│   │   api.md                    // API instructions
│   │   introduction.md           // Introduction document
│   │   principle.md              // Implementation principle
│   │   README.md                 // Document structure description
│   │   samples.md                // package sample
│   │   user-guide.md             // Instructions
│   └───version.md                // version
├───inc                           // header file
├───src                           // source file
├───samples                       // sample code
|   |   webclient_get_sample      // GET request sample code
│   └───webclient_post_sample     // POST request sample code
│   LICENSE                       // package license
│   README.md                     // Software package instructions
└───SConscript                    // RT-Thread default build script
```

### 1.2 License

The WebClient software package complies with the Apache-2.0 license, see the LICENSE file for details.

### 1.3 Dependency

- RT_Thread 3.0+

- [mbedtls package](https://github.com/RT-Thread-packages/mbedtls) (if HTTPS support is enabled)

## 2. Get the software package

To use the WebClient software package, you need to select it in the RT-Thread package management. The specific path is as follows:

```
RT-Thread online packages
    IoT-internet of things --->
         [*] WebClient: A HTTP/HTTPS Client for RT-Thread
         [ ]   Enable debug log output
         [ ]   Enable webclient GET/POST/SHARD samples
         [ ]   Enable file download feature support
               Select TLS mode (Not support)  --->
                   (x) Not support
                   () SAL TLS support
                   () MbedTLS support
               Version (latest) --->
```

**Enable webclient GET/POST samples**: add sample code;
**Select TLS mode**: Configure to enable HTTPS support and select the supported mode;
- **Not support**: Does not support TLS function;
- **SAL TLS support**: Configure the TLS function support in the SAL component, and abstract the TLS operation in the SAL component. Users also need to **manually configure the type of TLS software package used** (currently only supports the MbedTLS package);
- **MbedTLS support**: configure MbedTLS function support;
**Version**: Configure the software package version.

After the configuration is complete, let the RT-Thread package manager automatically update, or use the pkgs --update command to update the package to the BSP.

## 3. Use WebClient software package
- For detailed description of the software package, please refer to [Package Introduction](docs/introduction.md)
- For detailed sample introduction, please refer to [Sample Document](docs/samples.md) 
- How to use from scratch, please refer to [User Guide](docs/user-guide.md) 
- For complete API documentation, please refer to [API Manual](docs/api.md) 
- The working principle of the software package, please refer to [Working Principle](docs/principle.md)

- More **Detailed introduction documents** are located in the [`/docs`](/docs) folder, **Please check before using the package for development**.

## 4. Matters needing attention


 - When the WebClient software package connects to the HTTPS server, you need to enable the TLS function support in WebClient.
 - After the WebClient software package version update (`V1.0.0 -> the current latest version V2.0.0`), the function interface and usage process in the software package have changed. If the previous interface is used in the developer code, the latest version interface can be adapted , Or select the `V1.0.0` version in the version number configuration, the specific modification method can refer to the software package [migration guide](docs/migration-guide.md).

## 5. Contact & Thanks

- Maintenance: RT-Thread development team
- Homepage: https://github.com/RT-Thread-packages/webclient