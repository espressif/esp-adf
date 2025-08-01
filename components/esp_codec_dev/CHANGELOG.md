# Changelog

## v1.4.0

### Bug Fixed

- Fixed name error in some macro definitions for ES7210.
- Fixed link error for `vol_range` redefinition.

## v1.3.6

### Feature

- Added support for CJC8910 codec.

### Bug Fixed

- Added missing ES8389 include in `esp_codec_dev_defaults.h`.

## v1.3.5

### Feature

- Added support for ES8389 codec，see detail datasheet [ES8389](http://www.everest-semi.com/pdf/ES8389%20PB.pdf)
- Added read/write register APIs
- Disabled `CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE` by default.
  For ESP-IDF v5.x users who wish to continue using the legacy `i2c_driver_xx` APIs, please enable this option manually.

### Bug Fixes

- Fixed I2S PDM play noise issue

## v1.3.4

### Bug Fixes

- Enhance ES8311 I2C noise immunity


## v1.3.3

### Bug Fixes

- Fix codec without mute API, set mute wrong behavior


## v1.3.2

### Bug Fixes

- Fix `ES8374` volume register set error
- Fix `ES8388` microphone volume gain set incorrect


## v1.3.1

### Features

- Fix condition `CODEC_I2C_BACKWARD_COMPATIBLE` reversed
- Fix `CODEC_I2C_BACKWARD_COMPATIBLE` set to false build error


## v1.3.0

### Features

- Add configuration `CODEC_I2C_BACKWARD_COMPATIBLE` in Kconfig to allow use of the old I2C driver.
  Default is set to `y` for backward compatibility. To use the new I2C driver, set it to `n` instead.
- Fix send I2C address low byte firstly


## v1.2.0

### Features

- Add IDF v5.3 support
  Using new i2c driver `esp_driver_i2c`, add `bus_handle` configuration for `audio_codec_i2c_cfg_t`.
  User need create the `bus_handle` using API `i2c_new_master_bus` instead of `i2c_driver_install`.
- Change test code to standalone application, user can directly build it under folder [codec_dev_test](test_apps/codec_dev_test)

### Bug Fixes

- Fix I2S work in PDM mode record or play mono channel audio wrongly


## v1.1.0

### Features

- Add driver for AW88298, see detail datasheet [AW88298](https://datasheetspdf.com/download_new.php?id=1513778)

### Bug Fixes

- Fix ES8311 playback fade in for long time


## v1.0.3

### Bug Fixes

- Fix I2S TX and RX work in share mode, need enable TX before RX


## v1.0.2

### Features

- Add I2S TDM support
- Add API to `esp_codec_dev_set_in_channel_gain` to set input channel gain dependently


## v1.0.1

### Bug Fixes

- Fix ES8388 volume register set incorrectly


## v1.0.0

- Initial version of `esp_codec_dev`
