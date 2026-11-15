"""Windows companion for the OneOS PC status display."""

import argparse
import hashlib
import hmac
import ipaddress
import os
from pathlib import Path
import re
import secrets
import socket
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import psutil


PORT = 47831
ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
NONCE = re.compile(rb"[0-9a-f]{32}")
PROOF = re.compile(rb"[0-9a-f]{64}")
PRIVATE = tuple(map(ipaddress.ip_network, ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")))
MAGIC = b"WMPC1\n"


def private(address):
    try:
        candidate = ipaddress.IPv4Address(address)
    except ipaddress.AddressValueError:
        return False
    return any(candidate in network for network in PRIVATE)


def addresses():
    interfaces = psutil.net_if_stats()
    return sorted({addr.address for name, addrs in psutil.net_if_addrs().items()
                   if name in interfaces and interfaces[name].isup
                   for addr in addrs if addr.family == socket.AF_INET and private(addr.address)})


def windows_path(*parts):
    return Path(os.environ.get("SystemRoot", r"C:\Windows")).joinpath(*parts)


def power_off():
    subprocess.run([str(windows_path("System32", "shutdown.exe")), "/s", "/t", "0"],
                   check=True, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)


class Companion:
    def __init__(self):
        self.name = re.sub(r"[^-A-Za-z0-9_.]", "-", socket.gethostname())[:48] or "Windows-PC"
        self.key = "".join(secrets.choice(ALPHABET) for _ in range(20)).encode("ascii")
        self.lock = threading.Lock()
        self.challenges = {}
        self.allowed = False
        self.deadline = None
        self.metrics = None
        self.disk = str(Path(os.environ.get("SystemDrive", "C:") + "\\"))
        candidates = (windows_path("System32", "nvidia-smi.exe"),
                      Path(os.environ.get("ProgramFiles", r"C:\Program Files")) /
                      "NVIDIA Corporation" / "NVSMI" / "nvidia-smi.exe")
        self.nvidia = next((path for path in candidates if path.is_file()), None)
        self.previous = psutil.net_io_counters()
        self.previous_at = time.monotonic()
        self.gpu = (-1, -1)
        self.next_gpu = 0
        psutil.cpu_percent()

    def sample(self):
        now = time.monotonic()
        if self.nvidia and now >= self.next_gpu:
            self.next_gpu = now + 5
            try:
                data = subprocess.check_output(
                    [str(self.nvidia), "--query-gpu=utilization.gpu,temperature.gpu",
                     "--format=csv,noheader,nounits"], timeout=1.5,
                    creationflags=subprocess.CREATE_NO_WINDOW, stderr=subprocess.DEVNULL)
                load, temp = data.splitlines()[0].split(b",")
                self.gpu = (int(load) if load.strip().isdigit() else -1,
                            int(temp) if temp.strip().isdigit() else -1)
            except (OSError, IndexError, ValueError, subprocess.SubprocessError):
                self.gpu = (-1, -1)
        net = psutil.net_io_counters()
        elapsed = max(now - self.previous_at, 0.001)
        received = sent = -1
        if net and self.previous:
            received = round(max(0, net.bytes_recv - self.previous.bytes_recv) / elapsed / 1024)
            sent = round(max(0, net.bytes_sent - self.previous.bytes_sent) / elapsed / 1024)
        self.previous, self.previous_at = net, now
        memory = psutil.virtual_memory()
        disk = psutil.disk_usage(self.disk)
        metrics = (round(psutil.cpu_percent()), round(memory.percent), memory.used // 1048576,
                   memory.total // 1048576, round(disk.percent), disk.free // 1048576,
                   received, sent, max(0, int(time.time() - psutil.boot_time())), *self.gpu)
        with self.lock:
            self.metrics = metrics

    def status(self):
        with self.lock:
            if self.metrics is None:
                return None
            fields = (self.name, *self.metrics, int(self.allowed))
        return MAGIC + ("\n".join(map(str, fields)) + "\n").encode("ascii")

    def sign(self, kind, client, server):
        return hmac.new(self.key, b" ".join((b"WMPC1", kind, client, server)), hashlib.sha256).hexdigest().encode()

    def challenge(self, client):
        with self.lock:
            if not self.allowed or self.deadline is not None:
                return 403, b""
            now = time.monotonic()
            self.challenges = {nonce: ticket for nonce, ticket in self.challenges.items() if ticket[1] > now}
            if len(self.challenges) >= 64:
                self.challenges.pop(next(iter(self.challenges)))
            server = secrets.token_hex(16).encode()
            self.challenges[server] = (client, now + 20)
            return 200, MAGIC + server + b"\n" + self.sign(b"challenge", client, server) + b"\n"

    def schedule(self, client, server, proof):
        with self.lock:
            if not self.allowed or self.deadline is not None:
                return 403, b""
            ticket = self.challenges.get(server)
            if (not ticket or ticket[0] != client or ticket[1] <= time.monotonic()
                    or not hmac.compare_digest(proof, self.sign(b"shutdown", client, server))):
                return 403, b""
            self.challenges.clear()
            self.deadline = time.monotonic() + 15
            return 200, MAGIC + self.sign(b"scheduled", client, server) + b"\n"

    def allow(self, allowed):
        with self.lock:
            self.allowed = allowed
            if not allowed:
                self.deadline = None
                self.challenges.clear()

    def cancel(self):
        with self.lock:
            self.deadline = None
            self.challenges.clear()

    def remaining(self):
        with self.lock:
            if self.deadline is None:
                return None
            delay = self.deadline - time.monotonic()
            if delay <= 0:
                self.deadline = None
                self.allowed = False
            return delay


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        self.request.settimeout(2)
        super().setup()

    def log_message(self, *_):
        pass

    def reply(self, status, payload=b""):
        self.send_response(status)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        app = self.server.companion
        if self.path == "/wm-pc/v1/identify":
            self.reply(200, MAGIC + app.name.encode("ascii") + b"\n")
        elif self.path == "/wm-pc/v1/status":
            status = app.status()
            self.reply(200, status) if status else self.reply(503)
        else:
            self.reply(404)

    def do_POST(self):
        if self.path not in ("/wm-pc/v1/challenge", "/wm-pc/v1/shutdown"):
            return self.reply(404)
        challenge = self.path.endswith("/challenge")
        if self.headers.get_all("Content-Length") != (["32"] if challenge else ["130"]):
            return self.reply(400)
        if self.headers.get("Transfer-Encoding") or self.headers.get("Origin"):
            return self.reply(400)
        data = self.rfile.read(32 if challenge else 130)
        if challenge and NONCE.fullmatch(data):
            return self.reply(*self.server.companion.challenge(data))
        fields = data.split(b"\n")
        if (not challenge and len(fields) == 3 and NONCE.fullmatch(fields[0])
                and NONCE.fullmatch(fields[1]) and PROOF.fullmatch(fields[2])):
            return self.reply(*self.server.companion.schedule(*fields))
        self.reply(400)


class Server(ThreadingHTTPServer):
    def __init__(self, address, companion, port=PORT):
        self.companion = companion
        self.slots = threading.BoundedSemaphore(8)
        super().__init__((address, port), Handler)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()

    def handle_error(self, request, client_address):
        # Disconnected discovery probes are expected on a network.
        import sys
        if not isinstance(sys.exc_info()[1], (ConnectionError, TimeoutError)):
            super().handle_error(request, client_address)


def sample_until_stopped(app, stop):
    app.sample()
    while not stop.wait(1):
        app.sample()


def window(initial_address=None):
    import tkinter as tk
    from tkinter import ttk

    root = tk.Tk()
    root.title("大万状态监控屏 · Windows 上位机")
    root.resizable(False, False)
    panel = ttk.Frame(root, padding=24)
    panel.grid(sticky="nsew")
    heading = socket.gethostname()
    ttk.Label(panel, text=heading, font=("Microsoft YaHei UI", 18)).grid(sticky="w")
    ttk.Label(panel, text="电脑与大万连入同一局域网后，选择本机 IPv4。").grid(pady=(8, 16), sticky="w")
    address = tk.StringVar()
    choices = ttk.Combobox(panel, textvariable=address, width=26, state="readonly")
    choices.grid(row=2, column=0, sticky="w")
    enabled = tk.BooleanVar(value=False)
    status = tk.StringVar(value="服务尚未启动")
    code = tk.StringVar(value="启动服务并允许关机后显示")
    stats = tk.StringVar(value="")
    server = app = stop_event = None

    def refresh():
        found = addresses()
        choices["values"] = found
        if address.get() not in found:
            address.set(found[0] if found else "")

    def permit():
        if app:
            app.allow(enabled.get())
            code.set(app.key.decode() if enabled.get() else "未启用")

    def cancel():
        if app:
            app.cancel()
            status.set("已取消计划关机")

    def stop():
        nonlocal server, app, stop_event
        if server:
            app.allow(False)
            stop_event.set()
            server.shutdown()
            server.server_close()
            server = app = stop_event = None
        enabled.set(False)
        permission.configure(state="disabled")
        abort.configure(state="disabled")
        choices.configure(state="readonly")
        reload_button.configure(state="normal")
        toggle.configure(text="启动服务")
        code.set("启动服务并允许关机后显示")
        stats.set("")
        status.set("服务已停止")

    def start():
        nonlocal app, server, stop_event
        if server:
            return stop()
        if not address.get():
            status.set("未发现可用的私有 IPv4")
            return
        app = Companion()
        try:
            server = Server(address.get(), app)
        except OSError as error:
            app = None
            status.set(f"监听失败 · {error}")
            return
        stop_event = threading.Event()
        threading.Thread(target=sample_until_stopped, args=(app, stop_event), daemon=True).start()
        threading.Thread(target=server.serve_forever, daemon=True).start()
        permission.configure(state="normal")
        choices.configure(state="disabled")
        reload_button.configure(state="disabled")
        toggle.configure(text="停止服务")
        parts = address.get().split(".")
        status.set(f"服务已启动 · {address.get()}\n大万可搜索网段 {'.'.join(parts[:3])}，或直连完整 IP。")
        permit()

    def tick():
        if app:
            delay = app.remaining()
            abort.configure(state="normal" if delay is not None and delay > 0 else "disabled")
            if delay is not None:
                if delay > 0:
                    status.set(f"大万已预约关机 · {int(delay) + 1} 秒后执行")
                else:
                    enabled.set(False)
                    permit()
                    try:
                        power_off()
                        status.set("Windows 已受理关机")
                    except (OSError, subprocess.SubprocessError):
                        status.set("Windows 拒绝关机，可检查系统权限")
            with app.lock:
                if app.metrics:
                    stats.set(f"CPU {app.metrics[0]}%     内存 {app.metrics[1]}%     系统盘 {app.metrics[4]}%")
        root.after(200, tick)

    reload_button = ttk.Button(panel, text="刷新地址", command=refresh)
    reload_button.grid(row=2, column=1, padx=6)
    toggle = ttk.Button(panel, text="启动服务", command=start)
    toggle.grid(row=2, column=2)
    ttk.Separator(panel).grid(row=3, columnspan=3, pady=18, sticky="ew")
    permission = ttk.Checkbutton(panel, text="允许大万预约关机", variable=enabled, command=permit, state="disabled")
    permission.grid(row=4, columnspan=3, sticky="w")
    ttk.Label(panel, text="配对码").grid(row=5, sticky="w", pady=(10, 2))
    ttk.Entry(panel, textvariable=code, state="readonly", font=("Consolas", 12)).grid(row=6, columnspan=3, sticky="ew")
    ttk.Label(panel, textvariable=stats).grid(row=7, columnspan=3, sticky="w", pady=(16, 4))
    ttk.Label(panel, textvariable=status, wraplength=380).grid(row=8, columnspan=3, sticky="w")
    abort = ttk.Button(panel, text="取消关机", command=cancel, state="disabled")
    abort.grid(row=9, columnspan=3, sticky="e", pady=(12, 0))

    def close():
        stop()
        root.destroy()

    refresh()
    if initial_address is not None:
        if initial_address in choices["values"]:
            address.set(initial_address)
            start()
        else:
            status.set("指定的 IPv4 已不在本机接口列表中")
    root.protocol("WM_DELETE_WINDOW", close)
    root.after(200, tick)
    root.mainloop()


def main():
    parser = argparse.ArgumentParser()
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--serve", metavar="PRIVATE_IP", help="Run monitoring only on the specified IPv4")
    modes.add_argument("--start", metavar="PRIVATE_IP", help="Open the window and start monitoring on this IPv4")
    arguments = parser.parse_args()
    if not arguments.serve:
        return window(arguments.start)
    if not private(arguments.serve):
        parser.error("--serve requires a private IPv4")
    app = Companion()
    stop = threading.Event()
    with Server(arguments.serve, app) as server:
        threading.Thread(target=sample_until_stopped, args=(app, stop), daemon=True).start()
        print(f"WMPC1 {arguments.serve}:{PORT}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            stop.set()


if __name__ == "__main__":
    main()
