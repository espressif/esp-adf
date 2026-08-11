# Documentation Source Folder

This folder contains source files of **ESP-ADF documentation** available in [English](https://docs.espressif.com/projects/esp-adf/en/release-v2.x/) and [中文](https://docs.espressif.com/projects/esp-adf/zh_CN/release-v2.x/).

The sources do not render well in GitHub and some information is not visible at all.

Use actual documentation generated within about 20 minutes on each commit:

# Hosted Documentation

* English: https://docs.espressif.com/projects/esp-adf/en/release-v2.x/
* 中文: https://docs.espressif.com/projects/esp-adf/zh_CN/release-v2.x/

The above URLs are for the ESP-ADF release/v2.x documentation. Click on the link in the bottom right corner to download the PDF version.


# Building Documentation

The documentation is built using the python package `esp-docs`, which can be installed by running:

```
pip install esp-docs
```

For a summary of available options, run:

```
build-docs --help
```

For more information, see [ESP-Docs User Guide](https://docs.espressif.com/projects/esp-docs/en/latest/).

## For MSYS2 MINGW32 on Windows

If using Windows and the MSYS2 MINGW32 terminal, run this command before running "make html" the first time:

```
pacman -S doxygen mingw-w64-i686-python2-pillow
```

Note: Currently it is not possible to build docs on Windows without using a Unix-on-Windows layer such as MSYS2 MINGW32.
