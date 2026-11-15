# 第三方组件

Windows 上位机的独立 EXE 包含下列组件。发布页随文件提供 `THIRD-PARTY-LICENSES.zip`，其中收录可再分发组件的许可文本。

| 组件 | 版本与来源 |
| --- | --- |
| CPython | [Python 3.11.15](https://www.python.org/downloads/release/python-31115/)，包含其标准库 |
| psutil | [psutil 7.2.2](https://github.com/giampaolo/psutil/tree/release-7.2.2)，BSD 3-Clause |
| Tcl / Tk | [Tcl 8.6.12](https://github.com/tcltk/tcl/tree/core-8-6-12) 和 [Tk 8.6.12](https://github.com/tcltk/tk/tree/core-8-6-12) |
| OpenSSL | [OpenSSL 3.5.6](https://github.com/openssl/openssl/tree/openssl-3.5.6)，随 Python 组件打包 |
| Microsoft Visual C++ Runtime | 随 Windows 版 Python 提供的运行库 |

独立 EXE 由 PyInstaller 6.20.0 打包，分发规则见 [PyInstaller 许可说明](https://pyinstaller.org/en/stable/license.html)。万墨 SDK 和开发工具由 [OneOS 开发工具仓库](https://gitee.com/dogbro/one-os_dev-skill) 提供，其条款由上游维护。
