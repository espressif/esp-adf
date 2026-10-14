# 迁移指南

## 1、V1.0.0 -> V2.0.0 版本改动

本节主要介绍 WebClient 软件包版本升级之后（`V1.0.0` -> 最近版本 `V2.0.0`），软件包的改动和适配最新版本的方式。

### 函数改动

1. 添加 `webclient_session_create()` 创建客户端结构体函数

    V1.0.0 版本客户端会话结构体不需要手动创建，V2.0.0 版本改为手动创建，用户可自定义传入的头文件长度，减少资源占用。

2. `webclient_open` -> `wenclient_get`

    V1.0.0 发送 GET 命令的函数 `webclient_open` 改为 `wenclient_get`，两个函数区别在于 `webclient_open` 函数只能发送默认头部信息的 GET 请求， `wenclient_get` 函数既可以发送默认也可以发送自定义头部数据的 GET 请求。

3. `webclient_open_position` -> `wenclient_get_position`

    同上述 `webclient_open` 函数改动类似。

4. 添加 `webclient_post` 发送 POST 请求函数

    V1.0.0 版本若要发送 POST 请求需要用户完成创建会话、连接、发送头部数据等整个流程，V2.0.0 版本封装了 `webclient_post` 函数完成整个 POST 请求发送流程，具体使用方式可参考 [API 手册](api.md) 和 [用户指南](user-guide.md)。

5. 添加头部信息处理方式

    V1.0.0 版本中没有对头部数据处理的方式，用户只能通过字符串拼接的方式来添加头部数据。V2.0.0 版本就这一问题提出相应改进方式，增加了在请求头中添加字段数据的函数（`webclient_header_fields_add`），以及通过字段名获取字段数据的函数（`webclient_header_fields_get`），方便用户添加请求头数据和获取响应头数据中信息。

6. 其他功能函数添加

- 添加获取当前响应状态码函数（`webclient_resp_status_get` ）；
- 添加获取当前响应 Content-Length 数据函数（`webclient_content_length_get`）。

### 流程改动

#### GET 请求流程改动

下面以发送自定义头部数据的 GET 请求方式为例，介绍新版本软件包中 GET 流程改动。

V1.0.0 版本 GET 请求流如下：

- 拼接头部数据（字符串拼接方式）
- 创建客户端会话结构体
- 客户端与服务器建立连接
- 发送 GET 请求头部数据
- 接收并解析服务器下发头部数据
- 接收服务器发送主体数据
- 关闭连接

```c
/* 字符串处理方式拼接头部数据 */
const char header = "xxx";
struct webclient_session *session = RT_NULL;

session = web_malloc(sizeof(struct webclient_session))

webclient_connect(session, URI);

webclient_send_header(session, WEBCLIENT_GET, header, strlen(header));

webclient_handle_response(session);

while(1)
{
    webclient_read(session, post_data ,strlen(post_data));
    ...
}

webclient_close(session);
```

V2.0.0 版本 GET 请求流如下：

- 创建会话结构体 
- 拼接头部数据（函数拼接方式）
- 发送 GET 请求头部并接收响应
- 接收服务器下发数据
- 关闭连接

```c
struct webclient_session *session = RT_NULL;

session = webclient_session_create(1024)

webclient_header_fields_add(session, "HOST: %s", URI);

webclient_get(session, URI);

while(1)
{
    webclient_read(session, buffer ,buf_sz);
    ...
}

webclient_close(session);
```

#### POST 请求流程改动

下面以发送自定义头部数据的 POST 请求方式为例，介绍新版本软件包中的 POST 流程改动。

V1.0.0 版本 POST 请求流如下：

- 拼接头部数据（字符串拼接方式）
- 创建客户端会话结构体
- 客户端与服务器建立连接
- 发送 POST 请求头部数据
- 接收并解析服务器下发头部数据
- 上传数据到服务器
- 关闭连接

```c
/* 字符串处理方式拼接头部数据 */
const char header = "xxx";
struct webclient_session *session = RT_NULL;

session = web_malloc(sizeof(struct webclient_session))

webclient_connect(session, URI);

webclient_send_header(session, WEBCLIENT_POST, header, strlen(header));

webclient_handle_response(session);

while(1)
{
    webclient_write(session, post_data ,strlen(post_data));
    ...
}

webclient_close(session);
```

V2.0.0 版本 POST 请求流如下：

- 创建会话结构体
- 拼接头部数据（函数拼接方式）
- 发送头部数据和主体数据到服务器，并接收服务器响应
- 关闭连接

```c
struct webclient_session *session = RT_NULL;

session = webclient_session_create(1024)

webclient_header_fields_add(session, "Content-Length: %s", post_data_sz);

webclient_post(session, URI, post_data, rt_strlen(post_data));

webclient_close(session);
```

上述介绍 WebClient 常用 GET/POST 请求方法改动，更多流程介绍请查看软件包 [使用指南](user-guide.md)。

## 2、V2.1.0 -> V2.2.0 版本改动

V2.2.0 版本主要改动部分为函数接口参数定义，目的是为了适配非字符串格式数据的读写，其他函数的使用流程未产生明显改动。

### 函数改动

- `webclient_post` 函数参数改动

  该函数入参 `const char *post_data` 修改为 `const void *post_data`，并添加`size_t data_len` 参数，用于确定需要发送的数据长度。

- `webclient_read` 函数参数改动

  该函数入参 `unsigned char *buffer` 修改为 `void *buffer`，用于适配非字符串数据接收。

- `webclient_write` 函数参数改动

  该函数入参 `const unsigned char *buffer` 修改为 `const void *buffer`，用于适配非字符串数据发送。

- `webclient_response` 函数参数改动

  该函数入参 `unsigned char **response` 修改为 `void  **response`，并且添加参数 `size_t *resp_len`，用于获取接收数据长度信息，适配非字符串数据接收。

- `webclient_request` 函数参数改动

  该函数入参 `const char *post_data` 修改为 `const void *post_data`，入参 `unsigned char **response` 修改为 `void **response`，并且添加入参 `size_t data_len` 和 `size_t *resp_len`，用于适配非字符串数据接收和发送。

