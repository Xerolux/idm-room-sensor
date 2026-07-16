"""ESPHome component for safe IDM climate-command handling."""

import zlib

from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, i2c, output, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_I2C_ID,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_OHM,
    UNIT_PERCENT,
)


AUTO_LOAD = ["binary_sensor", "output", "sensor", "text_sensor"]
DEPENDENCIES = ["i2c"]

CONF_STALE_TIMEOUT = "stale_timeout"
CONF_FALLBACK_HUMIDITY = "fallback_humidity"
CONF_FALLBACK_TEMPERATURE = "fallback_temperature"
CONF_HUMIDITY_OUTPUT = "humidity_output"
CONF_TEMPERATURE_OUTPUT = "temperature_output"
CONF_EFFECTIVE_HUMIDITY = "effective_humidity"
CONF_EFFECTIVE_TEMPERATURE = "effective_temperature"
CONF_SAFE_ACTIVE = "safe_active"
CONF_STALE = "stale"
CONF_FAULT = "fault"
CONF_OUTPUT_READY = "output_ready"
CONF_STATE = "state"
CONF_ERROR = "error"
CONF_HUMIDITY = "humidity"
CONF_TEMPERATURE = "temperature"
CONF_ACTIVE = "active"
CONF_ANALOG_OUTPUT = "analog_output"
CONF_HUMIDITY_ADDRESS = "humidity_address"
CONF_TEMPERATURE_ADDRESS = "temperature_address"
CONF_HUMIDITY_DAC_MIN_CODE = "humidity_dac_min_code"
CONF_HUMIDITY_DAC_MAX_CODE = "humidity_dac_max_code"
CONF_TEMPERATURE_RESISTANCE_MIN = "temperature_resistance_min"
CONF_TEMPERATURE_RESISTANCE_MAX = "temperature_resistance_max"
CONF_TEMPERATURE_CODE_INVERTED = "temperature_code_inverted"
CONF_ACCEPT_UNVERIFIED_KTY_CALIBRATION = (
    "accept_unverified_kty_calibration"
)
CONF_OUTPUT_FAULT = "output_fault"
CONF_OUTPUT_ERROR = "output_error"
CONF_HUMIDITY_DAC_CODE = "humidity_dac_code"
CONF_TEMPERATURE_DIGIPOT_CODE = "temperature_digipot_code"
CONF_TARGET_RESISTANCE = "target_resistance"
CONF_COMMAND_SOURCE = "command_source"
CONF_COMMAND_QUALITY = "command_quality"
CONF_SOURCE = "source"
CONF_QUALITY = "quality"
CONF_CALIBRATION_PREFERENCE_ID = "calibration_preference_id"
CONF_CALIBRATION_STATUS = "calibration_status"
CONF_CALIBRATION_VERSION = "calibration_version"
CONF_CALIBRATION_USING_FACTORY = "calibration_using_factory"

idm_bridge_ns = cg.esphome_ns.namespace("idm_bridge")
IdmBridgeOutput = idm_bridge_ns.class_("IdmBridgeOutput")
IdmBridge = idm_bridge_ns.class_("IdmBridge", cg.Component)
IdmAnalogOutput = idm_bridge_ns.class_(
    "IdmAnalogOutput", cg.Component, IdmBridgeOutput
)
SetValuesAction = idm_bridge_ns.class_("SetValuesAction", automation.Action)
ApplyFallbackAction = idm_bridge_ns.class_(
    "ApplyFallbackAction", automation.Action
)
SetOutputFaultAction = idm_bridge_ns.class_(
    "SetOutputFaultAction", automation.Action
)
SetCalibrationAction = idm_bridge_ns.class_(
    "SetCalibrationAction", automation.Action
)
ResetCalibrationAction = idm_bridge_ns.class_(
    "ResetCalibrationAction", automation.Action
)


