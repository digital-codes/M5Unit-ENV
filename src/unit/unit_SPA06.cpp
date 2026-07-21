/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_SPA06.cpp
  @brief UnitSPA06 (SPA06-003) barometric pressure + temperature
*/
#include "unit_SPA06.hpp"
#include <M5Utility.hpp>
#include <limits>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::spa06;
using namespace m5::unit::spa06::command;

namespace {
// ID (0x0D) reset value is 0x11h (datasheet Table 7 / Sec 7.10): REV_ID[7:4]=1, PROD_ID[3:0]=1.
constexpr uint8_t PROD_ID_VALUE{0x01};
constexpr uint8_t SOFT_RESET{0x09};         // RESET(0x0C) SOFT_RST[3:0] = 1001b (datasheet Sec 7.9)
constexpr uint8_t MEAS_CTRL_STANDBY{0x00};  // MEAS_CFG(0x08) MEAS_CTRL[2:0] = idle/stop
constexpr uint8_t P_SHIFT_BIT{0x04};        // CFG_REG(0x09) bit2 PRS_SHIFT_EN
constexpr uint8_t T_SHIFT_BIT{0x08};        // CFG_REG(0x09) bit3 TMP_SHIFT_EN
constexpr uint8_t PRS_RDY_BIT{0x10};        // MEAS_CFG(0x08) bit4 PRS_RDY
constexpr uint8_t TMP_RDY_BIT{0x20};        // MEAS_CFG(0x08) bit5 TMP_RDY
constexpr uint8_t COEF_SENSOR_RDY_MASK{0xC0};
// Measurement period (ms) per Rate = 1000 / (measurements per second), indexed by the Rate nibble
constexpr uint32_t interval_table[] = {1000, 500, 250, 125, 62, 31, 15, 7};
}  // namespace

