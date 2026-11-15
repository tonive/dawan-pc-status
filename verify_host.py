"""Exercise the live HTTP protocol and native device HMAC without shutting down Windows."""

import ctypes
import http.client
from pathlib import Path
import sys
import threading
import time
from copy import deepcopy

from host import Companion, MAGIC, Server
from dawan_theme import encode_theme, load_theme


def verify(library):
    sign = ctypes.CDLL(str(Path(library).resolve())).pc_sign
    sign.argtypes = [ctypes.c_char_p] * 5
    sign.restype = None
    app = Companion()
    app.sample()
    app.allow(True)

    def proof(kind, client, server, key=app.key):
        out = ctypes.create_string_buffer(65)
        sign(out, key, kind, client, server)
        return out.value

    with Server("127.0.0.1", app, port=0) as server_socket:
        worker = threading.Thread(target=server_socket.serve_forever)
        worker.start()
        try:
            def request(path, body=None):
                connection = http.client.HTTPConnection(*server_socket.server_address, timeout=3)
                try:
                    connection.request("GET" if body is None else "POST", "/wm-pc/v1/" + path, body)
                    response = connection.getresponse()
                    return response.status, response.read()
                finally:
                    connection.close()

            assert request("identify") == (200, MAGIC + app.name.encode() + b"\n")
            code, body = request("status")
            assert code == 200 and len(body.splitlines()) == 14
            assert all(0 <= int(body.splitlines()[field]) <= 100 for field in (2, 3, 6))
            assert request("challenge", b"incomplete")[0] == 400

            client = b"0123456789abcdef" * 2
            code, body = request("challenge", client)
            assert code == 200 and len(body) == 104
            magic, server, challenge = body.splitlines()
            assert magic == b"WMPC1"
            assert challenge == proof(b"challenge", client, server)
            wrong = proof(b"shutdown", client, server, b"Z" * 20)
            assert request("shutdown", b"\n".join((client, server, wrong)))[0] == 403
            valid = b"\n".join((client, server, proof(b"shutdown", client, server)))
            assert request("shutdown", valid) == (200, MAGIC + proof(b"scheduled", client, server) + b"\n")
            assert 0 < app.remaining() <= 15
            assert request("shutdown", valid)[0] == 403
            app.cancel()
            assert app.remaining() is None
            assert request("shutdown", valid)[0] == 403

            code, body = request("challenge", client)
            assert code == 200
            expired = body.splitlines()[1]
            with app.lock:
                app.challenges[expired] = (client, time.monotonic() - 1)
            assert request("shutdown", b"\n".join((client, expired, proof(b"shutdown", client, expired))))[0] == 403
            app.allow(False)
            assert request("challenge", client)[0] == 403
        finally:
            server_socket.shutdown()
            worker.join()


def verify_themes():
    examples = Path(__file__).resolve().parent / "skills/dawan-pc-theme/assets"
    value, wire = load_theme(examples / "aurora-terminal.wmtheme.json")
    upgraded, new_wire = load_theme(examples / "starship-console.wmtheme.json")
    assert wire.startswith(b"WMT1\n") and len(wire) <= 256
    assert wire.splitlines()[5][:2] == b"04" and wire.splitlines()[6][:2] == b"24"
    assert new_wire.startswith(b"WMT2\n") and 256 < len(new_wire) <= 512
    assert new_wire.splitlines()[3] == b"121" and new_wire.splitlines()[5][:2] == b"05"

    def rejects(base, change):
        invalid = deepcopy(base)
        change(invalid)
        try:
            encode_theme(invalid)
        except ValueError:
            return
        raise AssertionError("Accepted invalid theme")

    for change in (
        lambda theme: theme["cards"].append(theme["cards"][0]),
        lambda theme: theme["cards"][1].update(metric="cpu"),
        lambda theme: theme["palette"].update(ink=theme["palette"]["surface"]),
        lambda theme: theme.update(name="主题\n输入", source="https://example.org"),
    ):
        rejects(value, change)
    for change in (
        lambda theme: theme.update(version=1, layout="grid"),
        lambda theme: theme["cards"][0]["icon"].update(pixels=["12312312"] * 8),
        lambda theme: theme["cards"][0]["icon"].update(pixels=["........"] * 8),
        lambda theme: theme["cards"][0]["icon"].update(pixels=["........"] * 7 + ["...x...."]),
        lambda theme: theme["cards"][0]["icon"].update(colors=["#123456"] * 2),
    ):
        rejects(upgraded, change)

    app = Companion()
    with Server("127.0.0.1", app, port=0) as server:
        worker = threading.Thread(target=server.serve_forever)
        worker.start()
        try:
            def get_theme():
                connection = http.client.HTTPConnection(*server.server_address, timeout=3)
                try:
                    connection.request("GET", "/wm-pc/v2/theme")
                    response = connection.getresponse()
                    return response.status, response.read()
                finally:
                    connection.close()

            assert get_theme() == (404, b"")
            app.offer_theme(wire)
            assert get_theme() == (200, wire)
            app.offer_theme(new_wire)
            assert get_theme() == (200, new_wire)
        finally:
            server.shutdown()
            worker.join()


if __name__ == "__main__":
    verify(sys.argv[1])
    print("Discovery, Windows metrics, C/Python HMAC, shutdown scheduling, cancel, expiry and replay checks passed.")
    verify_themes()
    print("Theme validation and HTTP delivery checks passed.")
