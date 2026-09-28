# API 说明

## 创建会话

```c
struct webclient_session *webclient_session_create(size_t header_sz);
```

创建客户端会话结构体。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|header_sz          | 最大支持的头部长度                   |
| **返回**          | **描述**                            |
|!= NULL            | webclient 会话结构体指针             |
|= NULL             | 创建失败                            |

## 关闭会话连接

```c
int webclient_close(struct webclient_session *session);
```

关闭传入的客户端会话连接，并释放内存。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
| **返回**          | **描述**                            |
|=0                 | 成功                                |


## 发送 GET 请求

```c
int webclient_get(struct webclient_session *session, const char *URI);
```

发送 HTTP GET 请求命令。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|URI                | 连接的 HTTP 服务器地址               |
| **返回**          | **描述**                            |
|`>0`               | HTTP 响应状态码                     |
|<0                 | 发送请求失败                        |

## 获取指定数据大小的 HEAD / GET 请求

```c
int webclient_shard_position_function(struct webclient_session *session, const char *URI, int size);
```

发送带有 Range 头信息的 HTTP GET/HEAD 请求命令，多用于断点续传 / 分片下载功能。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|URI                | 连接的 HTTP 服务器地址               |
|size               | 设定的接收空间                      |
| **返回**          | **描述**                            |
|`>0`               | HTTP 响应状态码                     |
|<0                 | 发送请求失败                        |

## 发送 POST 请求

```c
int webclient_post(struct webclient_session *session, const char *URI, const void *post_data, size_t data_len);
```

发送 HTTP POST 请求命令，上传数据到 HTTP 服务器。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|URI                | 连接的 HTTP 服务器地址               |
|post_data          | 需要上传的数据地址                   |
|data_len | 需要上传数据的长度 |
| **返回**          | **描述**                            |
|`>0`               | HTTP 响应状态码                     |
|<0                 | 发送请求失败                        |

## 发送数据

```c
int webclient_write(struct webclient_session *session, const void *buffer, size_t size);
```

发送数据到连接的服务器。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|buffer             | 发送数据的地址                       |
|size               | 发送数据的长度                       |
| **返回**          | **描述**                            |
|`>0`               | 成功发送数据的长度                   |
|=0                 | 连接关闭                            |
|<0                 | 发送数据失败                        |

## 接收数据

```c
int webclient_read(struct webclient_session *session, void *buffer, size_t size);
```

从连接的服务器接收数据。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|buffer             | 接收数据的存放地址                   |
|size               | 最大接收数据的长度                   |
| **返回**          | **描述**                            |
|`>0`               | 成功接收数据的长度                   |
|=0                 | 连接关闭                            |
|<0                 | 接收数据失败                        |


## 设置接收和发送数据超时时间

```c
int webclient_set_timeout(struct webclient_session *session, int millisecond);
```

设置连接的接收和发送数据超时时间。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|millisecond        | 设置的超时时间，单位毫秒             |
| **返回**          | **描述**                           |
|=0                 | 设置超时成功                        |

## 在请求头中添加字段数据

```c
int webclient_header_fields_add(struct webclient_session *session, const char *fmt, ...);
```

该函数用于创建会话之后和发送 GET 或 POST 请求之前，用于添加请求头字段数据。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|fmt                | 添加字段数据的表达式                 |
|...                | 添加的字段数据，为可变参数           |
| **返回**          | **描述**                           |
|`>0`               | 成功添加的字段数据的长度            |
| <=0               | 添加失败或者头部数据长度超出        |

## 通过字段名获取字段值数据

```c
const char *webclient_header_fields_get(struct webclient_session *session, const char *fields);
```

该函数用于发送 GET 或 POST 请求之后，可以通过传入的字段名称获取对应的字段数据。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|fields             | HTTP 字段名称                       |
| **返回**          | **描述**                            |
|= NULL             | 获取数据失败                        |
|!= NULL            | 成功获取的字段数据                   |


## 接收响应数据到指定地址

```c
int webclient_response(struct webclient_session *session, void **response, size_t *resp_len);
```

该函数用于发送 GET 或 POST 请求之后， 可以接收响应数据到指定地址。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
|response           | 存放接收数据的字符串地址             |
|resp_len | 接收数据的长度的指针 |
| **返回**          | **描述**                           |
| `>0`              | 成功接收数据的长度                  |
| <=0               | 接收数据失败                        |

## 发送 GET/POST 请求并接收响应数据

```c
int webclient_request(const char *URI, const char *header, const void *post_data, size_t data_len, void **response, size_t *resp_len);
```

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|URI                | 连接的 HTTP 服务器地址              |
|header             | 需要发送的头部数据                  |
|                   | = NULL，发送默认头数据信息，可用于发送 GET/POST请求 |
|                   | != NULL，发送指定头数据信息，可用于发送 GET/POST请求  |
|post_data          | 发送到服务器的数据                  |
|                   | = NULL，该发送请求为 GET 请求       |
|                   | != NULL，该发送请求为 POST 请求     |
| data_len | 发送数据的长度 |
|response           | 存放接收数据的字符串地址             |
|resp_len | 接收数据长度的指针 |
| **返回**          | **描述**                           |
| `>0`              | 成功接收数据的长度                  |
| <=0               | 接收数据失败                        |

## 拼接请求头部数据

```c
int webclient_request_header_add(char **request_header, const char *fmt, ...);
```

该函数适用于 webclient_request 函数发送请求之前，头部数据的拼接和添加。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|request_header     | 请求头部数据缓冲区地址               |
|fmt                | 添加字段数据的表达式                 |
|...                | 添加的字段数据，为可变参数           |
| **返回**          | **描述**                           |
| `>0`              | 成功添加的字段数据的长度              |
| <=0               | 头部数据添加失败或内存不足            |


## 获取 HTTP 响应状态码

```c
int webclient_resp_status_get(struct webclient_session *session);
```

该函数用于发送 GET 或 POST 请求之后，用于获取返回的响应状态码。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
| **返回**          | **描述**                           |
| `>0`              | HTTP 响应状态码                     |

## 获取 Content-Length 字段数据

```c
int webclient_content_length_get(struct webclient_session *session);
```

该函数用于发送 GET 或 POST 请求之后，用于获取返回的 Content-Length 字段数据。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|session            | 当前连接会话结构体指针               |
| **返回**          | **描述**                           |
| `>0`                | Content-Length 字段数据            |
| <0                | 获取失败                           |

## 下载文件到本地

```c
int webclient_get_file(const char *URI, const char *filename);
```

从 HTTP 服务器下载文件并存放到本地。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|URI                | 连接的 HTTP 服务器地址               |
|filename           | 存放文件位置、名称                   |
| **返回**          | **描述**                            |
|=0                 | 下载文件成功                        |
|<0                 | 下载文件失败                        |

## 上传文件到服务器

```c
int webclient_post_file(const char *URI, const char *filename, const char *form_data);
```

从 HTTP 服务器下载文件并存放到本地。

| 参数              | 描述                                |
|:------------------|:-----------------------------------|
|URI                | 连接的 HTTP 服务器地址               |
|filename           | 需要上传的文件位置、名称              |
|form_data          | 附加选项                            |
| **返回**          | **描述**                            |
|=0                 | 上传文件成功                        |
|<0                 | 上传文件失败                        |