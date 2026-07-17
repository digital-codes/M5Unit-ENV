/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for BMP280 data (framework independent); included by both the native
  (test_bmp280_data) and embedded (test_bmp280) suites.
*/
#include <gtest/gtest.h>
#include <unit/unit_BMP280_data.hpp>
#include <cmath>
#include <cstring>

using namespace m5::unit::bmp280;

namespace {

// Build Data from 24-bit raw register values (pressure, temperature)
Data make_data(const uint32_t raw24_p, const uint32_t raw24_t, const Trimming* trim)
{
    Data d{};
    d.raw      = {(uint8_t)(raw24_p >> 16), (uint8_t)(raw24_p >> 8), (uint8_t)raw24_p,
             (uint8_t)(raw24_t >> 16), (uint8_t)(raw24_t >> 8), (uint8_t)raw24_t};
    d.trimming = trim;
    return d;
}

float from_bits(const uint32_t u)
{
    float f{};
    memcpy(&f, &u, sizeof(f));
    return f;
}

// Trimming captured from a real unit (Core2 + UnitBMP280, 2026-07-17, pre-split implementation)
Trimming golden_trimming()
{
    constexpr uint8_t v[24] = {0xBB, 0x6E, 0x02, 0x67, 0x32, 0x00, 0x14, 0x8E, 0xD5, 0xD6, 0xD0, 0x0B,
                               0x5B, 0x1B, 0xCC, 0xFF, 0xF9, 0xFF, 0x8C, 0x3C, 0xF8, 0xC6, 0x70, 0x17};
    Trimming t{};
    memcpy(t.value, v, sizeof(v));
    return t;
}

// Coefficients from the Bosch BMP280 datasheet (DS001) compensation example
Trimming datasheet_trimming()
{
    Trimming t{};
    t.dig_T1 = 27504;
    t.dig_T2 = 26435;
    t.dig_T3 = -1000;
    t.dig_P1 = 36477;
    t.dig_P2 = -10685;
    t.dig_P3 = 3024;
    t.dig_P4 = 2855;
    t.dig_P5 = 140;
    t.dig_P6 = -7;
    t.dig_P7 = 15500;
    t.dig_P8 = -14600;
    t.dig_P9 = 6000;
    return t;
}

// Frozen verbatim copy of the pre-split compensation (unit_BMP280.cpp anonymous-namespace
// Calculator before the unit_BMP280_data.hpp split). DO NOT MODIFY: it is the parity reference
// proving the split preserved behavior bit-exactly.
struct ReferenceCalculator {
    inline float temperature(const int32_t adc_P, const int32_t adc_T, const Trimming* t)
    {
        return t ? compensate_temperature_f(adc_T, *t) : std::numeric_limits<float>::quiet_NaN();
    }
    inline float pressure(const int32_t adc_P, const int32_t adc_T, const Trimming* t)
    {
        if (!t) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        if (t_fine == 0) {
            (void)compensate_temperature_f(adc_T, *t);  // For t_fine
        }
        return compensate_pressure_f(adc_P, *t);
    }

private:
    float compensate_temperature_f(const int32_t adc_T, const Trimming& trim)
    {
        float var1{}, var2{}, T{};
        var1 = (((float)adc_T) / 16384.0f - ((float)trim.dig_T1) / 1024.0f) * ((float)trim.dig_T2);
        var2 = ((((float)adc_T) / 131072.0f - ((float)trim.dig_T1) / 8192.0f) *
                (((float)adc_T) / 131072.0f - ((float)trim.dig_T1) / 8192.0f)) *
               ((float)trim.dig_T3);
        t_fine = (int32_t)(var1 + var2);
        T      = (var1 + var2) / 5120.0f;
        return T;
    }

    float compensate_pressure_f(const int32_t adc_P, const Trimming& trim)
    {
        float var1{}, var2{}, P{};
        var1 = ((float)t_fine / 2.0f) - 64000.0f;
        var2 = var1 * var1 * ((float)trim.dig_P6) / 32768.0f;
        var2 = var2 + var1 * ((float)trim.dig_P5) * 2.0f;
        var2 = (var2 / 4.0f) + (((float)trim.dig_P4) * 65536.0f);
        var1 = (((float)trim.dig_P3) * var1 * var1 / 524288.0f + ((float)trim.dig_P2) * var1) / 524288.0f;
        var1 = (1.0f + var1 / 32768.0f) * ((float)trim.dig_P1);
        if (var1 == 0.0f) {
            return 0;
        }
        P    = 1048576.0f - (float)adc_P;
        P    = (P - (var2 / 4096.0f)) * 6250.0f / var1;
        var1 = ((float)trim.dig_P9) * P * P / 2147483648.0f;
        var2 = P * ((float)trim.dig_P8) / 32768.0f;
        P    = P + (var1 + var2 + ((float)trim.dig_P7)) / 16.0f;
        return P;
    }

    int32_t t_fine{};
};

// Reference results replicating the pre-split Data::celsius()/pressure() including the
// NOT_MEASURED guards (raw 24-bit value 0x800000 when osrs is Skipped)
float reference_celsius(const uint32_t raw24_p, const uint32_t raw24_t, const Trimming* trim)
{
    ReferenceCalculator c{};
    return (raw24_t != 0x800000) ? c.temperature((int32_t)raw24_p >> 4, (int32_t)raw24_t >> 4, trim)
                                 : std::numeric_limits<float>::quiet_NaN();
}
float reference_pressure(const uint32_t raw24_p, const uint32_t raw24_t, const Trimming* trim)
{
    ReferenceCalculator c{};
    return (raw24_t != 0x800000 && raw24_p != 0x800000) ? c.pressure((int32_t)raw24_p >> 4, (int32_t)raw24_t >> 4, trim)
                                                        : std::numeric_limits<float>::quiet_NaN();
}

bool bit_equal(const float a, const float b)
{
    return memcmp(&a, &b, sizeof(float)) == 0;
}

}  // namespace

