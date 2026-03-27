# esp_live_photo

`esp_live_photo` 为乐鑫平台提供 **Live Photo / Motion Photo** 支持。

它使设备能够**创建并解析混合媒体文件**，将以下内容合为一体：

- 高质量 **JPEG 图像**（封面）
- 短时 **MP4 视频片段**（可选音频）

该格式在智能手机上广泛用于拍摄带动态瞬间的照片：在兼容相册中查看时，静态图像可呈现动态效果。

---

## 什么是 Live / Motion Photo？

**Live Photo**（Android 上常称 **Motion Photo**）是一种混合媒体格式：

- 外观为标准 **JPEG 文件**
- 内部嵌有 **MP4 视频**
- 兼容的相册应用可识别并播放动态片段

借助 `esp_live_photo`，这些文件可在**设备端直接生成与解析**，无需在 PC 或服务器上进行后处理。

---

## 典型使用场景

- 智能相机拍摄「前后瞬间」
- 智能门铃保存带动态增强的快照
- AI 相机将检测结果与短视频片段配对
- IoT 设备生成与智能手机兼容的 Live Photo

---

## 主要特性

### Live Photo 创建

- 将 **JPEG + MP4** 合并为单个 Motion Photo 文件
- 自动生成所需的 **XMP 元数据**
- 可靠处理 **MP4 偏移量**
- 输出兼容智能手机相册的文件

### Live Photo 解析与提取

- 解析 **JPEG 封面图像**及嵌入的 **XMP 元数据**
- 定位嵌入的 **MP4 视频**
- 支持将 JPEG 与 MP4 作为独立资源提取

---

## 工作原理

### 创建流程

1. 将 JPEG 图像写入文件头
2. 为 Motion Photo 元数据预留空间
3. 追加 MP4 流（视频 + 可选音频）
4. 关闭文件时完成并更新 XMP 元数据

### 解析流程

1. 解析 Motion Photo **XMP 元数据**
2. 回退：若缺少元数据则扫描 MP4 `ftyp` box
3. 独立提取 JPEG 与 MP4

---

## 组件

### Live Photo Muxer

通过 `esp_muxer` 框架创建 Live Photo 文件。

- 注册：`esp_live_photo_muxer_register()`
- 类型：`ESP_MUXER_TYPE_LIVE_PHOTO`

---

### Live Photo Extractor

通过 `esp_extractor` 框架解析 Live Photo 文件。

- 注册：`esp_live_photo_extractor_register()`
- 类型：`ESP_EXTRACTOR_TYPE_LIVE_PHOTO`

---

## 接入示例

Live Photo 文件在 **`esp_muxer`**（创建 / 写入）与 **`esp_extractor`**（打开 / 读取）中作为**一种容器类型**使用。音视频仍通过常规的 muxer、extractor API 写入或读出；选择 Live Photo 类型即启用 JPEG + MP4 布局及相关元数据。

针对 **JPEG 封面**，可使用：

- **`esp_live_photo_muxer_set_cover_jpeg`** — 封装时设置封面图
- **`esp_live_photo_extractor_read_cover_frame`** — 将封面作为独立图像帧读出

完整流程请参考示例 [live_photo_capture](examples/live_photo_capture/README_CN.md)。

---

## 配置

在 menuconfig 中启用自定义 extractor 支持：

```
CONFIG_EXTRACTOR_CUSTOM_SUPPORT=y
```

## 技术支持

如需技术支持，请使用以下链接：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