namespace m5 {
namespace unit {

const char UnitSPA06::name[] = "UnitSPA06";
const types::uid_t UnitSPA06::uid{"UnitSPA06"_mmh3};
const types::attr_t UnitSPA06::attr{attribute::AccessI2C};

UnitSPA06::UnitSPA06(const uint8_t addr)
    : Component(addr), _data{new m5::container::CircularBuffer<spa06::Data>(spa06::DEFAULT_STORED_SIZE)}
{
    auto ccfg        = component_config();
    ccfg.clock       = 400 * 1000U;
    ccfg.stored_size = spa06::DEFAULT_STORED_SIZE;
    component_config(ccfg);
}

bool UnitSPA06::begin()
{
    // Re-entrant begin(): make sure _periodic and the hardware never get out of sync
    stop_periodic_measurement();

    auto ssize = stored_size();
    if (ssize != _data->capacity()) {
        _data.reset(new m5::container::CircularBuffer<spa06::Data>(ssize));
        if (!_data) {
            M5_LIB_LOGE("Failed to allocate");
            return false;
        }
    }
    _data->clear();
    _pushed = _consumed = 0;

    uint8_t id{};
    if (!readRegister8(REG_ID, id, 0) || (id & 0x0F) != PROD_ID_VALUE) {
        M5_LIB_LOGE("Not SPA06 %02X", id);
        return false;
    }
    if (!softReset()) {
        return false;
    }

    // Wait for sensor + coefficients ready (MEAS_CFG bit7 COEF_RDY, bit6 SENSOR_RDY).
    bool ready{};
    for (int i = 0; i < 100; ++i) {
        uint8_t st{};
        if (readRegister8(REG_MEAS_CFG, st, 0) && (st & COEF_SENSOR_RDY_MASK) == COEF_SENSOR_RDY_MASK) {
            ready = true;
            break;
        }
        m5::utility::delay(10);
    }
    if (!ready) {
        M5_LIB_LOGE("Sensor not ready");
        return false;
    }
    if (!read_coefficients()) {
        return false;
    }

    return _cfg.start_periodic ? startPeriodicMeasurement(_cfg.mode, _cfg.pressure_oversampling,
                                                          _cfg.temperature_oversampling, _cfg.rate)
                               : true;
}

bool UnitSPA06::softReset()
{
    if (inPeriodic()) {
        M5_LIB_LOGE("Periodic measurements are running");
        return false;
    }
    if (!writeRegister8(REG_RESET, SOFT_RESET)) {
        M5_LIB_LOGE("Failed to reset");
        return false;
    }
    m5::utility::delay(50);
    return true;
}

bool UnitSPA06::read_coefficients()
{
    std::array<uint8_t, 21> r{};
    if (!readRegister(REG_COEF, r.data(), r.size(), 0)) {
        M5_LIB_LOGE("Failed to read coefficients");
        return false;
    }
    _coeffs = spa06::parse_coeffs(r);
    return true;
}

void UnitSPA06::update(const bool force)
{
    _updated = false;
    if (inPeriodic()) {
        const auto at = m5::utility::millis();
        if (force || !_latest || at >= _latest + _interval) {
            spa06::Data d{};
            _updated = read_measurement(d);
            if (_updated) {
                _data->push_back(d);
                ++_pushed;
                _latest = at;
            }
        }
    }
}

bool UnitSPA06::read_measurement(spa06::Data& d)
{
    // Read the PSR+TMP data registers directly every _interval (no status/RDY gate), matching the other
    // barometers (BMP280/QMP6988). A register that has not been measured yet reads the reset value
    // 0x800000 (NOT_MEASURED); use that as the not-ready signal instead of polling MEAS_CFG.
    if (!readRegister(REG_PSR_B2, d.raw.data(), d.raw.size(), 0)) {
        M5_LIB_LOGE("Failed to read data");
        return false;
    }
    const bool p_measured = d.psr_raw() != spa06::NOT_MEASURED;
    const bool t_measured = d.tmp_raw() != spa06::NOT_MEASURED;

    switch (_cfg.mode) {
        case Mode::Pressure:
            // Temperature is not measured continuously; supply the seeded traw for pressure compensation.
            d.raw[3] = static_cast<uint8_t>(_seed_tmp_raw >> 16);
            d.raw[4] = static_cast<uint8_t>(_seed_tmp_raw >> 8);
            d.raw[5] = static_cast<uint8_t>(_seed_tmp_raw);
            return p_measured;
        case Mode::Temperature:
            return t_measured;
        case Mode::PressureAndTemperature:
        default:
            return p_measured && t_measured;
    }
}

bool UnitSPA06::seed_temperature()
{
    // Prime traw for pressure compensation with one temperature reading. Single-shot (command mode)
    // temperature does not reliably set TMP_RDY on SPA06-003, so use a brief continuous-temperature run
    // (background mode, the proven-reliable path) at the lowest rate -- Rate1 x any oversampling always
    // fits the 1 s budget -- and read the register directly (same NOT_MEASURED convention as periodic).
    const uint8_t tmp_cfg = static_cast<uint8_t>((static_cast<uint8_t>(Rate::Rate1) << 4) |
                                                 static_cast<uint8_t>(_cfg.temperature_oversampling));
    if (!writeRegister8(REG_TMP_CFG, tmp_cfg) ||
        !writeRegister8(REG_MEAS_CFG, static_cast<uint8_t>(Mode::Temperature))) {
        M5_LIB_LOGE("Failed to start temperature seed");
        return false;
    }

    // The first continuous measurement completes after the temperature measurement time (<=206.8 ms).
    const uint32_t timeout_ms = spa06::measurement_time_x10(_cfg.temperature_oversampling) / 10 * 2 + 200;
    const auto start          = m5::utility::millis();
    bool ok                   = false;
    while (m5::utility::millis() - start < timeout_ms) {
        m5::utility::delay(5);
        std::array<uint8_t, 3> t{};
        if (!readRegister(REG_TMP_B2, t.data(), t.size(), 0)) {
            continue;
        }
        const int32_t traw =
            spa06::sign_extend((static_cast<uint32_t>(t[0]) << 16) | (static_cast<uint32_t>(t[1]) << 8) | t[2], 24);
        if (traw != spa06::NOT_MEASURED) {
            _seed_tmp_raw = traw;
            ok            = true;
            break;
        }
    }
    // Stop the seed measurement so the caller can start the requested mode from standby.
    writeRegister8(REG_MEAS_CFG, MEAS_CTRL_STANDBY);
    if (!ok) {
        M5_LIB_LOGE("Temperature seed timed out");
    }
    return ok;
}

bool UnitSPA06::start_periodic_measurement(const Mode mode, const Oversampling pressure_oversampling,
                                           const Oversampling temperature_oversampling, const Rate rate)
{
    if (inPeriodic()) {
        return false;
    }

    // Reject combinations that exceed the datasheet timing budget before touching any register.
    if (!spa06::valid_combination(pressure_oversampling, temperature_oversampling, rate, mode)) {
        M5_LIB_LOGE("Combination exceeds the measurement time budget (datasheet 7.3)");
        return false;
    }

    // The enum value is the PM_PRC/TM_PRC (oversampling) and PM_RATE/TMP_RATE (rate) nibble directly.
    const uint8_t rate_bits = static_cast<uint8_t>(rate);
    const uint8_t prs_cfg   = static_cast<uint8_t>((rate_bits << 4) | static_cast<uint8_t>(pressure_oversampling));
    const uint8_t tmp_cfg   = static_cast<uint8_t>((rate_bits << 4) | static_cast<uint8_t>(temperature_oversampling));
    uint8_t cfg_reg{};
    // The datasheet requires the SHIFT bit to be enabled whenever oversampling exceeds 8x.
    if (pressure_oversampling > Oversampling::X8) {
        cfg_reg |= P_SHIFT_BIT;
    }
    if (temperature_oversampling > Oversampling::X8) {
        cfg_reg |= T_SHIFT_BIT;
    }
    if (!writeRegister8(REG_PRS_CFG, prs_cfg) || !writeRegister8(REG_TMP_CFG, tmp_cfg) ||
        !writeRegister8(REG_CFG_REG, cfg_reg)) {
        M5_LIB_LOGE("Failed to configure");
        return false;
    }

    _kp                           = spa06::scale_factor(pressure_oversampling);
    _kt                           = spa06::scale_factor(temperature_oversampling);
    _cfg.pressure_oversampling    = pressure_oversampling;
    _cfg.temperature_oversampling = temperature_oversampling;
    _cfg.rate                     = rate;
    _cfg.mode                     = mode;

    // Pressure-only mode never measures temperature, so seed traw once for pressure compensation.
    if (mode == Mode::Pressure && !seed_temperature()) {
        return false;
    }

    // Start the configured background measurement. Software I2C occasionally drops this single write, so
    // verify it landed (MEAS_CTRL[2:0] reads back the mode) and retry.
    bool started = false;
    for (int i = 0; i < 5 && !started; ++i) {
        uint8_t st{};
        if (writeRegister8(REG_MEAS_CFG, static_cast<uint8_t>(mode)) && readRegister8(REG_MEAS_CFG, st, 0) &&
            (st & 0x07) == static_cast<uint8_t>(mode)) {
            started = true;
            break;
        }
        m5::utility::delay(5);
    }
    if (!started) {
        M5_LIB_LOGE("Failed to start measurement");
        return false;
    }

    // Prime once: the data registers can still hold the previous configuration's result. Discard it (which
    // also clears any stale RDY), then wait -- one-time poll, not a per-read gate -- for a measurement taken
    // with the current settings, so the first periodic read never compensates a leftover raw with the new
    // scale factor. RDY is reliable in background mode (unlike the single-shot command path).
    {
        const uint8_t rdy = (mode == Mode::Pressure)      ? PRS_RDY_BIT
                            : (mode == Mode::Temperature) ? TMP_RDY_BIT
                                                          : static_cast<uint8_t>(PRS_RDY_BIT | TMP_RDY_BIT);
        std::array<uint8_t, 6> discard{};
        readRegister(REG_PSR_B2, discard.data(), discard.size(), 0);
        // Worst case: the discard clears RDY right after a cycle, so wait up to one rate period plus a
        // measurement time (x2 margin) for the next completion.
        const uint32_t period = interval_table[m5::stl::to_underlying(rate)];
        const uint32_t budget = period +
                                (spa06::measurement_time_x10(pressure_oversampling) +
                                 spa06::measurement_time_x10(temperature_oversampling)) /
                                    10 * 2 +
                                200;
        const auto pstart = m5::utility::millis();
        while (m5::utility::millis() - pstart < budget) {
            uint8_t st{};
            if (readRegister8(REG_MEAS_CFG, st, 0) && (st & rdy) == rdy) {
                break;
            }
            m5::utility::delay(5);
        }
    }

    _periodic = true;
    _latest   = 0;  // fresh data is now in the registers; the first update() reads it
    _interval = interval_table[m5::stl::to_underlying(rate)];
    return true;
}

bool UnitSPA06::start_periodic_measurement()
{
    return start_periodic_measurement(_cfg.mode, _cfg.pressure_oversampling, _cfg.temperature_oversampling, _cfg.rate);
}

bool UnitSPA06::stop_periodic_measurement()
{
    writeRegister8(REG_MEAS_CFG, MEAS_CTRL_STANDBY);
    _periodic = false;
    return true;
}

bool UnitSPA06::startPeriodicMeasurement(const Mode mode, const Oversampling pressure_oversampling,
                                         const Oversampling temperature_oversampling, const Rate rate)
{
    return PeriodicMeasurementAdapter<UnitSPA06, spa06::Data>::startPeriodicMeasurement(mode, pressure_oversampling,
                                                                                        temperature_oversampling, rate);
}

bool UnitSPA06::startPeriodicMeasurement()
{
    return PeriodicMeasurementAdapter<UnitSPA06, spa06::Data>::startPeriodicMeasurement();
}

bool UnitSPA06::stopPeriodicMeasurement()
{
    return PeriodicMeasurementAdapter<UnitSPA06, spa06::Data>::stopPeriodicMeasurement();
}

bool UnitSPA06::measureSingleshot(spa06::Data& d, const Oversampling pressure_oversampling,
                                  const Oversampling temperature_oversampling)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }
    // Configure P+T at the lowest rate (a single reading; Rate1 x any oversampling always fits the budget).
    const uint8_t rate_bits = static_cast<uint8_t>(Rate::Rate1);
    const uint8_t prs_cfg   = static_cast<uint8_t>((rate_bits << 4) | static_cast<uint8_t>(pressure_oversampling));
    const uint8_t tmp_cfg   = static_cast<uint8_t>((rate_bits << 4) | static_cast<uint8_t>(temperature_oversampling));
    uint8_t cfg_reg{};
    if (pressure_oversampling > Oversampling::X8) {
        cfg_reg |= P_SHIFT_BIT;
    }
    if (temperature_oversampling > Oversampling::X8) {
        cfg_reg |= T_SHIFT_BIT;
    }
    if (!writeRegister8(REG_PRS_CFG, prs_cfg) || !writeRegister8(REG_TMP_CFG, tmp_cfg) ||
        !writeRegister8(REG_CFG_REG, cfg_reg)) {
        M5_LIB_LOGE("Failed to configure");
        return false;
    }
    if (!measure_singleshot(d)) {
        return false;
    }
    // Stamp the compensation inputs by value so the returned Data is self-contained.
    d.coeffs = _coeffs;
    d.kp     = spa06::scale_factor(pressure_oversampling);
    d.kt     = spa06::scale_factor(temperature_oversampling);
    return true;
}

