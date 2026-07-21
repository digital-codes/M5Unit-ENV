/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_SPA06.hpp
  @brief SPA06 Unit for M5UnitUnified
*/
#ifndef M5_UNIT_ENV_UNIT_SPA06_HPP
#define M5_UNIT_ENV_UNIT_SPA06_HPP

#include <M5UnitComponent.hpp>
#include <m5_utility/container/circular_buffer.hpp>
#include <memory>
#include "unit_SPA06_data.hpp"

namespace m5 {
namespace unit {

/*!
  @class UnitSPA06
  @brief Goertek SPA06-003 barometer (pressure + temperature);
  read pressure()/temperature()/altitude() directly. On Unit DoF10 (U230) it sits at 0x76.
 */
class UnitSPA06 : public Component, public PeriodicMeasurementAdapter<UnitSPA06, spa06::Data> {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitSPA06, 0x76);

public:
    /*!
      @struct config_t
      @brief Settings for begin()
     */
    struct config_t {
        //! Start periodic measurement on begin()?
        bool start_periodic{true};
        //! Pressure oversampling; X16 = Standard
        spa06::Oversampling pressure_oversampling{spa06::Oversampling::X16};
        //! Temperature oversampling
        spa06::Oversampling temperature_oversampling{spa06::Oversampling::X1};
        //! Pressure/temperature measurement rate
        spa06::Rate rate{spa06::Rate::Rate16};
        //! Continuous measurement type (pressure / temperature / both)
        spa06::Mode mode{spa06::Mode::PressureAndTemperature};
    };

    explicit UnitSPA06(const uint8_t addr = DEFAULT_ADDRESS);
    virtual ~UnitSPA06()
    {
    }

    //! @brief Begin the unit
    virtual bool begin() override;
    //! @brief Update the unit
    //! @param[in] force Forces an update if true, even if not time for periodic measurement
    virtual void update(const bool force = false) override;

    ///@name Settings
    ///@{
    //! @brief Gets the configuration
    config_t config() const
    {
        return _cfg;
    }
    //! @brief Set the configuration
    void config(const config_t& cfg)
    {
        _cfg = cfg;
    }
    ///@}

    ///@name Reset
    ///@{
    //! @brief Soft reset (REG_RESET SOFT_RST), leaving the chip initialized but unconfigured and stopped
    //! @return True if successful
    //! @warning During periodic measurements, an error is returned; call stopPeriodicMeasurement() first
    //! @warning All settings revert to the chip defaults; call startPeriodicMeasurement(...) to reconfigure and
    //! resume measurement
    bool softReset();
    ///@}

    ///@name Periodic measurement
    ///@{
    /*!
      @brief Start periodic measurement
      @param mode Continuous measurement type (pressure / temperature / both)
      @param pressure_oversampling Pressure oversampling
      @param temperature_oversampling Temperature oversampling
      @param rate Pressure/temperature measurement rate
      @return True if successful
      @warning Returns false if the combination exceeds the datasheet timing budget (see valid_combination)
      @note In Mode::Pressure, temperature() returns NaN; in Mode::Temperature, pressure() returns NaN
      @note Blocks until the first measurement with the new settings is ready (up to roughly one measurement
      period plus a measurement time); Mode::Pressure additionally runs a one-shot temperature seed first
    */
    bool startPeriodicMeasurement(const spa06::Mode mode, const spa06::Oversampling pressure_oversampling,
                                  const spa06::Oversampling temperature_oversampling, const spa06::Rate rate);
    //! @brief Start periodic measurement in the current settings
    bool startPeriodicMeasurement();
    //! @brief Stop periodic measurement
    bool stopPeriodicMeasurement();
    ///@}

    ///@name Measurement
    ///@{
    //! @brief Latest compensated pressure (hPa)
    float pressure() const;
    //! @brief Latest compensated temperature (degC)
    float temperature() const;
    //! @brief Absolute altitude (m) versus the given sea-level pressure (QNH, hPa)
    float altitude(const float sea_level_hpa = 1013.25f) const;
    //! @brief Altitude (m) relative to the last setReference() tare point (0 m at that pressure)
    float relativeAltitude() const;
    //! @brief Tare the current pressure as the 0 m reference for relativeAltitude()
    void setReference();
    ///@}

protected:
    bool start_periodic_measurement(const spa06::Mode mode, const spa06::Oversampling pressure_oversampling,
                                    const spa06::Oversampling temperature_oversampling, const spa06::Rate rate);
    bool start_periodic_measurement();
    bool seed_temperature();
    bool stop_periodic_measurement();
    bool read_measurement(spa06::Data& d);
    bool read_coefficients();

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitSPA06, spa06::Data);

private:
    config_t _cfg{};
    spa06::coeffs_t _coeffs{};
    float _kp{};  // Set by start_periodic_measurement() before any compensation
    float _kt{};
    int32_t _seed_tmp_raw{};  // Temperature seeded once for pressure compensation in Mode::Pressure
    float _reference_hpa{1013.25f};
    uint32_t _pushed{}, _consumed{};
    std::unique_ptr<m5::container::CircularBuffer<spa06::Data>> _data{};
};

namespace spa06 {
//! @brief Default ring buffer depth
constexpr uint32_t DEFAULT_STORED_SIZE{4};

namespace command {
///@cond
constexpr uint8_t REG_PSR_B2{0x00};    // burst start: PSR(3) + TMP(3) = 6 bytes
constexpr uint8_t REG_TMP_B2{0x03};    // TMP(3) start (0x03..0x05)
constexpr uint8_t REG_PRS_CFG{0x06};   // PM_RATE[7:4] PM_PRC[3:0]
constexpr uint8_t REG_TMP_CFG{0x07};   // TMP_RATE[7:4] TM_PRC[3:0]
constexpr uint8_t REG_MEAS_CFG{0x08};  // bit7 COEF_RDY, bit6 SENSOR_RDY, bit5 TMP_RDY, bit4 PRS_RDY
constexpr uint8_t REG_CFG_REG{0x09};   // bit3 TMP_SHIFT_EN, bit2 PRS_SHIFT_EN
constexpr uint8_t REG_RESET{0x0C};
constexpr uint8_t REG_ID{0x0D};
constexpr uint8_t REG_COEF{0x10};  // 0x10..0x24 (21 bytes)
///@endcond
}  // namespace command
}  // namespace spa06

}  // namespace unit
}  // namespace m5
#endif