TEST(BMP280Data, TrimmingLayout)
{
    // The Trimming union parses the 24 calibration register bytes as packed little-endian words
    const auto t = golden_trimming();
    EXPECT_EQ(t.dig_T1, 28347);
    EXPECT_EQ(t.dig_T2, 26370);
    EXPECT_EQ(t.dig_T3, 50);
    EXPECT_EQ(t.dig_P1, 36372);
    EXPECT_EQ(t.dig_P2, -10539);
    EXPECT_EQ(t.dig_P3, 3024);
    EXPECT_EQ(t.dig_P4, 7003);
    EXPECT_EQ(t.dig_P5, -52);
    EXPECT_EQ(t.dig_P6, -7);
    EXPECT_EQ(t.dig_P7, 15500);
    EXPECT_EQ(t.dig_P8, -14600);
    EXPECT_EQ(t.dig_P9, 6000);
}

TEST(BMP280Data, DatasheetExample)
{
    // Bosch DS001 compensation example: adc_T=519888, adc_P=415148 (20-bit values)
    // -> 25.08 degC / 100653.27 Pa (double-precision reference; float version within tolerance)
    const auto t = datasheet_trimming();
    const auto d = make_data(415148U << 4, 519888U << 4, &t);
    EXPECT_NEAR(d.celsius(), 25.08f, 0.01f);
    EXPECT_NEAR(d.pressure(), 100653.27f, 5.0f);
}

TEST(BMP280Data, GoldenVectors)
{
    // Captured from a real unit with the PRE-SPLIT implementation (Core2 + UnitBMP280, 2026-07-17);
    // expected values are that implementation's actual outputs (float bit patterns)
    const auto t = golden_trimming();

    struct Vector {
        std::array<uint8_t, 6> raw;
        uint32_t celsius_bits;
        uint32_t pressure_bits;
    };
    constexpr Vector vec[] = {
        {{0x59, 0xC9, 0xF0, 0x88, 0x35, 0x80}, 0x42033FC3, 0x47C11F39},  // 32.8122673 degC / 98878.4453 Pa
        {{0x59, 0xC9, 0x00, 0x88, 0x32, 0x00}, 0x42032DBA, 0x47C11F25},  // 32.7946548 degC / 98878.2891 Pa
        {{0x59, 0xC8, 0x10, 0x88, 0x2E, 0x00}, 0x4203191E, 0x47C11EDE},  // 32.7745285 degC / 98877.7344 Pa
    };
    for (const auto& v : vec) {
        Data d{};
        d.raw      = v.raw;
        d.trimming = &t;
        EXPECT_FLOAT_EQ(d.celsius(), from_bits(v.celsius_bits));
        EXPECT_FLOAT_EQ(d.pressure(), from_bits(v.pressure_bits));
    }

    // Captured NOT_MEASURED sample (osrs Skipped): raw = 0x800000 / 0x800000 -> NaN
    const auto nm = make_data(0x800000, 0x800000, &t);
    EXPECT_TRUE(std::isnan(nm.celsius()));
    EXPECT_TRUE(std::isnan(nm.pressure()));
}

TEST(BMP280Data, NotMeasuredAndNullTrimming)
{
    const auto t = golden_trimming();

    // Temperature not measured -> both NaN (pressure compensation needs t_fine)
    const auto d0 = make_data(0x59C9F0, 0x800000, &t);
    EXPECT_TRUE(std::isnan(d0.celsius()));
    EXPECT_TRUE(std::isnan(d0.pressure()));

    // Pressure not measured -> temperature still valid, pressure NaN
    const auto d1 = make_data(0x800000, 0x883580, &t);
    EXPECT_FALSE(std::isnan(d1.celsius()));
    EXPECT_TRUE(std::isnan(d1.pressure()));

    // No trimming -> NaN
    const auto d2 = make_data(0x59C9F0, 0x883580, nullptr);
    EXPECT_TRUE(std::isnan(d2.celsius()));
    EXPECT_TRUE(std::isnan(d2.pressure()));
}

TEST(BMP280Data, ParityWithFrozenReference)
{
    // The split must preserve behavior bit-exactly: sweep the ADC space against the frozen
    // pre-split reference implementation (same TU, same compile flags -> deterministic floats)
    const Trimming trims[]     = {golden_trimming(), datasheet_trimming()};
    constexpr uint32_t adc20[] = {0x00000, 0x00001, 0x00FFF, 0x12345, 0x66666, 0x7EED0, 0x80000, 0xABCDE, 0xFFFFF};

    for (const auto& t : trims) {
        for (const auto ap : adc20) {
            for (const auto at : adc20) {
                const uint32_t rp = ap << 4;
                const uint32_t rt = at << 4;
                const auto d      = make_data(rp, rt, &t);
                const float rc    = reference_celsius(rp, rt, &t);
                const float rf    = reference_pressure(rp, rt, &t);
                EXPECT_TRUE(bit_equal(d.celsius(), rc)) << "celsius adc_P=" << ap << " adc_T=" << at;
                EXPECT_TRUE(bit_equal(d.pressure(), rf)) << "pressure adc_P=" << ap << " adc_T=" << at;
                EXPECT_TRUE(bit_equal(d.fahrenheit(), rc * 9.0f / 5.0f + 32.f)) << "fahrenheit adc_T=" << at;
            }
        }
    }
}
