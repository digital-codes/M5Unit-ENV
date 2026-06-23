/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitSCD41
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_SCD41.hpp>
#include <unit/unit_SCD4x_detail.hpp>
#include <chrono>
#include <iostream>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::scd4x;
using m5::unit::types::elapsed_time_t;

constexpr uint32_t STORED_SIZE{4};

class TestSCD4x : public I2CComponentTestBase<UnitSCD41> {
protected:
    virtual UnitSCD41* get_instance() override
    {
        auto ptr         = new m5::unit::UnitSCD41();
        auto ccfg        = ptr->component_config();
        ccfg.stored_size = STORED_SIZE;
        ptr->component_config(ccfg);
        auto cfg           = ptr->config();
        cfg.start_periodic = false;
        ptr->config(cfg);
        return ptr;
    }
};

namespace {
}  // namespace

#include "../scd4x_test.inl"

// Regression for issue #38: get_sensor_variant (0x202F) must identify the variant by word[0] bits[15..12]
// only; bits[11..0] may differ per unit (datasheet CD_DS_SCD4x §3.10.6). A unit reporting 0x1441 (instead of
// the canonical 0x1440) shares the same high byte 0x14 and must still be accepted as SCD41.
TEST_F(TestSCD4x, SensorVariant)
{
    using namespace m5::unit::scd4x::detail;

    // is_valid_chip() identifies the variant from var[0] (= get_sensor_variant word[0] bits[15..8]).
    // Mirror that extraction here over full word[0] values, including the issue #38 value 0x1441:
    // every SCD41 word[0] (canonical 0x1440 and the reported 0x1441) must be accepted, low bits ignored.
    for (const uint16_t word0 : {(uint16_t)0x1440, (uint16_t)0x1441, (uint16_t)0x14FF}) {
        const uint8_t high_byte = static_cast<uint8_t>(word0 >> 8);  // == var[0]
        EXPECT_TRUE(is_sensor_variant(high_byte, VARIANT_NIBBLE_SCD41)) << "word0=" << std::hex << word0;
    }
    // Other variants' word[0] must be rejected as SCD41
    for (const uint16_t word0 : {(uint16_t)0x0440, (uint16_t)0x5441}) {  // SCD40 / SCD43
        EXPECT_FALSE(is_sensor_variant(static_cast<uint8_t>(word0 >> 8), VARIANT_NIBBLE_SCD41))
            << "word0=" << std::hex << word0;
    }

    // The connected sensor was accepted by is_valid_chip() during fixture begin() (otherwise setup would have
    // failed and this test would not run); the fixture starts with periodic measurement disabled.
    EXPECT_FALSE(unit->inPeriodic());
}

TEST_F(TestSCD4x, Singleshot)
{
    SCOPED_TRACE(ustr);
    {
        Data d{};
        EXPECT_FALSE(unit->inPeriodic());
        EXPECT_TRUE(unit->measureSingleshot(d));
        EXPECT_NE(d.co2(), 0);
        EXPECT_TRUE(std::isfinite(d.temperature()));
        EXPECT_TRUE(std::isfinite(d.humidity()));

        EXPECT_TRUE(unit->startPeriodicMeasurement());

        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_FALSE(unit->measureSingleshot(d));
        EXPECT_EQ(d.co2(), 0);
        EXPECT_FLOAT_EQ(d.temperature(), -45.f);
        EXPECT_FLOAT_EQ(d.humidity(), 0.0f);

        EXPECT_TRUE(unit->stopPeriodicMeasurement());
    }
    {
        Data d{};
        EXPECT_FALSE(unit->inPeriodic());
        EXPECT_TRUE(unit->measureSingleshotRHT(d));
        EXPECT_EQ(d.co2(), 0);
        EXPECT_TRUE(std::isfinite(d.temperature()));
        EXPECT_TRUE(std::isfinite(d.humidity()));

        EXPECT_TRUE(unit->startPeriodicMeasurement());

        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_FALSE(unit->measureSingleshotRHT(d));
        EXPECT_EQ(d.co2(), 0);
        EXPECT_FLOAT_EQ(d.temperature(), -45.f);
        EXPECT_FLOAT_EQ(d.humidity(), 0.0f);

        EXPECT_TRUE(unit->stopPeriodicMeasurement());
    }
}

TEST_F(TestSCD4x, PowerMode)
{
    SCOPED_TRACE(ustr);

    EXPECT_FALSE(unit->inPeriodic());

    uint32_t count{8};
    while (count--) {
        EXPECT_TRUE(unit->powerDown()) << count;
        EXPECT_TRUE(unit->wakeup()) << count;
    }

    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());

    EXPECT_FALSE(unit->powerDown());
    EXPECT_FALSE(unit->wakeup());

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->reInit());
}

TEST_F(TestSCD4x, ASC)
{
    SCOPED_TRACE(ustr);

    constexpr uint16_t hours_table[] = {0, 32768, 65535};
    for (auto&& h : hours_table) {
        EXPECT_TRUE(unit->writeAutomaticSelfCalibrationInitialPeriod(h));
        EXPECT_TRUE(unit->writeAutomaticSelfCalibrationStandardPeriod(h));

        uint16_t ih{}, sh{};

        EXPECT_TRUE(unit->readAutomaticSelfCalibrationInitialPeriod(ih));
        EXPECT_TRUE(unit->readAutomaticSelfCalibrationStandardPeriod(sh));

        EXPECT_EQ(ih, (h >> 2) << 2);
        EXPECT_EQ(sh, (h >> 2) << 2);
    }

    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());
    for (auto&& h : hours_table) {
        EXPECT_FALSE(unit->writeAutomaticSelfCalibrationInitialPeriod(h));
        EXPECT_FALSE(unit->writeAutomaticSelfCalibrationStandardPeriod(h));
    }
}