bool UnitSPA06::measure_singleshot(spa06::Data& d)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }

    // Command-mode single-shot is unreliable on SPA06-003, so take one reading from a brief continuous P+T
    // run. Verify the start write landed (software I2C occasionally drops it) and retry.
    bool started = false;
    for (int i = 0; i < 5 && !started; ++i) {
        uint8_t st{};
        if (writeRegister8(REG_MEAS_CFG, static_cast<uint8_t>(Mode::PressureAndTemperature)) &&
            readRegister8(REG_MEAS_CFG, st, 0) && (st & 0x07) == static_cast<uint8_t>(Mode::PressureAndTemperature)) {
            started = true;
            break;
        }
        m5::utility::delay(5);
    }
    if (!started) {
        M5_LIB_LOGE("Failed to start measurement");
        return false;
    }

    // Discard any result left over from a previous configuration, then wait for a fresh coherent P+T pair.
    std::array<uint8_t, 6> discard{};
    readRegister(REG_PSR_B2, discard.data(), discard.size(), 0);
    bool ok             = false;
    const auto start_at = m5::utility::millis();
    while (m5::utility::millis() - start_at < 2 * 1000) {  // 2s cap
        m5::utility::delay(5);
        uint8_t st{};
        if (readRegister8(REG_MEAS_CFG, st, 0) && (st & (PRS_RDY_BIT | TMP_RDY_BIT)) == (PRS_RDY_BIT | TMP_RDY_BIT)) {
            ok = readRegister(REG_PSR_B2, d.raw.data(), d.raw.size(), 0);
            break;
        }
    }
    writeRegister8(REG_MEAS_CFG, MEAS_CTRL_STANDBY);  // Return to standby
    if (!ok) {
        M5_LIB_LOGE("Singleshot timed out");
    }
    return ok;
}

