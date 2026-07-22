/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitSPA06

  The SPA06-003 barometer has no standalone GROVE product; it ships on Unit DoF10 at 0x76. Plug a
  DoF10 into the Grove port -- this suite adds ONLY the standalone UnitSPA06 driver at 0x76 (the
  BMI270/BMM350 on the same bus are simply left untouched), so the barometer driver is exercised
  in isolation. The full 3-child composite path is covered by M5Unit-IMU (UnitDoF10).
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_SPA06.hpp>
#include <m5_unit_component/adapter_i2c.hpp>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>

using namespace m5::unit::googletest;
using namespace m5::unit;

class TestSPA06 : public I2CComponentTestBase<UnitSPA06> {
protected:
    virtual UnitSPA06* get_instance() override
    {
        auto ptr = new m5::unit::UnitSPA06();
        if (ptr) {
            auto ccfg = ptr->component_config();
            ptr->component_config(ccfg);
        }
        return ptr;
    }
};

namespace {
constexpr uint32_t STORED_SIZE{4};  // UnitSPA06 ring buffer default (spa06::DEFAULT_STORED_SIZE)

// Timeout for collecting STORED_SIZE samples. Software I2C (M5HAL bit-banged bus, e.g. NessoN1) is far
// slower per transaction, so it gets 4x the budget and a 500 ms/cycle floor (matches the other units).
uint32_t collect_timeout(UnitSPA06* u, const uint32_t count = STORED_SIZE)
{
    auto ad              = u->asAdapter<m5::unit::AdapterI2C>(m5::unit::Adapter::Type::I2C);
    const bool is_bus    = ad && ad->implType() == m5::unit::AdapterI2C::ImplType::Bus;
    const uint32_t cycle = std::max<uint32_t>(u->interval(), 1);
    return is_bus ? std::max<uint32_t>(cycle, 500) * (count + 1) * 4 : cycle * (count + 1);
}

// Records each collected pressure so a test can assert the samples are not all identical (stuck read).
std::vector<float> g_pressure_samples;
void record_pressure(UnitSPA06* u)
{
    g_pressure_samples.push_back(u->pressure());
}

// Callback for collect_periodic_measurements: every collected sample is a finite, plausible ambient value
void check_spa06_values(UnitSPA06* u)
{
    const float hpa = u->pressure();
    EXPECT_TRUE(std::isfinite(hpa));
    EXPECT_GT(hpa, 300.0f);  // high-altitude floor
    EXPECT_LT(hpa, 1100.0f);

    const float celsius = u->temperature();
    EXPECT_TRUE(std::isfinite(celsius));
    EXPECT_GT(celsius, -20.0f);
    EXPECT_LT(celsius, 60.0f);
}
}  // namespace

TEST_F(TestSPA06, BeginStartsPeriodic)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->inPeriodic());
}

TEST_F(TestSPA06, PeriodicMeasurement)
{
    SCOPED_TRACE(ustr);

    // Default Rate16 (interval ~62ms); collect STORED_SIZE samples (software-I2C-aware timeout)
    auto r = collect_periodic_measurements(unit.get(), STORED_SIZE, collect_timeout(unit.get()), check_spa06_values);
    EXPECT_FALSE(r.timed_out);
    EXPECT_EQ(r.update_count, STORED_SIZE);
    EXPECT_FALSE(unit->empty());

    const float alt = unit->altitude();
    EXPECT_TRUE(std::isfinite(alt));
    EXPECT_GT(alt, -500.0f);  // below-sea-level margin
    EXPECT_LT(alt, 9000.0f);
}

TEST_F(TestSPA06, PeriodicSamplesVary)
{
    SCOPED_TRACE(ustr);

    // Each update must read a NEW measurement, not the same register value repeatedly. Collect a handful
    // of samples and require at least one to differ: sensor noise makes identical pressure across the whole
    // set a strong sign of a stuck / duplicated read (e.g. reading before a fresh measurement completed).
    constexpr uint32_t COUNT{8};
    g_pressure_samples.clear();
    auto r = collect_periodic_measurements(unit.get(), COUNT, collect_timeout(unit.get(), COUNT), record_pressure);
    EXPECT_FALSE(r.timed_out);
    EXPECT_EQ(r.update_count, COUNT);
    EXPECT_GE(g_pressure_samples.size(), 2u);

    bool varied = false;
    for (size_t i = 1; i < g_pressure_samples.size(); ++i) {
        if (g_pressure_samples[i] != g_pressure_samples[0]) {
            varied = true;
            break;
        }
    }
    EXPECT_TRUE(varied) << "all " << g_pressure_samples.size() << " collected pressure samples identical (stuck read?)";
}

