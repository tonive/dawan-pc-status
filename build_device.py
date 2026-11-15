"""Build the device ELF with an optional private IPv4 discovery seed."""

import argparse
from ipaddress import IPv4Address, IPv4Network
import json
from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory


def main():
    project = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, default=project.parent / "one-os_dev-skill")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--lan-seed", type=IPv4Address, help="First-run discovery hint for this device package")
    args = parser.parse_args()
    sdk = args.sdk_root.resolve()
    out = args.out.resolve()
    networks = tuple(map(IPv4Network, ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")))
    if args.lan_seed and not any(args.lan_seed in network for network in networks):
        parser.error("--lan-seed requires an RFC1918 address")
    command = [sys.executable, "-B", str(sdk / "tools/build_elf.py")]
    if not args.lan_seed:
        subprocess.run(command + [str(project), "--out", str(out)], check=True)
        return

    config = json.loads((project / "app.json").read_text(encoding="utf-8"))
    out.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="pc-status-src-", dir=out.parent) as temporary:
        staged = Path(temporary)
        for name in (*config["sources"], "app.json", "crypto.h", "theme.h"):
            shutil.copyfile(project / name, staged / name)
        app = staged / "app.c"
        source = app.read_text(encoding="utf-8")
        app.write_text(f'#define PC_LAN_SEED "{args.lan_seed}"\n' + source, encoding="utf-8")
        subprocess.run(command + [str(staged), "--out", str(out)], check=True)


if __name__ == "__main__":
    main()
