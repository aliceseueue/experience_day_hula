import os
import pytest
from rf_attenuator import Attenuator, AttenuatorError

ATT_MAX_HALF_DB = 126


class FakeSerial:
    """Mimics the firmware protocol in src/proto.c, including its error codes.

    Kept deliberately strict: if the firmware's validation changes, these
    expectations should change with it.
    """
    def __init__(self):
        self.half = ATT_MAX_HALF_DB
        self.ready = True
        self._reply = b""

    def reset_input_buffer(self):
        self._reply = b""

    def write(self, data):
        if not self.ready:
            self._reply = b"ERR hw\n"
            return
        cmd = data.decode().strip()
        if cmd == "*IDN?":
            r = "OK rf-atten fw 0.1.0"
        elif cmd == "ATT?":
            r = f"OK {self.half // 2}.{(self.half % 2) * 5}"
        elif cmd == "ATT":
            r = "ERR syntax"
        elif cmd.startswith("ATT "):
            r = self._set(cmd[4:])
        else:
            r = "ERR unknown_cmd"
        self._reply = (r + "\n").encode()

    def _set(self, arg):
        if arg.startswith("-"):
            return "ERR syntax"
        parts = arg.split(".")
        if len(parts) > 2 or not parts[0].isdigit() or not parts[0]:
            return "ERR syntax"
        if len(parts[0]) > 3:
            return "ERR range"
        if len(parts) == 2:
            if parts[1] == "5":
                frac = 1
            elif parts[1] == "0":
                frac = 0
            elif parts[1].isdigit():
                return "ERR step"
            else:
                return "ERR syntax"
        else:
            frac = 0
        half = int(parts[0]) * 2 + frac
        if half > ATT_MAX_HALF_DB:
            return "ERR range"
        self.half = half
        return "OK"

    def readline(self):
        r, self._reply = self._reply, b""
        return r

    def close(self):
        pass


@pytest.fixture
def ser():
    return FakeSerial()


@pytest.fixture
def att(ser):
    return Attenuator("fake", _serial=ser)


@pytest.mark.parametrize("db", [0, 0.5, 20, 31.5, 32, 62.5, 63])
def test_set_and_get(att, db):
    att.set_db(db)
    assert att.get_db() == db


def test_set_sends_parseable_half_step_form(att, ser):
    att.set_db(2.5)
    assert ser.half == 5


def test_idn(att):
    assert "rf-atten" in att.idn()


def test_bad_input_does_not_change_state(att):
    att.set_db(10)
    for bad in (99, 63.5, 20.3, -1):
        with pytest.raises(ValueError):
            att.set_db(bad)
    assert att.get_db() == 10


@pytest.mark.parametrize("bad", [-1, 63.5, 100, 20.3, float("nan"),
                                 float("inf"), "20", None, True, False])
def test_invalid_inputs_rejected(att, bad):
    with pytest.raises(ValueError):
        att.set_db(bad)


def test_closed_connection_is_reported(att):
    att.close()
    with pytest.raises(AttenuatorError):
        att.get_db()


def test_device_error_is_reported(ser):
    ser.write = lambda data: setattr(ser, "_reply", b"ERR hw\n")
    with pytest.raises(AttenuatorError):
        Attenuator("fake", _serial=ser).set_db(5)


def test_not_ready_device_reports_error(ser):
    ser.ready = False
    a = Attenuator("fake", _serial=ser)
    with pytest.raises(AttenuatorError):
        a.get_db()


def test_timeout_is_reported(ser):
    ser.write = lambda data: None
    with pytest.raises(AttenuatorError, match="timeout"):
        Attenuator("fake", _serial=ser).get_db()


@pytest.mark.skipif("ATTENUATOR_PORT" not in os.environ, reason="no hardware")
def test_hardware_roundtrip():
    with Attenuator(os.environ["ATTENUATOR_PORT"]) as a:
        for db in (0, 20, 63):
            a.set_db(db)
            assert a.get_db() == db
