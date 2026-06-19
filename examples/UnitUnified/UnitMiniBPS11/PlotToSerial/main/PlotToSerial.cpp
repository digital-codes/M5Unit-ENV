/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitMiniBPS11
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedENV.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware I2C wiring helpers

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitMiniBPS11 unit;
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
        // Can be checked on serial plotters
        M5.Log.printf(">Temperature:%2.2f\n>Pressure:%.2f\n", unit.temperature(), unit.pressure() * 0.01f);
        lcd.startWrite();
        lcd.fillRect(0, 0, lcd.width(), lcd.fontHeight() * 2, TFT_BLACK);
        lcd.setCursor(0, 0);
        lcd.printf("Temp:%2.2f\nPressure:%.2f hPa", unit.temperature(), unit.pressure() * 0.01f);
        lcd.endWrite();
    }

    if (M5.BtnA.wasClicked()) {
        unit.stopPeriodicMeasurement();

        m5::unit::qmp6988::Data d{};
        if (unit.measureSingleshot(d)) {
            M5.Log.printf("== Singleshot Temp:%2.2f Pressure:%.2f\n", d.temperature(), d.pressure() * 0.01f);
        } else {
            M5_LOGW("Singleshot failed");
        }

        unit.startPeriodicMeasurement();
    }
}
