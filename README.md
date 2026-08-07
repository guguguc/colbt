# COLBT — 跨平台即时通讯

一套**逻辑与 UI 分离**的即时通讯软件，同一套纯 C++ 逻辑库驱动三端 UI：

| 端 | 技术栈 | 目录 |
|----|--------|------|
| 逻辑层 | 纯 C++17 静态库 `imcore`（**零 Qt 依赖**） | `core/` |
| 服务器 | 独立 C++ TCP 进程 + SQLite 持久化 | `server/` |
| 桌面端 | Qt 6 Widgets（QSS 仿 Discord 深色主题） | `client/` |
| Android | Kotlin + Jetpack Compose + JNI 桥 | `android/` |
| iOS | SwiftUI + ObjC++ 桥接 | `ios/` |

三端客户端复用同一份 `core` 逻辑（socket / 协议编解码 / ClientCore），只写各自的 UI 与桥接层，行为完全一致。

## 功能

- 注册 / 登录（密码 SHA-256 存储）、心跳保活、掉线检测、同账号顶号
- 好友：添加 / 删除、在线状态实时同步
- 单聊 / 群聊：文本、图片、文件消息
- 群管理：建群、改群名、踢人、退群、解散
- 消息能力：已读 / 未读回执、撤回、引用回复、系统消息
- 最近会话列表（时间、最后一条、未读红点角标）
- 历史消息加载、消息搜索、对方输入中提示
- Discord 风格深色 UI（桌面 / Android / iOS 一致）

## 架构

```
┌────────────┐  ┌───────────────┐  ┌──────────────────┐
│ Qt Widgets │  │ Android Compose│  │ SwiftUI (iOS)    │
│  client/   │  │  android/      │  │  ios/            │
└─────┬──────┘  └───────┬───────┘  └────────┬─────────┘
      │ Qt 信号槽         │ JNI              │ ObjC++ 桥接
┌─────▼──────────────────▼──────────────────▼─────────┐
│              core/  im::ClientCore (纯C++)            │
│   socket + codec + 业务逻辑，单工作线程，线程安全      │
└──────────────────────────┬────────────────────────────┘
                           │ 二进制协议 (TCP)
┌──────────────────────────▼────────────────────────────┐
│              server/  im::ServerCore (纯C++)           │
│   accept线程 + 每连接读写线程 + SQLite(FULLMUTEX)      │
└───────────────────────────────────────────────────────┘
```

### 逻辑层回调线程模型

`ClientCore` 在独立 `std::thread` 上运行（连接、收包、分发、心跳）。
所有 API 线程安全（发送加锁），结果通过 `im::IClientListener` 接口在逻辑线程回调。
各端桥接层把回调编组到 UI 线程：

- Qt：`AppContext` 收到后 `emit` 信号，AutoConnection 队列投递到 UI 线程
- Android：JNI 回调经 `Handler.post` 到主线程
- iOS：ObjC 桥接先转成 ObjC 对象，再 `dispatch_async` 到主队列（避免跨线程引用已销毁的 C++ 对象）

### 服务器并发

`accept` 线程 + 每连接 2 线程（读线程处理命令、写线程负责推送）。
`ServerCore` 维护 `userId → Session` 注册表；消息推送直接入目标会话发送队列。
SQLite 以 `FULLMUTEX` + 内部互斥锁保证多线程安全。

### 协议

长度前缀二进制帧：`magic(4) | ver(1) | cmd(2) | reserved(1) | bodyLen(4) | body`。
命令包括登录/注册、联系人/会话/历史、发送/撤回/已读/引用/输入中、文件上传下载、群管理、搜索等。
文件上限 32MB，图片消息在客户端加载历史时自动下载。

## 项目结构