bool UnitSPA06::readOversampling(Oversampling& pressure_oversampling, Oversampling& temperature_oversampling)
{
    uint8_t prs{}, tmp{};
    if (!readRegister8(REG_PRS_CFG, prs, 0) || !readRegister8(REG_TMP_CFG, tmp, 0)) {
        return false;
    }
    pressure_oversampling    = static_cast<Oversampling>(prs & 0x07);  // PM_PRC[2:0] (nibble = enum value)
    temperature_oversampling = static_cast<Oversampling>(tmp & 0x07);  // TM_PRC[2:0]
    return true;
}

bool UnitSPA06::writeOversamplingPressure(const Oversampling pressure_oversampling)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }
    uint8_t prs{}, cfg{};
    if (!readRegister8(REG_PRS_CFG, prs, 0) || !readRegister8(REG_CFG_REG, cfg, 0)) {
        return false;
    }
    prs = static_cast<uint8_t>((prs & 0xF0) | static_cast<uint8_t>(pressure_oversampling));  // keep the rate nibble
    cfg = (pressure_oversampling > Oversampling::X8) ? static_cast<uint8_t>(cfg | P_SHIFT_BIT)
                                                     : static_cast<uint8_t>(cfg & ~P_SHIFT_BIT);
    if (!writeRegister8(REG_PRS_CFG, prs) || !writeRegister8(REG_CFG_REG, cfg)) {
        return false;
    }
    _kp                        = spa06::scale_factor(pressure_oversampling);
    _cfg.pressure_oversampling = pressure_oversampling;
    return true;
}