TEST_F(TestSPA06, RelativeAltitudeTare)
{
    SCOPED_TRACE(ustr);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    for (int i = 0; i < 20; ++i) {
        unit->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    unit->setReference();
    // Right after taring, the relative altitude of a static unit is ~0 m
    const float rel = unit->relativeAltitude();
    EXPECT_TRUE(std::isfinite(rel));
    EXPECT_NEAR(rel, 0.0f, 2.0f);  // sensor noise floor is well under 2 m
}

TEST_F(TestSPA06, StopStartPeriodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    for (int i = 0; i < 10; ++i) {
        unit->update();
        EXPECT_FALSE(unit->updated());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bool updated{};
    for (int i = 0; i < 50; ++i) {
        unit->update();
        if (unit->updated()) {
            updated = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(updated);
}

TEST_F(TestSPA06, ConfigRoundTrip)
{
    SCOPED_TRACE(ustr);

    auto cfg                     = unit->config();
    cfg.start_periodic           = false;
    cfg.pressure_oversampling    = spa06::Oversampling::X32;
    cfg.temperature_oversampling = spa06::Oversampling::X2;
    cfg.rate                     = spa06::Rate::Rate4;
    cfg.mode                     = spa06::Mode::Pressure;
    unit->config(cfg);

    const auto cfg2 = unit->config();
    EXPECT_FALSE(cfg2.start_periodic);
    EXPECT_EQ(cfg2.pressure_oversampling, cfg.pressure_oversampling);
    EXPECT_EQ(cfg2.temperature_oversampling, cfg.temperature_oversampling);
    EXPECT_EQ(cfg2.rate, cfg.rate);
    EXPECT_EQ(cfg2.mode, cfg.mode);
}

TEST_F(TestSPA06, SoftReset)
{
    SCOPED_TRACE(ustr);

    // Running: rejected
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_FALSE(unit->softReset());

    // Stopped: accepted; startPeriodicMeasurement() reconfigures and resumes
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->softReset());
    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());

    // Measurement actually flows again
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bool updated{};
    for (int i = 0; i < 50; ++i) {
        unit->update();
        if (unit->updated()) {
            updated = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(updated);
}

TEST_F(TestSPA06, StartPeriodicWithArgs)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    // Non-default settings (32x/2x oversampling, 4 samples/s) are written and synced to config()
    EXPECT_TRUE(unit->startPeriodicMeasurement(spa06::Mode::PressureAndTemperature, spa06::Oversampling::X32,
                                               spa06::Oversampling::X2, spa06::Rate::Rate4));
    EXPECT_TRUE(unit->inPeriodic());

    const auto cfg = unit->config();
    EXPECT_EQ(cfg.pressure_oversampling, spa06::Oversampling::X32);
    EXPECT_EQ(cfg.temperature_oversampling, spa06::Oversampling::X2);
    EXPECT_EQ(cfg.rate, spa06::Rate::Rate4);
    EXPECT_EQ(cfg.mode, spa06::Mode::PressureAndTemperature);

    // Measurement actually flows with the new settings (software-I2C-aware timeout)
    auto r = collect_periodic_measurements(unit.get(), STORED_SIZE, collect_timeout(unit.get()));
    EXPECT_FALSE(r.timed_out);
    EXPECT_EQ(r.update_count, STORED_SIZE);

    // The no-argument overload resumes with the settings synced by the call above
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_EQ(unit->config().pressure_oversampling, spa06::Oversampling::X32);
}

TEST_F(TestSPA06, PeriodicAcrossCombos)
{
    SCOPED_TRACE(ustr);

    using namespace m5::unit::spa06;
    using namespace m5::unit::spa06::command;

    // PRS_CFG/TMP_CFG pack the rate in bits[7:4] and the oversampling (PM_PRC/TM_PRC) in bits[3:0].
    // CFG_REG bit2 (PRS_SHIFT_EN) / bit3 (TMP_SHIFT_EN) must be set whenever oversampling exceeds 8x.
    constexpr uint8_t PRS_SHIFT{0x04};  // CFG_REG bit2
    constexpr uint8_t TMP_SHIFT{0x08};  // CFG_REG bit3

    // Coverage of 8 datasheet-VALID P+T combos (each satisfies Rate*(Tp+Tt) < 1s, datasheet 7.3).
    // osrs_p and osrs_t each sweep all eight values (mirrored so osrs_p + osrs_t nibble = 7), so a swapped
    // rate/prc nibble or a missing SHIFT bit is caught, and both columns span the SHIFT threshold.
    // High oversampling forces a low rate (the budget makes high-rate x high-oversampling impossible).
    struct Combo {
        Oversampling osrs_p;
        Oversampling osrs_t;
        Rate rate;
    };
    constexpr Combo combos[] = {
        {Oversampling::X1, Oversampling::X128, Rate::Rate4},  // p=0 t=7 r=2
        {Oversampling::X2, Oversampling::X64, Rate::Rate8},   // p=1 t=6 r=3
        {Oversampling::X4, Oversampling::X32, Rate::Rate16},  // p=2 t=5 r=4
        {Oversampling::X8, Oversampling::X16, Rate::Rate16},  // p=3 t=4 r=4
        {Oversampling::X16, Oversampling::X8, Rate::Rate16},  // p=4 t=3 r=4
        {Oversampling::X32, Oversampling::X4, Rate::Rate16},  // p=5 t=2 r=4
        {Oversampling::X64, Oversampling::X2, Rate::Rate8},   // p=6 t=1 r=3
        {Oversampling::X128, Oversampling::X1, Rate::Rate4},  // p=7 t=0 r=2
    };

    for (const auto& c : combos) {
        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::PressureAndTemperature, c.osrs_p, c.osrs_t, c.rate));

        // --- Register encoding: rate/prc nibbles + SHIFT bits landed on the chip ---
        const uint8_t rate_bits = static_cast<uint8_t>(c.rate);
        const uint8_t exp_prs   = static_cast<uint8_t>((rate_bits << 4) | static_cast<uint8_t>(c.osrs_p));
        const uint8_t exp_tmp   = static_cast<uint8_t>((rate_bits << 4) | static_cast<uint8_t>(c.osrs_t));

        uint8_t v{};
        EXPECT_TRUE(unit->readRegister8(REG_PRS_CFG, v, 0));
        EXPECT_EQ(v, exp_prs);
        EXPECT_TRUE(unit->readRegister8(REG_TMP_CFG, v, 0));
        EXPECT_EQ(v, exp_tmp);

        EXPECT_TRUE(unit->readRegister8(REG_CFG_REG, v, 0));
        const bool p_shift = static_cast<uint8_t>(c.osrs_p) > static_cast<uint8_t>(Oversampling::X8);
        const bool t_shift = static_cast<uint8_t>(c.osrs_t) > static_cast<uint8_t>(Oversampling::X8);
        EXPECT_EQ((v & PRS_SHIFT) != 0, p_shift);
        EXPECT_EQ((v & TMP_SHIFT) != 0, t_shift);

        // --- Measurement count: STORED_SIZE samples flow within the (software-I2C-aware) timeout ---
        // A wrong Rate-derived interval would surface here as timed_out / an update_count shortfall.
        auto r =
            collect_periodic_measurements(unit.get(), STORED_SIZE, collect_timeout(unit.get()), check_spa06_values);
        EXPECT_FALSE(r.timed_out) << "rate nibble " << static_cast<int>(rate_bits);
        EXPECT_EQ(r.update_count, STORED_SIZE) << "rate nibble " << static_cast<int>(rate_bits);
    }
}

TEST_F(TestSPA06, RejectInvalidCombination)
{
    SCOPED_TRACE(ustr);

    using namespace m5::unit::spa06;

    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    // Datasheet budget violation: 64/s x (X4 + X128) ~= 13.8s of samples per second -> rejected, no start
    EXPECT_FALSE(unit->startPeriodicMeasurement(Mode::PressureAndTemperature, Oversampling::X4, Oversampling::X128,
                                                Rate::Rate64));
    EXPECT_FALSE(unit->inPeriodic());

    // The same pressure/rate is valid once temperature is dropped (Mode::Pressure ignores the T budget)
    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::Pressure, Oversampling::X4, Oversampling::X128, Rate::Rate64));
    EXPECT_TRUE(unit->inPeriodic());
}

