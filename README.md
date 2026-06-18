# M5Unit - ENV

## Overview

Library for Unit ENV using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

For information on legacy libraries, [see below](#legacy-library).

### SKU:U103
Unit CO2 is a digital air CO2 concentration detection unit, built-in with Sensirion's SCD40 sensor and power step-down circuit, using I2C communication.

### SKU:U104
Unit CO2L is a digital air CO2 concentration detection unit with a low power consumption mode for single measurements.

### SKU:U001-C
Unit ENV-III is an environmental sensor that integrates SHT30 and QMP6988 internally to detect temperature, humidity, and atmospheric pressure data.

### SKU:U001-D
Unit ENV-IV is an environmental sensor unit embedded with SHT40 and BMP280 sensors for measuring temperature, humidity, and atmospheric pressure data.

### SKU:U169
The Unit ENV-Pro sensor is a highly integrated environmental detection unit, equipped with the BME688 sensor solution.

### SKU:U088
The Unit Mini TVOC/eCO2 is a digital multi-pixel gas sensor unit with an integrated SGP30, primarily designed to measure various VOCs (Volatile Organic Compounds) and H2 in the air.

### SKU:U090
Unit Mini BPS is a barometer unit that uses the Bosch BMP280 pressure sensor for measuring atmospheric pressure and altitude estimation.

### SKU:U090-B
Unit Mini BPS v1.1 is a digital barometric pressure sensor unit that uses the QMP6988 barometric pressure sensor and utilizes an I2C communication interface.

### SKU:U053-D
Hat ENV-III is a multifunctional environmental sensor compatible with the M5StickC series. It integrates SHT30 and QMP6988 to detect temperature, humidity, and atmospheric pressure data.

### SKU:U070
Hat Yun is a cloud-shaped multifunctional environmental information collection base. It is equipped with a temperature and humidity sensor SHT20, a pressure sensor BMP280, a photoresistor, and 14 RGB LEDs.

## Related Link
- [Unit CO2 - Document & Datasheet](https://docs.m5stack.com/en/unit/co2)
- [Unit CO2L - Document & Datasheet](https://docs.m5stack.com/en/unit/CO2L)
- [Unit ENVIII - Document & Datasheet](https://docs.m5stack.com/en/unit/envIII)
- [Unit ENVIV - Document & Datasheet](https://docs.m5stack.com/en/unit/Unit_ENV-IV)
- [Unit ENVPro - Document & Datasheet](https://docs.m5stack.com/en/unit/ENV%20Pro%20Unit)
- [UnitTVOC/eCO2 & Datasheet](https://docs.m5stack.com/en/unit/tvoc)
- [Unit BPS - Document & Datasheet](https://docs.m5stack.com/en/unit/bps)
- [Unit BPS(QMP6988) - Document & Datasheet](https://docs.m5stack.com/en/unit/BPS%28QMP6988%29)
- [Hat ENVIII - Document & Datasheet](https://docs.m5stack.com/en/hat/hat_envIII)
- [Hat Yun - Document & Datasheet](https://docs.m5stack.com/en/hat/hat-yun)


## Required Libraries
- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

The Bosch library is required by ENVPro to obtain values that cannot be obtained without using the Bosch library.
- [Bosch-BME68x-Library](https://github.com/boschsensortec/Bosch-BME68x-Library)
- [Bosch-BSEC2-Library](https://github.com/boschsensortec/Bosch-BSEC2-Library) (Excluding ESP32-C6 [NanoC6 / NessoN1] and ESP32-P4 [Tab5])


## License

- [M5Unit-ENV - MIT](LICENSE)


## Examples
See also [examples/UnitUnified](examples/UnitUnified)

### For ArduinoIDE settings
The UnitENVIII example supports both Unit and Hat variants. Select the variant by uncommenting the appropriate `#define` at the top of the example sketch.

```cpp
#if !defined(USING_UNIT_ENV3) && !defined(USING_HAT_ENV3)
// For Unit ENVIII (U001-C)
// #define USING_UNIT_ENV3
// For Hat ENVIII (U053-D)
// #define USING_HAT_ENV3
#endif
```

## Doxygen document
[GitHub Pages](https://m5stack.github.io/M5Unit-ENV/)

If you want to generate documents on your local machine, execute the following command

```
bash docs/doxy.sh
```

It will output it under docs/html  
If you want to output Git commit hashes to html, do it for the git cloned folder.

### Required
- [Doxygen](https://www.doxygen.nl/)
- [Git](https://git-scm.com/) (Output commit hash to html)



---
## Legacy library

The legacy library provides standalone sensor drivers that do not depend on M5UnitUnified. When using M5UnitUnified, do not use it at the same time as the legacy library.

Contains M5Stack-**UNIT ENV & Hat ENV & UNIT BPS & UNIT CO2** series related case programs.

Unit ENV is an environmental sensor that integrates DHT12 and BMP280 internally to detect temperature, humidity, and atmospheric pressure data.

Unit ENV II integrates SHT30 and BMP280. Unit ENV III integrates SHT30 and QMP6988.

Unit Mini BPS uses the Bosch BMP280 pressure sensor. Unit Mini BPS v1.1 uses QMP6988 to measure atmospheric pressure and altitude estimation.

Hat ENV II and Hat ENV III are Hat form factor versions of ENV II and ENV III for M5StickC series.

Unit CO2 is a digital air CO2 concentration detection unit, built-in with Sensirion's SCD40 sensor.

### Related Link
- [Unit ENV - Document & Datasheet](https://docs.m5stack.com/en/unit/env)
- [Unit ENVII - Document & Datasheet](https://docs.m5stack.com/en/unit/envII)
- [Hat ENVII - Document & Datasheet](https://docs.m5stack.com/en/hat/hat_envII)

### Include file
```cpp
#include <M5UnitENV.h>
```

### Required Libraries

- [Adafruit_BMP280_Library](https://github.com/adafruit/Adafruit_BMP280_Library)
- [Adafruit_Sensor](https://github.com/adafruit/Adafruit_Sensor)
- [Sensirion I2C SCD4x](https://github.com/Sensirion/arduino-i2c-scd4x)
- [Sensirion I2C SHT4x](https://github.com/Sensirion/arduino-i2c-sht4x)
- [Sensirion Core](https://github.com/Sensirion/arduino-core)


