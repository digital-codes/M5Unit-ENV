/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitTVOC
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedENV.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware I2C wiring helpers

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitTVOC unit;
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
    M5.Log.printf("SGP30 measurement starts 15 seconds after begin\n");
    lcd.printf("SGP30 measurement starts 15 seconds after begin");
}

void loop()
{
    M5.update();
    Units.update();

    // SGP30 measurement starts 15 seconds after begin.
    if (unit.updated()) {
        // Can be checked on serial plotters
        M5.Log.printf(">CO2eq:%u\n>TVOC:%u\n", unit.co2eq(), unit.tvoc());
        lcd.startWrite();
        lcd.fillRect(0, 0, lcd.width(), lcd.fontHeight() * 2, TFT_BLACK);
        lcd.setCursor(0, 0);
        lcd.printf("CO2eq:%u\nTVOC:%u", unit.co2eq(), unit.tvoc());
        lcd.endWrite();
    }
}
