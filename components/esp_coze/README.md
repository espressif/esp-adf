# ESP_COZE

## Introduction

[ESP_COZE](https://www.coze.cn/home) is a bidirectional streaming dialogue component based on the Coze platform. It supports real-time voice/text interaction with a Coze agent via WebSocket.

This component is ideal for use cases such as voice assistants and intelligent voice Q&A systems. It features low latency and a lightweight design, making it suitable for applications running on embedded devices such as the ESP32.

## Adding the Component to Your Project

Use the component manager command add-dependency to add esp_coze as a project dependency. The component will be automatically downloaded and integrated into your project during the build process.
```c
idf.py add-dependency "espressif/esp-coze=*"
```

## How to Use

1. Create a Bot
Please create a bot on the official Coze platform. After creation, you will receive a unique BOTID, which is used to identify the target dialogue entity.

2. Create an App and Get a Token
Create an application on the Coze platform to generate an access token.


```c
    coze_chat_config_t chat_config = COZE_CHAT_DEFAULT_CONFIG();
    chat_config.bot_id = COZE_BOT_ID;
    chat_config.access_token = COZE_ACCESS_TOKEN;
    coze_chat_handle_t chat = coze_chat_init(&chat_config);
```