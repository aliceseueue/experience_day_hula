import math
import serial


class AttenuatorError(Exception):
    pass


class Attenuator:
    """Controls the RF attenuation board over its USB CDC port.

    set_db() sets the *programmable* attenuation (U3 + U2), 0..63 dB in
    0.5 dB steps. The fixed attenuator (U1) and insertion loss are not
    included. get_db() returns the last value the firmware wrote; the
    hardware has no readback.
    """

    MIN_DB, MAX_DB, STEP_DB = 0.0, 63.0, 0.5

    def __init__(self, port=None, timeout=1.0, _serial=None):
        if _serial is None and port is None:
            raise ValueError("port is required unless _serial is given")
        self._ser = _serial if _serial is not None else serial.Serial(port, 115200, timeout=timeout)
        self._closed = False

    def close(self):
        if not self._closed:
            self._closed = True
            self._ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _query(self, cmd):
        if self._closed:
            raise AttenuatorError("connection is closed")
        self._ser.reset_input_buffer()
        self._ser.write(f"{cmd}\n".encode())
        reply = self._ser.readline().decode(errors="replace").strip()
        if not reply:
            raise AttenuatorError(f"no reply to {cmd!r} (timeout)")
        if not reply.startswith("OK"):
            raise AttenuatorError(f"{cmd!r} failed: {reply}")
        return reply[2:].strip()

    def idn(self):
        return self._query("*IDN?")

    def set_db(self, db):
        # bool is an int subclass, and True would silently mean 1.0 dB.
        if isinstance(db, bool) or not isinstance(db, (int, float)):
            raise ValueError(f"attenuation must be a number, got {db!r}")
        if not math.isfinite(db):
            raise ValueError(f"attenuation must be finite, got {db!r}")
        if not self.MIN_DB <= db <= self.MAX_DB:
            raise ValueError(f"{db} dB out of range {self.MIN_DB}..{self.MAX_DB}")
        half = db * 2
        if half != round(half):
            raise ValueError(f"{db} dB is not a multiple of {self.STEP_DB} dB")
        # %.1f always emits the "N.5"/"N.0" form the firmware parser expects,
        # so no float repr such as "2.6750000000000003" reaches the wire.
        self._query(f"ATT {db:.1f}")

    def get_db(self):
        return float(self._query("ATT?"))
