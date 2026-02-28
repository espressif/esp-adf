# esp_live_photo

`esp_live_photo` enables **Live Photo / Motion Photo** support on Espressif platforms.

It allows devices to **create and parse hybrid media files** that combine:

- A high-quality **JPEG image** (cover)
- A short **MP4 video clip** (optional audio supported)

This format is widely used by smartphones to capture moments with motion, where a still image animates when viewed in compatible galleries.

---

## What is a Live / Motion Photo?

A **Live Photo** (or **Motion Photo** on Android) is a hybrid media format:

- Appears as a standard **JPEG file**
- Internally contains an embedded **MP4 video**
- Recognized by compatible gallery apps, which can play the motion segment

With `esp_live_photo`, these files can be **generated and parsed directly on-device**, without requiring post-processing on a PC or server.

---

## Typical Use Cases

- Smart cameras capturing “before and after” moments
- Smart doorbells storing motion-enhanced snapshots
- AI cameras pairing detection results with short video clips
- IoT devices generating smartphone-compatible Live Photos

---

## Key Features

### Live Photo Creation

- Combines **JPEG + MP4** into a single Motion Photo file
- Automatically generates required **XMP metadata**
- Handles **MP4 offset management** reliably
- Produces files compatible with smartphone galleries

### Live Photo Parsing & Extraction

- Parses **JPEG cover image** and embedded **XMP metadata**
- Locates embedded **MP4 video**
- Supports extraction of JPEG and MP4 as independent assets

---

## How It Works

### Creation Flow

1. Write JPEG image as the file header
2. Reserve space for Motion Photo metadata
3. Append MP4 stream (video + optional audio)
4. Finalize and update XMP metadata on file close

### Parsing Flow

1. Parse Motion Photo **XMP metadata**
2. Fallback: scan for MP4 `ftyp` box if metadata is missing
3. Extract JPEG and MP4 independently

---

## Components

### Live Photo Muxer

Used to create Live Photo files via the `esp_muxer`.

- Register with: `esp_live_photo_muxer_register()`
- Type: `ESP_MUXER_TYPE_LIVE_PHOTO`

---

### Live Photo Extractor

Used to parse Live Photo files via the `esp_extractor`.

- Register with: `esp_live_photo_extractor_register()`
- Type: `ESP_EXTRACTOR_TYPE_LIVE_PHOTO`

---

## Integration

A Live Photo file is handled as a **container type** in **`esp_muxer`** (create / write) and **`esp_extractor`** (open / read). Use the ordinary muxer and extractor APIs to feed or read **audio and video**; choosing the Live Photo type wires in the JPEG + MP4 layout and metadata.

For the **JPEG cover** specifically, use:

- **`esp_live_photo_muxer_set_cover_jpeg`** — set the cover image while muxing
- **`esp_live_photo_extractor_read_cover_frame`** — read the cover as a standalone image frame

For end-to-end usage, see the [live_photo_capture](examples/live_photo_capture/README.md) example.

---

## Configuration

Enable custom extractor support in menuconfig:

```
CONFIG_EXTRACTOR_CUSTOM_SUPPORT=y
```

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