def validate_analog_output(config):
    if (
        config[CONF_HUMIDITY_DAC_MIN_CODE]
        >= config[CONF_HUMIDITY_DAC_MAX_CODE]
    ):
        raise cv.Invalid(
            "humidity_dac_min_code must be lower than humidity_dac_max_code"
        )
    if (
        config[CONF_TEMPERATURE_RESISTANCE_MIN]
        >= config[CONF_TEMPERATURE_RESISTANCE_MAX]
    ):
        raise cv.Invalid(
            "temperature_resistance_min must be lower than "
            "temperature_resistance_max"
        )
    if not config[CONF_ACCEPT_UNVERIFIED_KTY_CALIBRATION]:
        raise cv.Invalid(
            "The KTY output is not electrically validated. Set "
            "accept_unverified_kty_calibration: true only for isolated bench "
            "testing, or omit analog_output."
        )
    return config


ANALOG_OUTPUT_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IdmAnalogOutput),
            cv.GenerateID(CONF_I2C_ID): cv.use_id(i2c.I2CBus),
            cv.Optional(
                CONF_HUMIDITY_ADDRESS, default=0x60
            ): cv.i2c_address,
            cv.Optional(
                CONF_TEMPERATURE_ADDRESS, default=0x2C
            ): cv.i2c_address,
            cv.Optional(
                CONF_HUMIDITY_DAC_MIN_CODE, default=0
            ): cv.int_range(min=0, max=4095),
            cv.Optional(
                CONF_HUMIDITY_DAC_MAX_CODE, default=4095
            ): cv.int_range(min=0, max=4095),
            cv.Optional(
                CONF_TEMPERATURE_RESISTANCE_MIN, default=650.0
            ): cv.float_range(min=100.0, max=50000.0),
            cv.Optional(
                CONF_TEMPERATURE_RESISTANCE_MAX, default=3000.0
            ): cv.float_range(min=100.0, max=50000.0),
            cv.Optional(
                CONF_TEMPERATURE_CODE_INVERTED, default=False
            ): cv.boolean,
            cv.Optional(CONF_CALIBRATION_PREFERENCE_ID): cv.hex_uint32_t,
            cv.Optional(
                CONF_ACCEPT_UNVERIFIED_KTY_CALIBRATION, default=False
            ): cv.boolean,
            cv.Optional(
                CONF_OUTPUT_FAULT
            ): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_OUTPUT_ERROR): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_HUMIDITY_DAC_CODE): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TEMPERATURE_DIGIPOT_CODE): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TARGET_RESISTANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                accuracy_decimals=1,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CALIBRATION_STATUS): (
                text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                )
            ),
            cv.Optional(CONF_CALIBRATION_VERSION): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CALIBRATION_USING_FACTORY): (
                binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                )
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_analog_output,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IdmBridge),
        cv.Optional(
            CONF_STALE_TIMEOUT, default="120s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_FALLBACK_HUMIDITY, default=80.0
        ): cv.float_range(min=0.0, max=100.0),
        cv.Optional(
            CONF_FALLBACK_TEMPERATURE, default=28.0
        ): cv.float_range(min=-20.0, max=60.0),
        cv.Optional(CONF_HUMIDITY_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_TEMPERATURE_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ANALOG_OUTPUT): ANALOG_OUTPUT_SCHEMA,
        cv.Optional(CONF_EFFECTIVE_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_EFFECTIVE_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SAFE_ACTIVE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_STALE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_FAULT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_OUTPUT_READY): binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_ERROR): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_COMMAND_SOURCE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_COMMAND_QUALITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_stale_timeout(config[CONF_STALE_TIMEOUT]))
    cg.add(
        var.set_fallback_values(
            config[CONF_FALLBACK_HUMIDITY],
            config[CONF_FALLBACK_TEMPERATURE],
        )
    )

    if CONF_HUMIDITY_OUTPUT in config:
        humidity_output = await cg.get_variable(config[CONF_HUMIDITY_OUTPUT])
        cg.add(var.set_humidity_output(humidity_output))
    if CONF_TEMPERATURE_OUTPUT in config:
        temperature_output = await cg.get_variable(config[CONF_TEMPERATURE_OUTPUT])
        cg.add(var.set_temperature_output(temperature_output))
    if CONF_ANALOG_OUTPUT in config:
        analog_config = config[CONF_ANALOG_OUTPUT]
        analog_output = cg.new_Pvariable(analog_config[CONF_ID])
        await cg.register_component(analog_output, analog_config)
        i2c_bus = await cg.get_variable(analog_config[CONF_I2C_ID])
        cg.add(analog_output.set_i2c_bus(i2c_bus))
        cg.add(
            analog_output.set_addresses(
                analog_config[CONF_HUMIDITY_ADDRESS],
                analog_config[CONF_TEMPERATURE_ADDRESS],
            )
        )
        cg.add(
            analog_output.set_humidity_code_range(
                analog_config[CONF_HUMIDITY_DAC_MIN_CODE],
                analog_config[CONF_HUMIDITY_DAC_MAX_CODE],
            )
        )
        cg.add(
            analog_output.set_temperature_resistance_range(
                analog_config[CONF_TEMPERATURE_RESISTANCE_MIN],
                analog_config[CONF_TEMPERATURE_RESISTANCE_MAX],
            )
        )
        cg.add(
            analog_output.set_temperature_code_inverted(
                analog_config[CONF_TEMPERATURE_CODE_INVERTED]
            )
        )
        storage_key = analog_config.get(CONF_CALIBRATION_PREFERENCE_ID)
        if storage_key is None:
            storage_key = zlib.crc32(
                f"idm_bridge:{analog_config[CONF_ID]}".encode()
            )
        cg.add(
            analog_output.set_preference_keys(
                storage_key ^ 0x43414C02,
                storage_key ^ 0x43414C01,
            )
        )
        cg.add(
            analog_output.set_unverified_calibration_accepted(
                analog_config[CONF_ACCEPT_UNVERIFIED_KTY_CALIBRATION]
            )
        )
        analog_entity_setters = (
            (
                CONF_OUTPUT_FAULT,
                binary_sensor.new_binary_sensor,
                "set_fault_sensor",
            ),
            (
                CONF_OUTPUT_ERROR,
                text_sensor.new_text_sensor,
                "set_error_sensor",
            ),
            (
                CONF_HUMIDITY_DAC_CODE,
                sensor.new_sensor,
                "set_humidity_code_sensor",
            ),
            (
                CONF_TEMPERATURE_DIGIPOT_CODE,
                sensor.new_sensor,
                "set_temperature_code_sensor",
            ),
            (
                CONF_TARGET_RESISTANCE,
                sensor.new_sensor,
                "set_target_resistance_sensor",
            ),
            (
                CONF_CALIBRATION_STATUS,
                text_sensor.new_text_sensor,
                "set_calibration_status_sensor",
            ),
            (
                CONF_CALIBRATION_VERSION,
                sensor.new_sensor,
                "set_calibration_version_sensor",
            ),
            (
                CONF_CALIBRATION_USING_FACTORY,
                binary_sensor.new_binary_sensor,
                "set_calibration_using_factory_sensor",
            ),
        )
        for key, factory, setter in analog_entity_setters:
            if key in analog_config:
                entity = await factory(analog_config[key])
                cg.add(getattr(analog_output, setter)(entity))
        cg.add(var.set_output_driver(analog_output))

    entity_setters = (
        (CONF_EFFECTIVE_HUMIDITY, sensor.new_sensor, "set_effective_humidity_sensor"),
        (
            CONF_EFFECTIVE_TEMPERATURE,
            sensor.new_sensor,
            "set_effective_temperature_sensor",
        ),
        (CONF_SAFE_ACTIVE, binary_sensor.new_binary_sensor, "set_safe_active_sensor"),
        (CONF_STALE, binary_sensor.new_binary_sensor, "set_stale_sensor"),
        (CONF_FAULT, binary_sensor.new_binary_sensor, "set_fault_sensor"),
        (
            CONF_OUTPUT_READY,
            binary_sensor.new_binary_sensor,
            "set_output_ready_sensor",
        ),
        (CONF_STATE, text_sensor.new_text_sensor, "set_state_sensor"),
        (CONF_ERROR, text_sensor.new_text_sensor, "set_error_sensor"),
        (
            CONF_COMMAND_SOURCE,
            text_sensor.new_text_sensor,
            "set_command_source_sensor",
        ),
        (
            CONF_COMMAND_QUALITY,
            sensor.new_sensor,
            "set_command_quality_sensor",
        ),
    )
    for key, factory, setter in entity_setters:
        if key in config:
            entity = await factory(config[key])
            cg.add(getattr(var, setter)(entity))


