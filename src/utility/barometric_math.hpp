/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file barometric_math.hpp
  @brief Barometric math shared by pressure sensors (framework independent)
*/
#ifndef M5_UNIT_ENV_UTILITY_BAROMETRIC_MATH_HPP
#define M5_UNIT_ENV_UTILITY_BAROMETRIC_MATH_HPP

#include <cmath>

namespace m5 {
namespace unit {
namespace barometric {

/*!
  @brief Altitude (m) from a pressure and a reference sea-level pressure (both Pa)
  @details Barometric formula (temperature-independent): h = 44330 * (1 - (p/p0)^(1/5.255)).
  Sea-level pressure defaults to the ICAO standard atmosphere (101325 Pa / 1013.25 hPa).
 */
inline float calculate_altitude(const float pressure_pa, const float sea_level_pa = 101325.0f)
{
    return 44330.0f * (1.0f - std::pow(pressure_pa / sea_level_pa, 1.0f / 5.255f));
}

}  // namespace barometric
}  // namespace unit
}  // namespace m5

#endif