bool UnitSPA06::writeOversamplingTemperature(const Oversampling temperature_oversampling)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }
    uint8_t tmp{}, cfg{};
    if (!readRegister8(REG_TMP_CFG, tmp, 0) || !readRegister8(REG_CFG_REG, cfg, 0)) {
        return false;
    }
    tmp = static_cast<uint8_t>((tmp & 0xF0) | static_cast<uint8_t>(temperature_oversampling));  // keep the rate nibble
    cfg = (temperature_oversampling > Oversampling::X8) ? static_cast<uint8_t>(cfg | T_SHIFT_BIT)
                                                        : static_cast<uint8_t>(cfg & ~T_SHIFT_BIT);
    if (!writeRegister8(REG_TMP_CFG, tmp) || !writeRegister8(REG_CFG_REG, cfg)) {
        return false;
    }
    _kt                           = spa06::scale_factor(temperature_oversampling);
    _cfg.temperature_oversampling = temperature_oversampling;
    return true;
}

bool UnitSPA06::writeOversampling(const Oversampling pressure_oversampling, const Oversampling temperature_oversampling)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }
    return writeOversamplingPressure(pressure_oversampling) && writeOversamplingTemperature(temperature_oversampling);
}

bool UnitSPA06::readRate(Rate& rate)
{
    uint8_t prs{};
    if (!readRegister8(REG_PRS_CFG, prs, 0)) {
        return false;
    }
    rate = static_cast<Rate>((prs >> 4) & 0x07);  // PM_RATE[2:0]
    return true;
}

bool UnitSPA06::writeRate(const Rate rate)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }
    uint8_t prs{}, tmp{};
    if (!readRegister8(REG_PRS_CFG, prs, 0) || !readRegister8(REG_TMP_CFG, tmp, 0)) {
        return false;
    }
    const uint8_t r = static_cast<uint8_t>(static_cast<uint8_t>(rate) << 4);
    prs             = static_cast<uint8_t>((prs & 0x0F) | r);  // keep the PM_PRC nibble
    tmp             = static_cast<uint8_t>((tmp & 0x0F) | r);  // keep the TM_PRC nibble
    if (!writeRegister8(REG_PRS_CFG, prs) || !writeRegister8(REG_TMP_CFG, tmp)) {
        return false;
    }
    _cfg.rate = rate;
    return true;
}

float UnitSPA06::pressure() const
{
    if (_cfg.mode == Mode::Temperature) {
        return std::numeric_limits<float>::quiet_NaN();  // Pressure is not measured in temperature-only mode
    }
    if (!_data || _data->empty()) {
        return 0.0f;
    }
    const auto d = _data->back();
    if (!d) {
        return 0.0f;
    }
    return spa06::compensate_pressure(d->psr_raw(), d->tmp_raw(), _coeffs, _kp, _kt) / 100.0f;  // Pa -> hPa
}

float UnitSPA06::temperature() const
{
    if (_cfg.mode == Mode::Pressure) {
        return std::numeric_limits<float>::quiet_NaN();  // Seeded traw is internal; not a user reading
    }
    if (!_data || _data->empty()) {
        return 0.0f;
    }
    const auto d = _data->back();
    if (!d) {
        return 0.0f;
    }
    return spa06::compensate_temperature(d->tmp_raw(), _coeffs, _kt);
}

float UnitSPA06::altitude(const float sea_level_hpa) const
{
    return spa06::altitude_m(pressure(), sea_level_hpa);
}

float UnitSPA06::relativeAltitude() const
{
    return spa06::altitude_m(pressure(), _reference_hpa);
}

void UnitSPA06::setReference()
{
    _reference_hpa = pressure();
}

}  // namespace unit
}  // namespace m5
