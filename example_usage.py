#!/usr/bin/env python3
"""Minimal example: drive the RF attenuator board from a test.

Run with the board's CDC port as the argument, or set ATTENUATOR_PORT:

    ./example_usage.py /dev/ttyACM0
    ATTENUATOR_PORT=/dev/ttyACM0 ./example_usage.py
"""

import os
import sys

from rf_attenuator import Attenuator, AttenuatorError


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("ATTENUATOR_PORT")
    if port is None:
        sys.exit("usage: example_usage.py <serial-port>")

    # The context manager closes the port, including when set_db() raises.
    with Attenuator(port) as att:
        print("firmware:", att.idn())

        # Set the attenuation.
        att.set_db(20)
        print("set 20.0 dB")

        # The board has no readback path on the RF side, so get_db() reports
        # the value the firmware last wrote. If it matches, the control path
        # and the parser are working; it is not proof the RF path is correct.
        current = att.get_db()
        print("read back:", current, "dB")
        assert current == 20, f"expected 20.0, got {current}"

        # Invalid input is rejected host-side, before anything is sent.
        try:
            att.set_db(99)
        except ValueError as exc:
            print("rejected 99 dB:", exc)

        # An error reply from the firmware surfaces as an exception. This
        # bypasses the public API on purpose: the class never sends a command
        # it expects to fail, so this is the only way to see a firmware error.
        try:
            att._query("BOGUS")
        except AttenuatorError as exc:
            print("rejected unknown command:", exc)

        # Return the path to minimum loss when the run ends, so a failure part
        # way through does not leave the RF path blocked.
        att.set_db(0)
        print("restored to 0.0 dB")


if __name__ == "__main__":
    main()