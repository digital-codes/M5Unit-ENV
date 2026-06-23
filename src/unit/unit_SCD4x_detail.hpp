/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_SCD4x_detail.hpp
  @brief Internal helpers shared by SCD40/SCD41 (and future SCD43)
  @details Not part of the public API. Included by the SCD4x unit implementations.
*/
#ifndef M5_UNIT_ENV_UNIT_SCD4X_DETAIL_HPP
#define M5_UNIT_ENV_UNIT_SCD4X_DETAIL_HPP

#include <cstdint>

namespace m5 {
namespace unit {
namespace scd4x {
/*!
  @namespace detail
  @brief Internal helpers shared across the SCD4x family (SCD40/SCD41/SCD43)
 */
namespace detail {

//! @brief Mask for the variant nibble (word[0] bits[15..12]) of get_sensor_variant (0x202F)
constexpr uint8_t VARIANT_NIBBLE_MASK{0xF0};
constexpr uint8_t VARIANT_NIBBLE_SCD40{0x00};  //!< @brief SCD40 variant nibble (bits[15..12] == 0b0000)
constexpr uint8_t VARIANT_NIBBLE_SCD41{0x10};  //!< @brief SCD41 variant nibble (bits[15..12] == 0b0001)
constexpr uint8_t VARIANT_NIBBLE_SCD43{0x50};  //!< @brief SCD43 variant nibble (bits[15..12] == 0b0101)

/*!
  @brief Does the get_sensor_variant high byte indicate the given variant?
  @param variant_high_byte High byte (word[0] bits[15..8]) read from get_sensor_variant (0x202F)
  @param nibble Expected variant nibble (e.g. VARIANT_NIBBLE_SCD41)
  @details Only bits[15..12] identify the variant; bits[11..0] may differ per unit
  (datasheet CD_DS_SCD4x §3.10.6, issue #38), so compare the high nibble only.
 */
constexpr bool is_sensor_variant(const uint8_t variant_high_byte, const uint8_t nibble)
{
    return (variant_high_byte & VARIANT_NIBBLE_MASK) == nibble;
}

}  // namespace detail
}  // namespace scd4x
}  // namespace unit
}  // namespace m5
#endif
