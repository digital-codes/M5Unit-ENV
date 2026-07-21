/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_SPA06_data.hpp
  @brief SPA06-003 measurement data + calibration/compensation (framework independent)
  @details PSR/TMP are 24-bit 2's complement; 11 calibration coefficients
  (c0,c1 12-bit; c00,c10 20-bit; c01,c11,c20,c21,c30 16-bit; c31,c40 12-bit) drive a high-order
  compensation polynomial. Pressure is returned in Pa; the UnitSPA06 driver converts to hPa.
*/
#ifndef M5_UNIT_ENV_UNIT_SPA06_DATA_HPP
#define M5_UNIT_ENV_UNIT_SPA06_DATA_HPP

#include <array>
#include <cstdint>
#include <cmath>
#include <limits>

namespace m5 {
namespace unit {
namespace spa06 {

/*!
  @enum Oversampling
  @brief Oversampling factor; the enum value is the PM_PRC/TM_PRC nibble (datasheet Table 4)
 */
enum class Oversampling : uint8_t {
    X1,    //!< x1 (single, lowest precision)
    X2,    //!< x2
    X4,    //!< x4
    X8,    //!< x8
    X16,   //!< x16 (standard)
    X32,   //!< x32
    X64,   //!< x64
    X128,  //!< x128 (highest precision)
};

/*!
  @enum Rate
  @brief Measurement rate; the enum value is the PM_RATE/TMP_RATE nibble (measurements per second)
 */
enum class Rate : uint8_t {
    Rate1,    //!< 1 measurement/s
    Rate2,    //!< 2 measurements/s
    Rate4,    //!< 4 measurements/s
    Rate8,    //!< 8 measurements/s
    Rate16,   //!< 16 measurements/s
    Rate32,   //!< 32 measurements/s
    Rate64,   //!< 64 measurements/s
    Rate128,  //!< 128 measurements/s
};

/*!
  @enum Mode
  @brief Continuous measurement type; the enum value is the MEAS_CFG MEAS_CTRL[2:0] background-mode code
 */
enum class Mode : uint8_t {
    Pressure               = 0x05,  //!< Continuous pressure only (temperature() returns NaN)
    Temperature            = 0x06,  //!< Continuous temperature only (pressure() returns NaN)
    PressureAndTemperature = 0x07,  //!< Continuous pressure and temperature (both valid)
};

//! @brief Compensation scale factor kP/kT for an oversampling rate (datasheet Table 4)
inline float scale_factor(const Oversampling oversampling)
{
    switch (oversampling) {
        case Oversampling::X1:
            return 524288.0f;
        case Oversampling::X2:
            return 1572864.0f;
        case Oversampling::X4:
            return 3670016.0f;
        case Oversampling::X8:
            return 7864320.0f;
        case Oversampling::X16:
            return 253952.0f;
        case Oversampling::X32:
            return 516096.0f;
        case Oversampling::X64:
            return 1040384.0f;
        case Oversampling::X128:
            return 2088960.0f;
    }
    return std::numeric_limits<float>::quiet_NaN();  // Out-of-range enum value
}

//! @brief Measurement time (ms x10) for an oversampling rate (datasheet Table 8), indexed by nibble
inline uint32_t measurement_time_x10(const Oversampling oversampling)
{
    constexpr uint32_t t[] = {36, 52, 84, 148, 276, 532, 1044, 2068};
    return t[static_cast<uint8_t>(oversampling)];
}

/*!
  @brief True if a rate/oversampling/mode combination fits the datasheet timing budget
  @details Background mode shares a 1-second timeline between pressure and temperature (single ADC).
  The active measurement type(s) must fit: Rate x (sum of active measurement times) < 1 second
  (datasheet 7.3 Note under Table 9). Pressure and temperature run at the same Rate in this driver.
 */
inline bool valid_combination(const Oversampling osrs_p, const Oversampling osrs_t, const Rate rate, const Mode mode)
{
    const uint32_t r = static_cast<uint32_t>(1) << static_cast<uint8_t>(rate);  // measurements per second
    uint32_t budget_x10{};
    switch (mode) {
        case Mode::Pressure:
            budget_x10 = measurement_time_x10(osrs_p);
            break;
        case Mode::Temperature:
            budget_x10 = measurement_time_x10(osrs_t);
            break;
        case Mode::PressureAndTemperature:
            budget_x10 = measurement_time_x10(osrs_p) + measurement_time_x10(osrs_t);
            break;
    }
    return r * budget_x10 < 10000u;  // < 1000 ms (values are ms x10)
}

//! @brief Sentinel value of psr_raw()/tmp_raw() for a register not measured yet (reset value 0x800000)
constexpr int32_t NOT_MEASURED{-8388608};

//! @brief Sign-extend the low `bits` of `v` (2's complement)
inline int32_t sign_extend(const uint32_t v, const uint8_t bits)
{
    const uint32_t m = static_cast<uint32_t>(1) << (bits - 1);
    return static_cast<int32_t>((v ^ m) - m);
}

//! @brief Factory calibration coefficients (all 2's complement, already sign-extended)
struct coeffs_t {
    int32_t c0{}, c1{};
    int32_t c00{}, c10{};
    int32_t c01{}, c11{}, c20{}, c21{}, c30{};
    int32_t c31{}, c40{};
};

//! @brief Parse the 21 COEF register bytes (0x10..0x24) into coeffs_t (datasheet Table 10)
inline coeffs_t parse_coeffs(const std::array<uint8_t, 21>& r)
{
    coeffs_t c{};
    c.c0  = sign_extend((static_cast<uint32_t>(r[0]) << 4) | (r[1] >> 4), 12);
    c.c1  = sign_extend(((static_cast<uint32_t>(r[1]) & 0x0F) << 8) | r[2], 12);
    c.c00 = sign_extend((static_cast<uint32_t>(r[3]) << 12) | (static_cast<uint32_t>(r[4]) << 4) | (r[5] >> 4), 20);
    c.c10 = sign_extend(((static_cast<uint32_t>(r[5]) & 0x0F) << 16) | (static_cast<uint32_t>(r[6]) << 8) | r[7], 20);
    c.c01 = sign_extend((static_cast<uint32_t>(r[8]) << 8) | r[9], 16);
    c.c11 = sign_extend((static_cast<uint32_t>(r[10]) << 8) | r[11], 16);
    c.c20 = sign_extend((static_cast<uint32_t>(r[12]) << 8) | r[13], 16);
    c.c21 = sign_extend((static_cast<uint32_t>(r[14]) << 8) | r[15], 16);
    c.c30 = sign_extend((static_cast<uint32_t>(r[16]) << 8) | r[17], 16);
    c.c31 = sign_extend((static_cast<uint32_t>(r[18]) << 4) | (r[19] >> 4), 12);
    c.c40 = sign_extend(((static_cast<uint32_t>(r[19]) & 0x0F) << 8) | r[20], 12);
    return c;
}

//! @brief Compensated temperature (degC) from a raw TMP reading and scale kT
inline float compensate_temperature(const int32_t tmp_raw, const coeffs_t& c, const float kt)
{
    const float traw_sc = tmp_raw / kt;
    return c.c0 * 0.5f + c.c1 * traw_sc;
}

//! @brief Compensated pressure (Pa) from raw PSR/TMP readings and scales kP/kT (datasheet 4.6.1)
inline float compensate_pressure(const int32_t psr_raw, const int32_t tmp_raw, const coeffs_t& c, const float kp,
                                 const float kt)
{
    const float praw_sc = psr_raw / kp;
    const float traw_sc = tmp_raw / kt;
    return c.c00 + praw_sc * (c.c10 + praw_sc * (c.c20 + praw_sc * (c.c30 + praw_sc * c.c40))) + traw_sc * c.c01 +
           traw_sc * praw_sc * (c.c11 + praw_sc * (c.c21 + praw_sc * c.c31));
}

//! @brief Relative altitude (m) from a pressure and a reference (both hPa), barometric formula
inline float altitude_m(const float pressure_hpa, const float sea_level_hpa)
{
    return 44330.0f * (1.0f - std::pow(pressure_hpa / sea_level_hpa, 1.0f / 5.255f));
}

/*!
  @struct Data
  @brief One SPA06 sample: a 6-byte burst from PSR_B2 (0x00) = PSR(24) + TMP(24), big-endian
 */
struct Data {
    std::array<uint8_t, 6> raw{};  //!< @brief PSR B2/B1/B0 + TMP B2/B1/B0

    //! @brief Raw pressure (24-bit 2's complement)
    inline int32_t psr_raw() const
    {
        return sign_extend((static_cast<uint32_t>(raw[0]) << 16) | (static_cast<uint32_t>(raw[1]) << 8) | raw[2], 24);
    }
    //! @brief Raw temperature (24-bit 2's complement)
    inline int32_t tmp_raw() const
    {
        return sign_extend((static_cast<uint32_t>(raw[3]) << 16) | (static_cast<uint32_t>(raw[4]) << 8) | raw[5], 24);
    }
};

}  // namespace spa06
}  // namespace unit
}  // namespace m5
#endif
