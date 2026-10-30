"""Read-only/negative USB checks. Leave Chiave USB open; do not press B during this test."""
import threading
from hashlib import sha256
from fido2.hid import CtapHidDevice
from fido2.ctap2 import Ctap2
from fido2.ctap import CtapError

def reject(operation, expected):
    try:
        operation()
    except CtapError as error:
        assert error.code == expected, error
    else:
        raise AssertionError("Operazione accettata senza la conferma richiesta")

def main():
    devices = [d for d in CtapHidDevice.list_devices() if d.descriptor.vid == 0x303A and d.descriptor.pid == 0x1001]
    if len(devices) != 1:
        raise SystemExit("Collega un solo StopWatch e apri Chiave USB · LAB")
    device = devices[0]
    try:
        ctap = Ctap2(device)
        info = ctap.get_info()
        assert "FIDO_2_1" in info.versions and "U2F_V2" not in info.versions
        assert "hmac-secret" in info.extensions and info.options["rk"]
        assert info.pin_uv_protocols == [1, 2]
        assert info.transports == ["usb"]
        assert info.algorithms == [{"alg": -7, "type": "public-key"}]
        assert info.max_msg_size == 1300
        print("OK: FIDO2.1, credenziali residenti, PIN1/2, ES256, HMAC-secret, USB", flush=True)
        reject(lambda: ctap.get_assertion("not-the-test-rp.invalid", sha256(b"separate-rp").digest(), options={"up": False}), CtapError.ERR.NO_CREDENTIALS)
        print("OK: RP estraneo rifiutato", flush=True)
        reject(lambda: ctap.get_assertion("x"*81+".invalid", sha256(b"too-long").digest(), options={"up": False}), CtapError.ERR.INVALID_PARAMETER)
        reject(lambda: ctap.make_credential(sha256(b"missing-rp").digest(), {}, {"id": b"invalid", "name": "test"}, [{"alg": -7, "type": "public-key"}]), CtapError.ERR.MISSING_PARAMETER)
        for length in (0, 57, 58, 1300):
            data=bytes(i % 251 for i in range(length))
            assert device.ping(data)==data
        print("OK: RP non visualizzabile rifiutato e frammentazione PING fino a 1300 byte", flush=True)
        cancel = threading.Event()
        timer = threading.Timer(1, cancel.set)
        timer.start()
        try:
            reject(lambda: ctap.selection(event=cancel), CtapError.ERR.KEEPALIVE_CANCEL)
        finally:
            timer.cancel()
        print("OK: CANCEL interrompe la richiesta pendente", flush=True)
        reject(lambda: ctap.selection(), CtapError.ERR.USER_ACTION_TIMEOUT)
        print("OK: senza B la richiesta scade, senza autorizzazione", flush=True)
    finally:
        device.close()

if __name__ == "__main__":
    main()
