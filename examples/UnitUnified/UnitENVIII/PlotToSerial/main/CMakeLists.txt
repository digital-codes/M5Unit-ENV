# Dependencies are pulled in via main/idf_component.yml.
idf_component_register(
    SRCS "PlotToSerial.cpp"
    INCLUDE_DIRS ".")

# ENVIII variant (UnitENV3 / HatENV3) is selected via Kconfig (main/Kconfig.projbuild ->
# common/Kconfig.variant). Include the CONFIG_* -> USING_* mapping after the register call.
include("${CMAKE_CURRENT_LIST_DIR}/../../../common/variant.cmake")
