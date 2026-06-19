/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitENVPro
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedENV.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware I2C wiring helpers

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitENVPro unit;
}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);
    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    // Board-aware I2C: NessoN1 -> SoftwareI2C(port_b), NanoC6/NanoH2 -> Ex_I2C, others -> Wire
    bool unit_ready = m5::unit::wiring::addI2C(Units, unit) && Units.begin();
    if (!unit_ready) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());
    lcd.fillScreen(TFT_DARKGREEN);
}

void loop()
{
    M5.update();
    Units.update();
    if (unit.updated()) {
#if defined(UNIT_BME688_USING_BSEC2)
        M5.Log.printf(">IAQ:%.2f\n>Temperature:%.2f\n>Pressure:%.2f\n>Humidity:%.2f\n>GAS:%.2f\n", unit.iaq(),
                      unit.temperature(), unit.pressure(), unit.humidity(), unit.gas());
        lcd.startWrite();
        lcd.fillRect(0, 0, lcd.width(), lcd.fontHeight() * 5, TFT_BLACK);
        lcd.setCursor(0, 0);
        lcd.printf("IAQ:%.2f\nTemperature:%.2f\nPressure:%.2f\nHumidity:%.2f\nGAS:%.2f", unit.iaq(), unit.temperature(),
                   unit.pressure(), unit.humidity(), unit.gas());
        lcd.endWrite();
#else
        M5.Log.printf(">Temperature:%.2f\n>Pressure:%.2f\n>Humidity:%.2f\n>GAS:%.2f\n", unit.temperature(),
                      unit.pressure(), unit.humidity(), unit.gas());
        lcd.startWrite();
        lcd.fillRect(0, 0, lcd.width(), lcd.fontHeight() * 4, TFT_BLACK);
        lcd.setCursor(0, 0);
        lcd.printf("Temperature:%.2f\nPressure:%.2f\nHumidity:%.2f\nGAS:%.2f", unit.temperature(), unit.pressure(),
                   unit.humidity(), unit.gas());
        lcd.endWrite();
        // m5::utility::delay(1000);
#endif
    }
}
