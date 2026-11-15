# 大万状态监控屏

把大万变成 Windows 电脑的局域网监控屏。设备界面适配 480×480 实屏，显示 CPU、GPU、内存、系统盘、网络速率和运行时长。数据约每两秒更新一次。2.0 提供可安装的主题 Skill，用户可通过 Agent 定制界面并从 Windows 上位机上传。

## 下载与使用

从 [GitHub Releases](https://github.com/tonive/dawan-pc-status/releases) 选择同一版本的两个文件。

| 文件 | 用途 |
| --- | --- |
| `DawanPCMonitor.exe` | Windows 10/11 x64 上位机，双击即可运行 |
| `dawan_pc_status.zip` | 万墨 OS 开发包，使用 OneOS 官方开发安装工具装到大万 |

1. 把电脑和大万接入同一可信局域网，在上位机选择对应的 IPv4 并点「启动服务」。
2. 在大万打开监控屏，首次使用点击「电脑 → 手动输入」，填入窗口上的完整 IPv4。
3. 大万会记住地址，之后自动重连。连接失败时搜索已知网段，也可以在「电脑」页点选其他可用上位机。

[完整安装与操作指南](docs/user-guide.md) · [自制主题和安装 Skill](docs/themes.md) · [源码构建与验证](docs/build.md) · [第三方组件](THIRD_PARTY_NOTICES.md)

## 用 Agent 定制主题

把下面这段发给你平时使用的智能体。支持 Agent Skills 的平台可导入同一个完整 Skill。

```text
请安装大万监控屏的 dawan-pc-theme Skill。
来源：https://github.com/tonive/dawan-pc-status/tree/v2.0.2/skills/dawan-pc-theme
请获取完整文件夹，按你所在平台支持的方式安装。如果需要我手动导入，请告诉我导入入口。
```

安装后，只需告诉智能体你想要的主题，例如「做一个太空主题」。它会设计对应的像素图标，并在未指定时选择布局。把生成的 `.wmtheme.json` 导入 Windows 上位机，再到大万预览和安装。[主题指南](docs/themes.md)介绍了 ZIP 导入、仅支持附件时的用法和 Codex 示例。

## 功能范围

- 支持 CPU 和内存占用、Windows 系统盘、全机网络上下行及开机时长。系统可用 `nvidia-smi` 时，可显示 NVIDIA 显卡使用率和温度。
- 上位机可选择允许大万预约关机。配对后可在大万发起操作，Windows 窗口提供 15 秒倒计时和取消按钮。
- USB 用于安装和诊断。实时数据和关机请求使用局域网，电脑可通过网线接入局域网。
- 开发包以万墨公开 SDK 2026-11-03 的 STREAMING ABI `0x00010006` 为目标。安装前请确认设备固件支持相应的 ELF 和开发安装协议。

产品交互参考 [ESP32C3-PC-Wireless-Display](https://github.com/WuyongLab/ESP32C3-PC-Wireless-Display)。本项目的设备界面、上位机和通信协议为独立实现。