@automation.register_action(
    "idm_bridge.set_values",
    SetValuesAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(IdmBridge),
            cv.Required(CONF_HUMIDITY): cv.templatable(cv.float_),
            cv.Required(CONF_TEMPERATURE): cv.templatable(cv.float_),
            cv.Optional(CONF_SOURCE, default="automation"): cv.templatable(
                cv.string_strict
            ),
            cv.Optional(CONF_QUALITY, default=100): cv.templatable(
                cv.int_range(min=0, max=100)
            ),
        }
    ),
    synchronous=True,
)
async def set_values_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    humidity = await cg.templatable(config[CONF_HUMIDITY], args, cg.float_)
    temperature = await cg.templatable(config[CONF_TEMPERATURE], args, cg.float_)
    source = await cg.templatable(config[CONF_SOURCE], args, cg.std_string)
    quality = await cg.templatable(config[CONF_QUALITY], args, cg.uint8)
    cg.add(var.set_humidity(humidity))
    cg.add(var.set_temperature(temperature))
    cg.add(var.set_source(source))
    cg.add(var.set_quality(quality))
    return var


@automation.register_action(
    "idm_bridge.apply_fallback",
    ApplyFallbackAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(IdmBridge),
        }
    ),
    synchronous=True,
)
async def apply_fallback_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "idm_bridge.set_output_fault",
    SetOutputFaultAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(IdmBridge),
            cv.Required(CONF_ACTIVE): cv.templatable(cv.boolean),
        }
    ),
    synchronous=True,
)
async def set_output_fault_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    active = await cg.templatable(config[CONF_ACTIVE], args, cg.bool_)
    cg.add(var.set_active(active))
    return var


