ESP Audio Analyzer
==================

:link_to_translation:`zh_CN:[中文]`

As smart audio devices develop rapidly and their application scenarios continue to expand, requirements for audio quality and system performance keep increasing. Espressif has introduced a new audio analysis tool that provides a convenient, efficient testing method and a standardized process. Combined with an intuitive web interface and companion test project, it supports standardized audio testing.

With this tool, users can comprehensively test key audio modules such as microphones, speakers, and AEC, quickly locating and troubleshooting system issues, thereby effectively improving development efficiency and product quality.

Links
----------------

- Analysis platform: `ESP Audio Analyzer <https://audio-tools.espressif.com.cn>`__
- Companion test project: `ESP Audio Analyzer Test Project <https://github.com/espressif/esp-adf/tree/master/adf_examples/checks/esp_audio_analyzer_app>`__

Platform Features
-----------------

- Comprehensive coverage: covers three major modules—microphone, speaker, and AEC—with 11 audio tests in total
- Convenient adaptation: based on ESP Board Manager, supporting quick adaptation to different hardware
- Performance analysis: provides audio metrics such as THD and SNR for evaluating system performance
- Standardized process: provides unified test methods and evaluation criteria
- Raw data download: supports exporting raw recording files for retesting and in-depth analysis
- Visual reports: generates structured test reports containing data, charts, and test results

Test Items
----------------

- Microphone tests

  - Microphone structural airtightness test
  - Microphone electro-acoustic characteristics test
  - Microphone frequency response curve test
  - Microphone noise floor test
  - Microphone array loudness consistency test
  - Microphone array frequency response consistency test
  - Microphone array phase consistency test

- Speaker tests

  - Speaker electro-acoustic characteristics test
  - Speaker frequency sweep test
  - Speaker PA noise floor test

- AEC test

  - AEC echo signal test
