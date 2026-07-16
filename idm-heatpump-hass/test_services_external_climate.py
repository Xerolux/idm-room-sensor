import pytest

@pytest.mark.parametrize("circuit,address", [("A",1650),("B",1652),("C",1654),("D",1656),("E",1658),("F",1660),("G",1662)])
def test_external_temperature_address_map(circuit,address):
    from services_implementation import EXT_ROOM_TEMP
    assert EXT_ROOM_TEMP[circuit] == address

def test_external_humidity_address():
    from services_implementation import EXT_HUMIDITY
    assert EXT_HUMIDITY == 1692
