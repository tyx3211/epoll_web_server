# 高性能C语言Web服务器

本项目是为期三周深度实训的最终成果。它是一个使用C语言编写的、模块化的、轻量级高性能Web服务器，被设计为一个可复用的库。

该服务器基于Linux的`epoll` API构建了非阻塞I/O模型，并遵循Reactor设计模式以高效处理并发连接。

## 核心功能

-   **高性能核心**：采用基于`epoll`的Reactor模型，高效处理事件，适用于高并发场景。
-   **模块化库设计**：服务器核心被编译为静态库(`libwebserver.a`)，将通用服务器逻辑与用户具体应用逻辑解耦。
-   **静态文件服务**：能够从可配置的Web根目录提供静态文件服务（HTML, CSS, JS, 图片等），并包含基础的目录穿越攻击防御。
-   **动态API路由**：一个简洁有效的路由系统，允许将特定的URI路径与`GET`和`POST`请求的处理函数进行绑定。
-   **基于JWT的认证**：实现了现代化的、基于JSON Web Token (JWT)的无状态认证。专用的登录路由会生成令牌，用于访问受保护的资源。
-   **配置系统**：服务器参数（如端口、Web根目录、JWT密钥、日志级别）都从外部的`server.conf`文件加载，避免了硬编码。
-   **双日志系统**：维护两个独立的日志文件：`access.log`用于记录HTTP请求，`system.log`用于记录服务器内部事件，且日志级别可配置。

## 项目结构

```
.
├── Makefile          # 主Makefile，用于构建核心库和后端应用
├── bin/
│   └── user_backend  # 最终的可执行程序
├── config/
│   └── server.conf   # 服务器配置文件
├── include/          # 服务器库的头文件
│   ├── config.h
│   ├── http.h
│   ├── logger.h
│   ├── router.h
│   ├── server.h
│   └── utils.h
├── lib/
│   └── libwebserver.a # 编译生成的静态库
├── obj/              # 编译过程中生成的目标文件
├── src/              # 服务器库的源代码
│   ├── config.c
│   ├── http_parser.c
│   ├── logger.c
│   ├── router.c
│   ├── server.c
│   └── utils.c
├── user_backend/
│   └── main.c        # 使用服务器库的示例后端应用
└── www/              # 默认的静态文件根目录
    └── index.html
```

## 编译与运行

### 1. 构建项目

项目提供了一个完整的`Makefile`。只需运行`make`即可编译核心库(`libwebserver.a`)和示例后端应用(`user_backend`)。

```bash
make
```

要清除所有编译生成的文件，请使用：

```bash
make clean
```

### 2. 运行服务器

编译完成后，通过运行`server_app`可执行文件来启动服务器。它将自动从`conf/server.conf`加载配置。

```bash
./bin/server_app
```

加载我们预先提供下的配置文件，服务器将运行在 `http://127.0.0.1:8888`。

## 如何测试

您可以使用浏览器或命令行工具`curl`来测试服务器的各项功能。

### 1. 测试静态文件服务

打开您的浏览器并访问 `http://127.0.0.1:8888/index.html`。您应该能看到`www/index.html`文件的内容。

或者使用`curl`：

```bash
curl http://127.0.0.1:8888/index.html
```

### 2. 测试认证 (登录与JWT)

#### 步骤A：登录并获取JWT令牌

向`/api/login`路由发送一个包含用户名和密码的POST请求。

```bash
curl -X POST \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "username=admin&password=password123" \
     http://127.0.0.1:8888/api/login
```

如果凭证正确，服务器将返回一个JWT令牌：

```json
{
  "status": "success",
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJNeVdlYlNlcnZlciIsInN1YiI6ImFkbWluIiwiaWF0IjoxNzE5ODI0NTM0LCJleHAiOjE3MTk4MjgxMzR9.abcdefg..."
}
```

#### 步骤B：使用JWT令牌访问受保护路由

复制上面响应中的令牌。在访问受保护路由`/api/me`时，将其放入`Authorization`请求头中（并带上"Bearer "前缀）。

```bash
# 将 YOUR_TOKEN_HERE 替换为您实际收到的令牌
TOKEN="YOUR_TOKEN_HERE"

curl -H "Authorization: Bearer $TOKEN" http://127.0.0.1:8888/api/me
```

如果令牌有效，您将收到成功信息：
```json
{"status":"success", "user":{"username":"admin"}}
```

如果令牌缺失、无效或已过期，您将收到错误信息：
```json
{"error": "Unauthorized"}
``` 