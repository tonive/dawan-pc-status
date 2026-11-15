"""Exercise the live HTTP protocol and native device HMAC without shutting down Windows."""

import ctypes
import http.client
from pathlib import Path
import sys
import threading
import time

from host import Companion, MAGIC, Server


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


if __name__ == "__main__":
    verify(sys.argv[1])
    print("Discovery, Windows metrics, C/Python HMAC, shutdown scheduling, cancel, expiry and replay checks passed.")