TEST_F(TestSPA06, PressureOnlyMode)
{
    SCOPED_TRACE(ustr);

    using namespace m5::unit::spa06;

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::Pressure, Oversampling::X16, Oversampling::X1, Rate::Rate16));
    EXPECT_TRUE(unit->inPeriodic());

    auto r = collect_periodic_measurements(unit.get(), STORED_SIZE, collect_timeout(unit.get()));
    EXPECT_FALSE(r.timed_out);
    EXPECT_EQ(r.update_count, STORED_SIZE);

    // Pressure is compensated (seeded traw); temperature() is NaN in pressure-only mode
    const float hpa = unit->pressure();
    EXPECT_TRUE(std::isfinite(hpa));
    EXPECT_GT(hpa, 300.0f);
    EXPECT_LT(hpa, 1100.0f);
    EXPECT_TRUE(std::isnan(unit->temperature()));
}

TEST_F(TestSPA06, TemperatureOnlyMode)
{
    SCOPED_TRACE(ustr);

    using namespace m5::unit::spa06;

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->startPeriodicMeasurement(Mode::Temperature, Oversampling::X16, Oversampling::X1, Rate::Rate16));
    EXPECT_TRUE(unit->inPeriodic());

    auto r = collect_periodic_measurements(unit.get(), STORED_SIZE, collect_timeout(unit.get()));
    EXPECT_FALSE(r.timed_out);
    EXPECT_EQ(r.update_count, STORED_SIZE);

    // Temperature is valid; pressure() is NaN in temperature-only mode
    const float celsius = unit->temperature();
    EXPECT_TRUE(std::isfinite(celsius));
    EXPECT_GT(celsius, -20.0f);
    EXPECT_LT(celsius, 60.0f);
    EXPECT_TRUE(std::isnan(unit->pressure()));
}

