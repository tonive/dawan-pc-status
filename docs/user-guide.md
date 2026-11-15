# 安装与操作指南

## 准备

- 一台支持万墨 ELF 开发安装的大万，插入 SD 卡并开机。
- 一台 Windows 10/11 x64 电脑，与大万接入同一可信局域网。
- 一条可传数据的 USB 线，用于首次安装和更新。
- 从同一 [GitHub Release](https://github.com/tonive/dawan-pc-status/releases) 下载 `DawanPCMonitor.exe`、`dawan_pc_status.zip` 和 `SHA256SUMS.txt`。

在下载目录打开 PowerShell，运行下列命令，将结果与 `SHA256SUMS.txt` 对照。

```powershell
Get-FileHash -Algorithm SHA256 .\DawanPCMonitor.exe, .\dawan_pc_status.zip
```

## 安装到大万

下载完整的 [OneOS 开发工具仓库](https://gitee.com/dogbro/one-os_dev-skill)，按其中的环境说明准备 Python 3、串口驱动和可用的 C++17 编译器。官方安装器会在电脑上用 C++17 编译器执行 ELF 预检。若编译器不在 PATH，把环境变量 `CXX` 指向编译器的完整路径。在 OneOS 开发工具仓库目录运行下列命令，将 ZIP 路径换成实际下载位置。

```powershell
python -m pip install -r .\requirements-device.txt
python -m serial.tools.list_ports -v
python .\tools\install_dev_package.py "C:\Downloads\dawan_pc_status.zip" --app dawan_pc_status --port auto --transport otg --launch
```

`--port auto --transport otg` 适用于 Windows 能明确识别出唯一大万 OTG 端口的情况。如果只枚举到大万 UART 端口，用上一步看到的端口号替换示例的 `COM3`。

```powershell
python .\tools\install_dev_package.py "C:\Downloads\dawan_pc_status.zip" --app dawan_pc_status --port COM3 --transport auto --launch
```

出现 `installed_instance_id` 表示设备已完成安装登记。若应用未自动打开，在大万的应用列表中打开开发版。更新前先退出正在运行的应用，再用同一条命令安装新版。更多端口及协议说明见 OneOS 工具仓库中的 `docs/dev-install.md`。

## 启动监控

1. 双击 `DawanPCMonitor.exe`，选择与大万互通的私有 IPv4，点击「启动服务」。窗口会显示电脑 IP。预览版 EXE 未做代码签名，运行前请核对下载来源和 SHA256。
2. 在大万底部点「电脑 → 手动输入」，填入完整 IP，点「直连电脑」。也可以输入 IP 前三段，点「搜索网段」并选择电脑。
3. 连接成功后，首页显示电脑名和 IP，读数会持续刷新。下次打开时会尝试恢复连接。在新的网段首次使用时，重新输入一次地址。

上位机监听所选地址的 TCP `47831`。如果专用网络上的 Windows 防火墙拦截连接，可为该上位机开放专用网络入站访问。监控指标经局域网 HTTP 传输，适合可信私人网络。

## 预约关机

1. 在上位机勾选「允许大万预约关机」。
2. 在大万点「配对」，输入上位机显示的 20 位配对码。上位机重新启动服务会生成新码，大万退出应用后也需要重新输入。
3. 点「关机」，核对电脑名称，保存电脑上的工作，再点「确认关机」。
4. 上位机窗口会开始 15 秒倒计时，期间可点「取消关机」。到期后由 Windows 执行正常关机。

关机使用双方随机挑战和 HMAC-SHA256。配对码留在两端，网络上传输的是一次性的验证数据。关机功能默认关闭。

## 常见问题

| 现象 | 检查顺序 |
| --- | --- |
| 安装器找不到设备 | 确认设备开机、SD 卡已插入，并使用可传数据的 USB 线。用 `python -m serial.tools.list_ports -v` 核对驱动与端口。 |
| 安装提示设备忙 | 在大万退出监控屏，关闭占用同一 COM 口的工具，再执行安装。 |
| 找不到电脑或读数为空 | 确认上位机已启动，输入的是窗口上的 IPv4。检查两端能否互通，以及防火墙是否拦截 TCP 47831。访客 Wi-Fi 可能隔离设备。 |
| GPU 或温度显示「—」 | 上位机需要可用的 NVIDIA 驱动和 `nvidia-smi` 才能读取这些指标。 |
| 设备端没有关机按钮 | 先在上位机允许预约关机，等待设备下一次指标刷新。 |
