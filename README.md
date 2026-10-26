# Yuki9008工具

> 基于 **YRGui 4.0** 的高通 EDL（9008）刷机工具  
> Qualcomm Emergency Download Studio · Modern dark UI · Rich motion
>
> 作者：**river**  
> GitHub：<https://github.com/yriver1314/Yuki9008Tool>

![Yuki9008工具 刷写中心](preview.png)

## 这是什么

Yuki9008工具 是一个面向高通 9008（QDLoader / EDL）刷机流程的 Windows 桌面工具。
界面包含设备枚举、固件结构解析、分区浏览、真实写入进度、运行日志与导出。

**没有演示模式，也没有模拟写入。** 只有同时满足以下条件，写入按钮才会启用：

1. 检测到 `VID_05C6 / PID_9008` 设备，且设备暴露 `COMx` 端口；
2. 已选择有效的固件目录，并找到 `rawprogram*.xml` 与 Firehose 编程器；
3. 本机安装 QFIL / QPST 写入工具（`QSaharaServer.exe`、`fh_loader.exe`）。

未就绪时界面会明确显示原因，不会生成假的写入进度。

## 功能

- **刷写中心**：EDL 状态、真实 `fh_loader` 进度、速度 / 已写入 / 剩余时间、分区流水线
- **设备枚举**：SetupAPI 枚举 `VID_05C6 / PID_9008`（以及 900E），识别 COM 端口
- **固件管理**：选择或拖入固件目录，解析 `rawprogram*.xml` / `patch*.xml` / `prog_firehose*`
  - 读取分区标签、镜像文件名、文件大小
  - 生成 SHA-256 包指纹
  - 展示 Firehose 编程器、Mapping XML、分区统计
- **真实写入**：调用 `QSaharaServer.exe` 发送编程器，再调用 `fh_loader.exe` 写入 rawprogram；存在 patch XML 时追加 patch 阶段
- **分区浏览**：分区搜索、类型过滤、写入状态；支持导出 CSV
- **运行日志**：完整捕获写入工具输出，自动滚动、清空、导出 UTF-8 文本
- **设置**：四种强调色、动效开关、默认校验 / 重启策略、引擎与运行环境信息
- **动效**：页面淡入、导航 hover 光效、EDL 环形轨道、呼吸状态点、扫描线、
  数据脉冲、传输曲线、完成彩带、堆叠 Toast 与流光背景
- **原生窗口**：无边框、自绘标题栏、窗口拖动 / 最大化 / Win11 贴靠、Per-Monitor V2 DPI
- **品牌图标**：窗口图标、exe 资源图标、标题栏与侧栏 logo 全部读取 `logo.ico`
- **拖拽**：把解压后的固件目录拖到窗口任意位置即可载入

## 运行要求

写入依赖 Qualcomm 官方工具中的两个命令行程序：

```text
QSaharaServer.exe
fh_loader.exe
```

程序会自动在以下位置查找：

- 当前 exe 目录
- `C:\Program Files (x86)\Qualcomm\QPST\bin`
- `C:\Program Files\Qualcomm\QPST\bin`
- `C:\Program Files (x86)\Qualcomm\QFIL`
- `C:\Program Files\Qualcomm\QFIL`
- `PATH` 中的目录

如果安装在自定义位置，可设置环境变量：

```bat
set YUKI9008_ENGINE_DIR=D:\Qualcomm\QPST\bin
Yuki9008Tool.exe
```

设备需要以 QDLoader 9008 模式连接，并在设备管理器中显示为 `COMx`。
如果只显示 USB 设备而没有 COM 端口，工具会拒绝启动写入。

## 快速开始

直接运行：

```text
dist\Yuki9008Tool.exe
```

发行目录需要与 `logo.ico`、`libwinpthread-1.dll` 放在一起。

### 构建（MinGW-w64）

```bat
build.bat
```

或手动执行：

```bat
windres --codepage=65001 version.rc -O coff -o version.res
g++ main.cpp app_state.cpp paint.cpp paint_foreground.cpp paint_cards.cpp paint_pages.cpp version.res ^
  -o Yuki9008Tool.exe -mwindows -std=c++17 -O2 ^
  -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
  -lgdiplus -lgdi32 -luser32 -ldwmapi -luxtheme ^
  -lole32 -luuid -lshell32 -lsetupapi -lcomctl32 -lbcrypt -lwinhttp ^
  -static-libgcc -static-libstdc++
```