@automation.register_action(
    "idm_bridge.set_calibration",
    SetCalibrationAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(IdmAnalogOutput),
            cv.Required(CONF_HUMIDITY_DAC_MIN_CODE): cv.templatable(
                cv.int_range(min=0, max=4095)
            ),
            cv.Required(CONF_HUMIDITY_DAC_MAX_CODE): cv.templatable(
                cv.int_range(min=0, max=4095)
            ),
            cv.Required(CONF_TEMPERATURE_RESISTANCE_MIN): cv.templatable(
                cv.float_range(min=100.0, max=50000.0)
            ),
            cv.Required(CONF_TEMPERATURE_RESISTANCE_MAX): cv.templatable(
                cv.float_range(min=100.0, max=50000.0)
            ),
            cv.Required(CONF_TEMPERATURE_CODE_INVERTED): cv.templatable(
                cv.boolean
            ),
        }
    ),
    synchronous=True,
)
async def set_calibration_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    minimum_code = await cg.templatable(
        config[CONF_HUMIDITY_DAC_MIN_CODE], args, cg.uint16
    )
    maximum_code = await cg.templatable(
        config[CONF_HUMIDITY_DAC_MAX_CODE], args, cg.uint16
    )
    minimum_resistance = await cg.templatable(
        config[CONF_TEMPERATURE_RESISTANCE_MIN], args, cg.float_
    )
    maximum_resistance = await cg.templatable(
        config[CONF_TEMPERATURE_RESISTANCE_MAX], args, cg.float_
    )
    inverted = await cg.templatable(
        config[CONF_TEMPERATURE_CODE_INVERTED], args, cg.bool_
    )
    cg.add(var.set_humidity_code_min(minimum_code))
    cg.add(var.set_humidity_code_max(maximum_code))
    cg.add(var.set_temperature_resistance_min(minimum_resistance))
    cg.add(var.set_temperature_resistance_max(maximum_resistance))
    cg.add(var.set_temperature_code_inverted(inverted))
    return var


@automation.register_action(
    "idm_bridge.reset_calibration",
    ResetCalibrationAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(IdmAnalogOutput),
        }
    ),
    synchronous=True,
)
async def reset_calibration_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