TEST_F(TestSPA06, MeasureSingleshot)
{
    SCOPED_TRACE(ustr);

    using namespace m5::unit::spa06;

    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    // The returned Data is self-contained (carries coeffs/kp/kt): read it directly
    Data d{};
    EXPECT_TRUE(unit->measureSingleshot(d, Oversampling::X16, Oversampling::X2));
    EXPECT_TRUE(std::isfinite(d.pressure()));
    EXPECT_GT(d.pressure(), 300.0f);
    EXPECT_LT(d.pressure(), 1100.0f);
    EXPECT_TRUE(std::isfinite(d.temperature()));
    EXPECT_GT(d.temperature(), -20.0f);
    EXPECT_LT(d.temperature(), 60.0f);

    // A single shot does not disturb periodic: it can still be started afterwards
    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());

    // Rejected while periodic is running
    Data d2{};
    EXPECT_FALSE(unit->measureSingleshot(d2));
}

TEST_F(TestSPA06, SettingsAccessors)
{
    SCOPED_TRACE(ustr);

    using namespace m5::unit::spa06;

    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    // Oversampling round-trip (combined + individual)
    EXPECT_TRUE(unit->writeOversampling(Oversampling::X32, Oversampling::X4));
    Oversampling p{}, t{};
    EXPECT_TRUE(unit->readOversampling(p, t));
    EXPECT_EQ(p, Oversampling::X32);
    EXPECT_EQ(t, Oversampling::X4);

    EXPECT_TRUE(unit->writeOversamplingPressure(Oversampling::X8));
    EXPECT_TRUE(unit->writeOversamplingTemperature(Oversampling::X16));
    EXPECT_TRUE(unit->readOversampling(p, t));
    EXPECT_EQ(p, Oversampling::X8);
    EXPECT_EQ(t, Oversampling::X16);

    // Rate round-trip; writing the rate preserves the oversampling nibbles
    EXPECT_TRUE(unit->writeRate(Rate::Rate8));
    Rate r{};
    EXPECT_TRUE(unit->readRate(r));
    EXPECT_EQ(r, Rate::Rate8);
    EXPECT_TRUE(unit->readOversampling(p, t));
    EXPECT_EQ(p, Oversampling::X8);
    EXPECT_EQ(t, Oversampling::X16);

    // Writes are rejected while periodic runs; reads still work
    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_FALSE(unit->writeOversampling(Oversampling::X1, Oversampling::X1));
    EXPECT_FALSE(unit->writeOversamplingPressure(Oversampling::X1));
    EXPECT_FALSE(unit->writeRate(Rate::Rate1));
    EXPECT_TRUE(unit->readOversampling(p, t));
    EXPECT_TRUE(unit->readRate(r));
}
