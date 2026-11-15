# 源码构建与验证

设备端使用万墨公开 SDK 2026-11-03 的 STREAMING ABI `0x00010006`，目标 ESP32-P4。构建环境使用 ESP-IDF v5.5.5 配套的 `esp-14.2.0_20260121` RISC-V GCC 和本机 C++17 编译器。SDK 及其开发安装器从 [OneOS 开发工具仓库](https://gitee.com/dogbro/one-os_dev-skill) 获取。上位机使用 Python 3.11、Tk 和 psutil。

## 构建设备包

从本项目根目录执行。这里假设 OneOS 工具仓库位于项目的同级目录，已激活工具链，本机 C++17 编译器位于 PATH 或已设置 `CXX`。每次构建使用新的输出目录。

```powershell
$project = (Get-Location).Path
$sdk = (Resolve-Path ..\one-os_dev-skill).Path
python -B .\build_device.py --sdk-root $sdk --out "$project/.build/elf"
python -B "$sdk/sdk/tools/validate_elf.py" "$project/.build/elf/dawan_pc_status.elf" --abi 0x00010006
python -B "$sdk/skills/wanmo-app-dev/scripts/package_app.py" --kind elf --sdk-root $sdk --package "$project/.build/elf" --output "$project/delivery/device"
```

公开包使用以上默认命令。若只为自己的大万定制首次搜索，可在第一条 Python 命令加 `--lan-seed <电脑的私有IPv4>`。

## 构建 Windows 上位机

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install psutil==7.2.2 pyinstaller==6.20.0
.\.venv\Scripts\python.exe -m PyInstaller --onefile --windowed --name DawanPCMonitor --distpath delivery --workpath .build/pyi --specpath .build host.py
```

## 验证

- 官方 ELF 工具检查 ABI、SDK 能力清单、文件摘要及 ZIP 内容。
- 上位机测试通过回环接口检查发现、实时指标、C/Python HMAC 一致性、关机预约、过期、重放与取消。测试不会调用 Windows 关机。
- 最终用大万开发安装工具测试加载和退出，并在设备上检查首次连接、读数刷新及触控。安装方法见 [操作指南](user-guide.md)。

在本机 MinGW 的 GCC 环境执行上位机协议测试。

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
gcc -std=c11 -Wall -Wextra -Werror -shared crypto.c -o .build/pc-crypto.dll
.\.venv\Scripts\python.exe -B verify_host.py .build/pc-crypto.dll
```
