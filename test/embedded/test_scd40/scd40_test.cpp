/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitSCD40
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_SCD40.hpp>
#include <unit/unit_SCD4x_detail.hpp>
#include <chrono>
#include <iostream>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::scd4x;
using m5::unit::types::elapsed_time_t;

constexpr uint32_t STORED_SIZE{4};

class TestSCD4x : public I2CComponentTestBase<UnitSCD40> {
protected:
    virtual UnitSCD40* get_instance() override
    {
        auto ptr         = new m5::unit::UnitSCD40();
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

// Regression for issue #38 (companion to SCD41): get_sensor_variant (0x202F) must identify the variant by
// word[0] bits[15..12] only; bits[11..0] may differ per unit (datasheet CD_DS_SCD4x §3.10.6).
TEST_F(TestSCD4x, SensorVariant)
{
    using namespace m5::unit::scd4x::detail;

    // is_valid_chip() identifies the variant from var[0] (= get_sensor_variant word[0] bits[15..8]).
    // Every SCD40 word[0] (canonical 0x0440 and any low-bit variation) must be accepted, low bits ignored.
    for (const uint16_t word0 : {(uint16_t)0x0440, (uint16_t)0x0441, (uint16_t)0x04FF}) {
        const uint8_t high_byte = static_cast<uint8_t>(word0 >> 8);  // == var[0]
        EXPECT_TRUE(is_sensor_variant(high_byte, VARIANT_NIBBLE_SCD40)) << "word0=" << std::hex << word0;
    }
    // Other variants' word[0] must be rejected as SCD40
    for (const uint16_t word0 : {(uint16_t)0x1440, (uint16_t)0x5441}) {  // SCD41 / SCD43
        EXPECT_FALSE(is_sensor_variant(static_cast<uint8_t>(word0 >> 8), VARIANT_NIBBLE_SCD40))
            << "word0=" << std::hex << word0;
    }

    // The connected sensor was accepted by is_valid_chip() during fixture begin() (otherwise setup would have
    // failed and this test would not run); the fixture starts with periodic measurement disabled.
    EXPECT_FALSE(unit->inPeriodic());
}
