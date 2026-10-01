from rf_attenuator import Attenuator

with Attenuator("/dev/ttyACM0") as att:
    att.set_db(20)
    assert att.get_db() == 20
    print("attenuation set to", att.get_db(), "dB (programmable part only)")