```
colbt/
├── core/                     # 纯C++逻辑库（零Qt/sqlite 仅服务器端需要）
│   ├── include/im/core/      #   公共接口：types / protocol / icclient / icserver
│   └── src/                  #   net(socket) protocol(codec) client server(storage/session)
├── server/                   # TCP服务器可执行入口
├── client/                   # Qt Widgets 桌面端
│   └── src/app/              #   AppContext 桥接层（跨线程）
│   └── src/ui/               #   登录/主窗口/会话/联系人/聊天面板/气泡
├── android/                  # Android 客户端
│   └── app/src/main/
│       ├── cpp/              #   JNI 桥接（jni_imclient / jni_bridge）
│       ├── java/com/colbt/im/core/   # ImCore / NativeImClient / 数据类
│       └── java/com/colbt/im/ui/     # Compose UI（MainActivity）
├── ios/                      # iOS 客户端
│   ├── CMakeLists.txt        #   交叉编译 core → libimcore.a
│   ├── build_core.sh         #   真机(arm64) + 模拟器(arm64/x86_64)
│   ├── project.yml           #   XcodeGen 工程定义
│   └── App/                  #   ObjC++ 桥接 + SwiftUI（IMModels / ImClientBridge / IMCore / UI）
└── .github/workflows/        # CI：桌面构建 / iOS 构建
```

## 构建

### 依赖（桌面端 + 服务器）

CMake ≥ 3.16、g++ ≥ 9、Qt 6（Widgets）、sqlite3、OpenSSL。

```bash
cmake -S . -B build
cmake --build build -j
```

产物：
- `build/server/imserver` —— 服务器：`imserver [端口=9000] [数据库=im.db]`
- `build/client/imclient` —— 桌面客户端

> 核心库仅编译 `client/` 子集时无需 sqlite3/OpenSSL（见 `ios/CMakeLists.txt`、`android/.../cpp/CMakeLists.txt`）。

### Android

Gradle 8.7 + AGP 8.4.2 + Kotlin 2.0 + Compose BOM 2024.06.00，NDK 交叉编译 JNI 桥（arm64-v8a / x86_64）。

```bash
cd android
# 国内网络：用本地 Gradle 离线构建（避免 services.gradle.org 拉取卡住）
~/Android/gradle/gradle-8.7/bin/gradle :app:assembleDebug --offline
# 产物：app/build/outputs/apk/debug/app-debug.apk
```

> 依赖仓库已配置阿里云镜像（`maven.aliyun.com`）；NDK/SDK 建议用腾讯云镜像 `mirrors.cloud.tencent.com/AndroidSDK/`。

### iOS

iOS 端依赖 macOS + Xcode，本仓库通过 GitHub Actions 自动构建（见下）。本地构建：

```bash
cd ios
./build_core.sh                       # 交叉编译 core → build/lib/{dev,sim}/libimcore.a
xcodegen generate                     # 生成 Colbt.xcodeproj
xcodebuild -project Colbt.xcodeproj -scheme Colbt \
  -destination 'generic/platform=iOS Simulator' \
  IMCORE_LIB_PATH="$(pwd)/build/lib/sim" CODE_SIGNING_ALLOWED=NO build
```

### CI（GitHub Actions）

- **iOS**（`.github/workflows/ios-build.yml`）：macOS runner 上 CMake 交叉编译 core → XcodeGen 生成工程 → 构建模拟器 `.app` 与真机未签名 `.ipa`，作为 artifact 上传。
- 每次 push 到 `main` 自动触发；也支持 `workflow_dispatch` 手动触发。

## 运行

```bash
# 终端1：启动服务器
build/server/imserver 9000 im.db

# 终端2/3：启动两个桌面客户端，分别注册两个账号，互相添加好友、聊天
build/client/imclient
```

### 手机连接局域网服务器

服务器监听 `0.0.0.0`，同一 Wi-Fi 下手机直接连电脑局域网 IP：

1. 查电脑 IP：`hostname -I`（如 `192.168.1.4`）
2. 客户端登录页：服务器填该 IP，端口填服务器端口（如 `9000`）
3. iOS 首次连接会弹「允许访问本地网络」，需允许；Android 模拟器访问宿主机用 `10.0.2.2`

## 测试

核心逻辑集成测试覆盖：注册/登录/好友/群聊/文本/图片/文件/已读/撤回/引用/群管理/搜索/输入中，命令：

```bash
cmake --build build -j && ./build/server/imserver 9000 im.db
g++ -std=c++17 -I core/include -I core/src core/src/net/socket.cpp core/src/protocol/codec.cpp core/src/client/clientcore.cpp /tmp/opencode/im_integration_test.cpp -o /tmp/opencode/itest
```
