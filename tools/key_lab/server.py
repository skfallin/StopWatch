"""Loopback-only WebAuthn/PRF test. No account, PIN, password or PRF output is stored."""
import argparse
import json
import secrets
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from fido2.server import Fido2Server
from fido2.webauthn import AttestedCredentialData, PublicKeyCredentialRpEntity, AuthenticationResponse
from fido2.utils import websafe_decode, websafe_encode

ROOT = Path(__file__).resolve().parents[2]
ORIGIN = "http://localhost:8765"
TOKEN = secrets.token_urlsafe(32)
server = Fido2Server(PublicKeyCredentialRpEntity(id="localhost", name="StopWatch · prova locale"),
                    verify_origin=lambda origin: origin == ORIGIN)
pending = {}
credential = None
counter = 0
store = ROOT / ".venv/key-lab-public.json"
if store.exists():
    saved = json.loads(store.read_text())
    credential = AttestedCredentialData(websafe_decode(saved["credential"]))
    counter = saved["counter"]

def save():
    # Only the public credential and signature counter survive a server/device restart.
    store.write_text(json.dumps({"credential": websafe_encode(bytes(credential)), "counter": counter}))

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, status, value, content_type="application/json"):
        data = value if isinstance(value, bytes) else json.dumps(value).encode()
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.headers.get("Host") != "localhost:8765":
            return self.reply(403, {"error": "Host non valido"})
        if self.path == "/":
            page = Path(__file__).with_name("index.html").read_text().replace("__TOKEN__", TOKEN)
            return self.reply(200, page.encode(), "text/html; charset=utf-8")
        if self.path == "/status":
            return self.reply(200, {"credential": credential is not None})
        self.reply(404, {})

    def do_POST(self):
        global credential, counter
        if self.headers.get("Host") != "localhost:8765" or self.headers.get("Origin") != ORIGIN or self.headers.get("X-Lab") != TOKEN:
            return self.reply(403, {"error": "Richiesta non autorizzata"})
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 16384:
                raise ValueError("Dimensione non valida")
            body = json.loads(self.rfile.read(length))
            operation, step = self.path.strip("/").split("/")
            if operation not in ("register", "get"):
                raise ValueError("Operazione non valida")
            if step == "begin":
                if operation == "register":
                    options, state = server.register_begin(
                        {"id": secrets.token_bytes(32), "name": "Prova StopWatch", "displayName": "Solo test locale"},
                        resident_key_requirement="required", user_verification="required",
                        authenticator_attachment="cross-platform", extensions={"prf": {}})
                else:
                    if credential is None:
                        raise ValueError("Crea prima la passkey di prova")
                    options, state = server.authenticate_begin(user_verification="required")
                request_id = secrets.token_urlsafe(24)
                pending.clear()
                pending[request_id] = (operation, state, time.monotonic())
                return self.reply(200, {"request": request_id, "options": dict(options)})
            if step != "complete":
                raise ValueError("Passaggio non valido")
            op, state, started = pending.pop(body["request"])
            if op != operation or time.monotonic() - started > 120:
                raise ValueError("Prova scaduta")
            response = body["credential"]
            if operation == "register":
                auth = server.register_complete(state, response)
                credential = auth.credential_data
                counter = auth.counter
            else:
                parsed = AuthenticationResponse.from_dict(response)
                server.authenticate_complete(state, [credential], parsed)
                auth = parsed.response.authenticator_data
                if counter and auth.counter <= counter:
                    raise ValueError("Contatore firma non crescente")
                counter = auth.counter
            save()
            print(f"OK: {operation}, origine/challenge/UV/UP verificati, contatore={counter}", flush=True)
            self.reply(200, {"verified": True, "counter": counter})
        except (ValueError, KeyError, TypeError) as error:
            self.reply(400, {"error": str(error)[:180]})

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    print(f"Prova locale: {ORIGIN} — Ctrl+C per terminare", flush=True)
    HTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