- 开发环境：MinGW-w64 GCC 15.2（CLion 自带工具链）
- 不需要 Qt / .NET；仅使用 Windows 自带库：GDI+、DWM、SetupAPI、BCrypt、Shell
- `libwinpthread-1.dll` 随发行目录提供

## 快捷键

| 快捷键 | 动作 |
| --- | --- |
| `Ctrl + Enter` | 开始 / 重新开始刷写（仅在设备、固件、引擎就绪时有效） |
| `F5` | 重新扫描 9008 设备 |
| `Ctrl + L` | 打开运行日志 |

## GitHub 软件更新

设置页内置 GitHub Releases 更新检查：

- 读取 `https://api.github.com/repos/yriver1314/Yuki9008Tool/releases/latest`
- 比较 Release tag 与当前版本号
- 有新版时显示版本号和“前往下载”
- 下载按钮优先打开 Release 中的 `Yuki9008Tool.exe` 资源，没有资源时回退到 Release 页面

更新仓库必须是 **public**，并至少发布一个 GitHub Release。发布资源建议命名为：

```text
Yuki9008Tool.exe
```

检查更新不会静默替换正在运行的 exe：下载完成后关闭旧版本，再运行新版本即可。
如果在 fork 上测试更新，可临时指定仓库：

```bat
set YUKI9008_REPO=owner/repo
Yuki9008Tool.exe
```

> GitHub 未认证 API 有频率限制（通常每小时 60 次），手动检查更新不受影响。

## 写入流程

1. `QSaharaServer.exe -u <COM号> -s 13:"<prog_firehose...>"`
2. `fh_loader.exe --port=\\.\COMn --sendxml="rawprogram0.xml" --search_path="<固件目录>" --memoryname=ufs|emmc --noprompt --showpercentagecomplete --zlpawarehost=1`
3. 存在 `patch*.xml` 时，再执行一次 `fh_loader --sendxml="patch0.xml"`
4. 成功后提示设备重启；重新进入 9008 后可再次刷写

> UFS / eMMC 根据固件路径与编程器文件名自动推断；若你的固件需要固定存储类型，请确认目录名或编程器中包含 `ufs` / `emmc` 标识。

## 目录

```text
.
├── YRGui.h                  # YRGui 4.0（已扩展自定义绘制钩子、Windows 临界区投递）
├── main.cpp                 # 程序入口、窗口子类、拖拽与快捷键
├── app_state.h / app_state.cpp  # 状态机、设备 / 固件 / 写入引擎 / 日志
├── paint.cpp                # 基础绘制、图标、背景
├── paint_foreground.cpp     # 侧栏、标题区、Hero、阶段、仪表
├── paint_cards.cpp          # 固件、传输、选项、设备、操作栏
├── paint_pages.cpp          # 固件 / 分区 / 日志 / 设置页与彩带
├── logo.ico                 # 应用 / 窗口 / 文件图标
├── version.rc               # 图标与作者等 Windows 版本资源（作者：river）
├── LICENSE
├── .gitignore
├── build.bat
├── libwinpthread-1.dll
├── preview.png
└── dist/
    ├── Yuki9008Tool.exe
    ├── logo.ico
    ├── libwinpthread-1.dll
    ├── README.md
    └── preview.png
```

## 设计

- 深色「午夜蓝 + 电光青」视觉，强调色可切换
- 三栏布局：232px 侧栏 / 自适应主内容 / 280px 右栏 / 底部操作条
- 字号与间距以逻辑像素设计，随系统 DPI 缩放
- 颜色语义：`ink / dim / accent / green / amber / red`
- 所有动画由 30 FPS 定时器驱动，空闲时自动降频；关闭动效后保留进度反馈

## 许可

- YRGui 4.0：MIT
- Yuki9008工具 应用代码：MIT

> 刷机有风险。请先确认机型、固件、分区表与供电状态，并确保 QFIL 工具版本与设备匹配。
