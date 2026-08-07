# colbt — Qt 即时通讯客户端 + 纯C++逻辑层 + TCP服务器

一个仿 QQ 的即时通讯软件。**UI 与逻辑分离**：

- **逻辑层 `core/`**：纯 C++17 静态库（`imcore`），**零 Qt 依赖**。
  包含跨平台 socket、二进制协议编解码、客户端/服务器业务逻辑、SQLite 持久化（OpenSSL SHA-256 密码哈希）。
- **UI 层 `client/`**：Qt 6 Widgets 客户端，通过 `AppContext`（Qt 桥接层）调用逻辑库，回调经信号队列编组到 UI 线程。
- **服务器 `server/`**：独立进程，多线程（每连接一读一写线程），数据落 SQLite。

```
colbt/
├── CMakeLists.txt            # 顶层构建
├── core/                     # 纯C++逻辑库（无Qt）
│   ├── include/im/core/      #   公共接口：types/protocol/icclient/icserver
│   └── src/                  #   net(socket) protocol(codec) client server(storage)
├── server/                   # TCP服务器可执行
└── client/                   # Qt Widgets UI（QSS 仿QQ主题）
    └── src/app/              #   AppContext 桥接层（跨线程）
    └── src/ui/               #   登录/主窗口/会话列表/联系人/聊天面板/气泡
```

## 功能

- 注册 / 登录（密码 SHA-256 存储）
- 好友列表（在线状态实时同步）、添加好友、创建群聊
- 单聊 / 群聊，文本消息 + 系统消息
- 最近会话列表（时间、最后一条、未读红点角标）
- 历史消息加载、消息气泡界面、表情插入
- 心跳保活、掉线检测、同账号顶号

## 构建

依赖：CMake ≥ 3.16、g++ ≥ 9、Qt 6（Widgets）、sqlite3、OpenSSL。

```bash
cmake -S . -B build
cmake --build build -j
```

产物：
- `build/server/imserver`  —— 服务器：`imserver [端口=9000] [数据库=im.db]`
- `build/client/imclient`  —— 客户端

## 运行

```bash
# 终端1：启动服务器
build/server/imserver 9000 /tmp/im.db

# 终端2/3：启动两个客户端，分别注册两个账号，登录后互相添加好友、聊天
build/client/imclient
```

## 架构要点

| 层 | 目录 | 说明 |
|----|------|------|
| UI | `client/src/ui` | 纯展示，仅依赖 `AppContext` 信号/槽 |
| 桥接 | `client/src/app` | `AppContext` 持有 `ClientCore`，把逻辑线程回调转为 Qt 队列信号 |
| 逻辑 | `core/src` | `ClientCore`/`ServerCore`，单工作线程 + 事件分发，跨平台 |
| 协议 | `core/src/protocol` | 长度前缀二进制帧 `magic|ver|cmd|len|body` |

### 逻辑层回调线程模型

`ClientCore` 在独立 `std::thread` 上运行（连接、收包、分发、心跳）。
所有 API 线程安全（发送加锁）。结果通过 `im::IClientListener` 接口在逻辑线程回调，
`AppContext` 收到后 `emit` Qt 信号 —— 自动连接（AutoConnection）会以队列方式投递到 UI 线程，天然解耦。

### 服务器并发

`accept` 线程 + 每连接 2 线程（读线程处理命令、写线程负责推送）。
`ServerCore` 维护 `userId → Session` 注册表；消息推送直接入目标会话发送队列。
SQLite 以 `FULLMUTEX` + 内部互斥锁保证多线程安全。
