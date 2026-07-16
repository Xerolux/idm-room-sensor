"""Proposed service implementation; adapt to current idm-heatpump-api interfaces."""
from homeassistant.exceptions import ServiceValidationError
from idm_heatpump import DataType, RegisterDef

EXT_ROOM_TEMP = {"A":1650,"B":1652,"C":1654,"D":1656,"E":1658,"F":1660,"G":1662}
EXT_HUMIDITY = 1692

async def handle_set_external_climate(coordinator, call):
    if call.data.get("acknowledge_risk") is not True:
        raise ServiceValidationError("acknowledge_risk is required")
    circuit = str(call.data.get("heating_circuit", "A")).upper()
    if circuit not in EXT_ROOM_TEMP:
        raise ServiceValidationError("invalid heating circuit")
    writes=[]
    if "room_temperature" in call.data:
        value=float(call.data["room_temperature"])
        if not -20 <= value <= 60: raise ServiceValidationError("temperature out of range")
        writes.append((RegisterDef(address=EXT_ROOM_TEMP[circuit], datatype=DataType.FLOAT, name=f"hc_{circuit.lower()}_ext_room_temp", writable=True),value))
    if "humidity" in call.data:
        value=float(call.data["humidity"])
        if not 0 <= value <= 100: raise ServiceValidationError("humidity out of range")
        writes.append((RegisterDef(address=EXT_HUMIDITY, datatype=DataType.FLOAT, name="ext_humidity", writable=True),value))
    if not writes: raise ServiceValidationError("provide room_temperature and/or humidity")
    for reg,value in writes:
        await coordinator.async_write_register(reg,value)
