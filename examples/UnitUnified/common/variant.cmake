# Map the Kconfig "Target unit / board" choice (common/Kconfig.variant) to the source-level USING_*
# macro that both the Arduino and ESP-IDF builds use. Include this from an example's
# main/CMakeLists.txt *after* idf_component_register() (it needs ${COMPONENT_LIB}). Only the ENVIII
# example needs a variant; other examples do not include this file.
# Default (nothing selected): UnitENV3 (GROVE).
if(CONFIG_EXAMPLE_USING_HAT_ENV3)
    set(M5UNIT_VARIANT USING_HAT_ENV3)
else()
    set(M5UNIT_VARIANT USING_UNIT_ENV3)
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${M5UNIT_VARIANT})
