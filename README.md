# VMOSEdge

![VMOS Edge 界面截图](screenshot1.png)

## 简介

VMOS Edge 是一款面向开发者与高性能用户的边缘算力盒，通过软硬一体化设计，将虚拟 Android 环境部署在本地，实现低延迟、高稳定的云机体验。无论是应用测试、自动化脚本、账号多开，还是高性能投屏控制，VMOS Edge 都能为您提供接近真机的运行效果。

## 许可证

本项目采用 [MIT](LICENSE) 许可证开源。

### 第三方库许可证声明

本项目使用了以下开源库，特此声明：

#### QtScrcpyCore

- **许可证**: Apache License 2.0
- **来源**: [QtScrcpy](https://github.com/barry-ran/QtScrcpy)
- **许可证文件**: [QtScrcpyCore/LICENSE](QtScrcpyCore/LICENSE)
- **说明**: QtScrcpyCore 使用了 [scrcpy](https://github.com/Genymobile/scrcpy) 的 server 端代码（scrcpy-server），scrcpy 采用 Apache License 2.0 许可证

#### FluentUI

- **许可证**: 请查看 FluentUI 目录下的许可证文件
- **说明**: FluentUI 组件库包含以下子组件：
  - QRCode 相关代码：LGPL-2.1
  - QCustomPlot：GPL-3.0
  - Chart.js：MIT License

#### FFmpeg (通过 QtScrcpyCore)

- **许可证**: LGPL v2.1+ / GPL v2+（取决于配置）
- **说明**: QtScrcpyCore 集成了 FFmpeg 库用于视频解码和处理
- **来源**: [FFmpeg](https://ffmpeg.org/)

#### ADB (Android Debug Bridge)

- **说明**: 项目使用 Android Debug Bridge (ADB) 工具与 Android 设备通信
- **来源**: ADB 工具随 Android SDK 提供

## 编译前置条件

### 1. 安装 Qt 6.8.2

- 下载并安装 [Qt 6.8.2](https://www.qt.io/download)
- 选择 **msvc2022** 编译器（64位）
- **注意**: Qt 采用 LGPL 许可证，使用本项目时请遵守 Qt 的许可证要求

### 2. 安装 vcpkg

- 克隆 vcpkg 仓库：
  ```bash
  git clone https://github.com/Microsoft/vcpkg.git
  cd vcpkg
  ```

- Windows 平台：
  ```bash
  .\bootstrap-vcpkg.bat
  ```

- Linux 平台：
  ```bash
  ./bootstrap-vcpkg.sh
  ```

- macOS 平台：
  ```bash
  ./bootstrap-vcpkg.sh
  ```

- 将 vcpkg 集成到系统（可选）：
  ```bash
  .\vcpkg integrate install
  ```

## 编译步骤

### Windows 平台

1. 配置 CMake，指定 vcpkg 工具链：
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>/scripts/buildsystems/vcpkg.cmake
   ```

2. 编译项目：
   ```bash
   cmake --build build --config Release
   ```

3. 编译完成后，可执行文件位于 `bin/Release/` 目录

### Linux 平台

1. 配置 CMake：
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>/scripts/buildsystems/vcpkg.cmake
   ```

2. 编译项目：
   ```bash
   cmake --build build --config Release
   ```

### macOS 平台

1. 配置 CMake：
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>/scripts/buildsystems/vcpkg.cmake
   ```

2. 编译项目：
   ```bash
   cmake --build build --config Release
   ```

## 依赖项

项目通过 vcpkg 管理以下依赖：

- pkgconf
- spdlog
- libyuv
- openssl
- stb
- ffmpeg
- libarchive
- zstd

## 技术栈

- **Qt 6.8.2** - 应用程序框架（LGPL 许可证）
- **CMake 3.21+** - 构建系统
- **vcpkg** - C++ 包管理
- **编译器要求**:
  - Windows: MSVC 2022 (64位)
  - Linux: GCC 或 Clang（支持 C++17）
  - macOS: Clang（Xcode 工具链）

## 项目结构

```
vmosedge/
├── CMakeLists.txt          # 主 CMake 配置文件
├── vcpkg.json              # vcpkg 依赖清单
├── FluentUI/               # FluentUI 组件库
├── QtScrcpyCore/           # QtScrcpy 核心库
├── src/                    # 源代码目录
│   ├── src/               # C++ 源代码
│   ├── qml/               # QML 界面文件
│   └── res/               # 资源文件
└── 3rdparty/              # 第三方库文件
```

## 贡献

欢迎提交 Issue 和 Pull Request！

## 注意事项

- **Qt 许可证**: 本项目使用 Qt 6.8.2。Qt 采用 LGPL 许可证，商业使用需要遵守相应的许可证要求。详情请参考 [Qt 许可证](https://www.qt.io/licensing/)
- **ADB 工具**: 运行时需要 Android Debug Bridge (ADB) 工具连接到 Android 设备
- **平台支持**: 项目支持 Windows、Linux 和 macOS 平台

## 问题反馈

如有问题，请在 GitHub Issues 中反馈。

