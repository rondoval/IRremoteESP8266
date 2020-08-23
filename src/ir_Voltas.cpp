// Copyright 2020 David Conran (crankyoldgit)
// Copyright 2020 manj9501
/// @file
/// @brief Support for Voltas A/C protocol
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/1238

#include "ir_Voltas.h"
#include <algorithm>
#include <cstring>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRtext.h"
#include "IRutils.h"

using irutils::addBoolToString;
using irutils::addModeToString;
using irutils::addFanToString;
using irutils::addLabeledString;
using irutils::addTempToString;
using irutils::minsToString;

// Constants
const uint16_t kVoltasBitMark = 1026;   ///< uSeconds.
const uint16_t kVoltasOneSpace = 2553;  ///< uSeconds.
const uint16_t kVoltasZeroSpace = 554;  ///< uSeconds.
const uint16_t kVoltasFreq = 38000;     ///< Hz.

#if SEND_VOLTAS
/// Send a Voltas formatted message.
/// Status: ALPHA / Untested.
/// @param[in] data An array of bytes containing the IR command.
///                 It is assumed to be in MSB order for this code.
/// e.g.
/// @code
///   uint8_t data[kVoltasStateLength] = {0x33, 0x28, 0x88, 0x1A, 0x3B, 0x3B,
///                                       0x3B, 0x11, 0x00, 0x40};
/// @endcode
/// @param[in] nbytes Nr. of bytes of data in the array. (>=kVoltasStateLength)
/// @param[in] repeat Nr. of times the message is to be repeated.
void IRsend::sendVoltas(const uint8_t data[], const uint16_t nbytes,
                        const uint16_t repeat) {
  sendGeneric(0, 0,
              kVoltasBitMark, kVoltasOneSpace,
              kVoltasBitMark, kVoltasZeroSpace,
              kVoltasBitMark, kDefaultMessageGap,
              data, nbytes,
              kVoltasFreq, true, repeat, kDutyDefault);
}
#endif  // SEND_VOLTAS

#if DECODE_VOLTAS
/// Decode the supplied Voltas message.
/// Status: ALPHA / Untested.
/// @param[in,out] results Ptr to the data to decode & where to store the decode
/// @param[in] offset The starting index to use when attempting to decode the
///   raw data. Typically/Defaults to kStartOffset.
/// @param[in] nbits The number of data bits to expect.
/// @param[in] strict Flag indicating if we should perform strict matching.
/// @return A boolean. True if it can decode it, false if it can't.
bool IRrecv::decodeVoltas(decode_results *results, uint16_t offset,
                          const uint16_t nbits, const bool strict) {
  if (strict && nbits != kVoltasBits) return false;

  // Data + Footer
  if (!matchGeneric(results->rawbuf + offset, results->state,
                    results->rawlen - offset, nbits,
                    0, 0,  // No header
                    kVoltasBitMark, kVoltasOneSpace,
                    kVoltasBitMark, kVoltasZeroSpace,
                    kVoltasBitMark, kDefaultMessageGap, true)) return false;

  // Compliance
  if (strict && !IRVoltas::validChecksum(results->state, nbits / 8))
    return false;
  // Success
  results->decode_type = decode_type_t::VOLTAS;
  results->bits = nbits;
  return true;
}
#endif  // DECODE_VOLTAS

/// Class constructor
/// @param[in] pin GPIO to be used when sending.
/// @param[in] inverted Is the output signal to be inverted?
/// @param[in] use_modulation Is frequency modulation to be used?
IRVoltas::IRVoltas(const uint16_t pin, const bool inverted,
                   const bool use_modulation)
    : _irsend(pin, inverted, use_modulation) {
  stateReset();
}

// Reset the internal state to a fixed known good state.
void IRVoltas::stateReset() {
  // This resets to a known-good state.
  // ref: https://github.com/crankyoldgit/IRremoteESP8266/issues/1238#issuecomment-674699746
  const uint8_t kReset[kVoltasStateLength] = {
      0x33, 0x28, 0x00, 0x17, 0x3B, 0x3B, 0x3B, 0x11, 0x00, 0xCB};
  setRaw(kReset);
}

/// Set up hardware to be able to send a message.
void IRVoltas::begin() { _irsend.begin(); }

#if SEND_VOLTAS
/// Send the current internal state as an IR message.
/// @param[in] repeat Nr. of times the message will be repeated.
void IRVoltas::send(const uint16_t repeat) {
  _irsend.sendVoltas(getRaw(), kVoltasStateLength, repeat);
}
#endif  // SEND_VOLTAS

/// Get a PTR to the internal state/code for this protocol.
/// @return PTR to a code for this protocol based on the current internal state.
uint8_t* IRVoltas::getRaw(void) {
  checksum();  // Ensure correct settings before sending.
  return _.raw;
}

/// Set the internal state from a valid code for this protocol.
/// @param[in] new_code A valid code for this protocol.
void IRVoltas::setRaw(const uint8_t new_code[]) {
  std::memcpy(_.raw, new_code, kVoltasStateLength);
}

/// Calculate and set the checksum values for the internal state.
void IRVoltas::checksum(void) {
  _.Checksum = calcChecksum(_.raw);
}

/// Verify the checksum is valid for a given state.
/// @param[in] state The array to verify the checksum of.
/// @param[in] length The length of the state array.
/// @return true, if the state has a valid checksum. Otherwise, false.
bool IRVoltas::validChecksum(const uint8_t state[], const uint16_t length) {
  if (length) return state[length - 1] == calcChecksum(state, length);
  return true;
}

/// Calculate the checksum is valid for a given state.
/// @param[in] state The array to calculate the checksum of.
/// @param[in] length The length of the state array.
/// @return The valid checksum value for the state.
uint8_t IRVoltas::calcChecksum(const uint8_t state[], const uint16_t length) {
  uint8_t result = 0;
  if (length)
    result = sumBytes(state, length - 1);
  return ~result;
}

/// Change the power setting to On.
void IRVoltas::on() { setPower(true); }

/// Change the power setting to Off.
void IRVoltas::off() { setPower(false); }

/// Change the power setting.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRVoltas::setPower(const bool on) { _.Power = on; }

/// Get the value of the current power setting.
/// @return true, the setting is on. false, the setting is off.
bool IRVoltas::getPower(void) const { return _.Power; }

/// Set the operating mode of the A/C.
/// @param[in] mode The desired operating mode.
/// @note If we get an unexpected mode, default to AUTO.
void IRVoltas::setMode(const uint8_t mode) {
  switch (mode) {
    case kVoltasFan:
      setFan(kVoltasFanHigh);
      break;
    case kVoltasDry:
      setFan(kVoltasFanLow);
      setTemp(kVoltasDryTemp);
      break;
    case kVoltasHeat:
    case kVoltasCool:
      break;
    default:
      setMode(kVoltasCool);
      return;
  }
  _.Mode = mode;
}

/// Get the operating mode setting of the A/C.
/// @return The current operating mode setting.
uint8_t IRVoltas::getMode(void) { return _.Mode; }

/// Convert a stdAc::opmode_t enum into its native mode.
/// @param[in] mode The enum to be converted.
/// @return The native equivilant of the enum.
uint8_t IRVoltas::convertMode(const stdAc::opmode_t mode) {
  switch (mode) {
    case stdAc::opmode_t::kHeat: return kVoltasHeat;
    case stdAc::opmode_t::kDry:  return kVoltasDry;
    case stdAc::opmode_t::kFan:  return kVoltasFan;
    default:                     return kVoltasCool;
  }
}

/// Convert a native mode into its stdAc equivilant.
/// @param[in] mode The native setting to be converted.
/// @return The stdAc equivilant of the native setting.
stdAc::opmode_t IRVoltas::toCommonMode(const uint8_t mode) {
  switch (mode) {
    case kVoltasHeat: return stdAc::opmode_t::kHeat;
    case kVoltasDry:  return stdAc::opmode_t::kDry;
    case kVoltasFan:  return stdAc::opmode_t::kFan;
    default:            return stdAc::opmode_t::kCool;
  }
}

/// Set the temperature.
/// @param[in] temp The temperature in degrees celsius.
void IRVoltas::setTemp(const uint8_t temp) {
  uint8_t new_temp = std::max(kVoltasMinTemp, temp);
  new_temp = std::min(kVoltasMaxTemp, new_temp);
  _.Temp = new_temp - kVoltasMinTemp;
}

/// Get the current temperature setting.
/// @return The current setting for temp. in degrees celsius.
uint8_t IRVoltas::getTemp(void) { return _.Temp + kVoltasMinTemp; }

/// Set the speed of the fan.
/// @param[in] fan The desired setting.
void IRVoltas::setFan(const uint8_t fan) {
  switch (fan) {
    case kVoltasFanLow:
    case kVoltasFanMed:
    case kVoltasFanHigh:
    case kVoltasFanAuto:
      _.FanSpeed = fan;
      break;
    default:
      setFan(kVoltasFanAuto);
  }
}

/// Get the current fan speed setting.
/// @return The current fan speed/mode.
uint8_t IRVoltas::getFan(void) { return _.FanSpeed; }

/// Convert a stdAc::fanspeed_t enum into it's native speed.
/// @param[in] speed The enum to be converted.
/// @return The native equivilant of the enum.
uint8_t IRVoltas::convertFan(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin:
    case stdAc::fanspeed_t::kLow:    return kVoltasFanLow;
    case stdAc::fanspeed_t::kMedium: return kVoltasFanMed;
    case stdAc::fanspeed_t::kHigh:
    case stdAc::fanspeed_t::kMax:    return kVoltasFanHigh;
    default:                         return kVoltasFanAuto;
  }
}

/// Convert a native fan speed into its stdAc equivilant.
/// @param[in] spd The native setting to be converted.
/// @return The stdAc equivilant of the native setting.
stdAc::fanspeed_t IRVoltas::toCommonFanSpeed(const uint8_t spd) {
  switch (spd) {
    case kVoltasFanHigh: return stdAc::fanspeed_t::kMax;
    case kVoltasFanMed:  return stdAc::fanspeed_t::kMedium;
    case kVoltasFanLow:  return stdAc::fanspeed_t::kMin;
    default:             return stdAc::fanspeed_t::kAuto;
  }
}

/// Set the Vertical Swing setting of the A/C.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRVoltas::setSwingV(const bool on) { _.SwingV = on ? 0b111 : 0b000; }

/// Get the Vertical Swing setting of the A/C.
/// @return true, the setting is on. false, the setting is off.
bool IRVoltas::getSwingV(void) const { return _.SwingV == 0b111; }

/// Set the Horizontal Swing setting of the A/C.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRVoltas::setSwingH(const bool on) {
  _.SwingH = on;
  setSwingHChange(true);
}

/// Get the Horizontal Swing setting of the A/C.
/// @return true, the setting is on. false, the setting is off.
bool IRVoltas::getSwingH(void) const { return _.SwingH; }

/// Set the bits for changing the Horizontal Swing setting of the A/C.
/// @param[in] on true, the change bits are set.
///            false, the "no change" bits are set.
void IRVoltas::setSwingHChange(const bool on) {
  _.SwingHChange = on ? kVoltasSwingHChange : kVoltasSwingHNoChange;
  if (!on) _.SwingH = true;  // "No Change" also sets SwingH to 1.
}

/// Are the Horizontal Swing change bits set in the message?
/// @return true, the correct bits are set. false, the correct bits are not set.
bool IRVoltas::getSwingHChange(void) const {
  return _.SwingHChange == kVoltasSwingHChange;
}

/// Change the Wifi setting.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRVoltas::setWifi(const bool on) { _.Wifi = on; }

/// Get the value of the current Wifi setting.
/// @return true, the setting is on. false, the setting is off.
bool IRVoltas::getWifi(void) const { return _.Wifi; }

/// Change the Turbo setting.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRVoltas::setTurbo(const bool on) { _.Turbo = on; }

/// Get the value of the current Turbo setting.
/// @return true, the setting is on. false, the setting is off.
bool IRVoltas::getTurbo(void) const { return _.Turbo; }

/// Change the Econo setting.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRVoltas::setEcono(const bool on) { _.Econo = on; }

/// Get the value of the current Econo setting.
/// @return true, the setting is on. false, the setting is off.
bool IRVoltas::getEcono(void) const { return _.Econo; }

/// Change the Light setting.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRVoltas::setLight(const bool on) { _.Light = on; }

/// Get the value of the current Light setting.
/// @return true, the setting is on. false, the setting is off.
bool IRVoltas::getLight(void) const { return _.Light; }

/// Convert the current internal state into its stdAc::state_t equivilant.
/// @param[in] prev Ptr to the previous state if available.
/// @return The stdAc equivilant of the native settings.
stdAc::state_t IRVoltas::toCommon(const stdAc::state_t *prev) {
  stdAc::state_t result;
  // Start with the previous state if given it.
  if (prev != NULL) {
    result = *prev;
  } else {
    // Set defaults for non-zero values that are not implicitly set for when
    // there is no previous state.
    result.swingh = stdAc::swingh_t::kOff;
  }
  result.protocol = decode_type_t::VOLTAS;
  result.power = _.Power;
  result.mode = toCommonMode(_.Mode);
  result.celsius = true;
  result.degrees = getTemp();
  result.fanspeed = toCommonFanSpeed(_.FanSpeed);
  result.swingv = getSwingV() ? stdAc::swingv_t::kAuto : stdAc::swingv_t::kOff;
  if (getSwingHChange())
    result.swingh = _.SwingH ? stdAc::swingh_t::kAuto : stdAc::swingh_t::kOff;
  result.turbo = _.Turbo;
  result.econo = _.Econo;
  result.light = _.Light;
  // Not supported.
  result.model = -1;
  result.quiet = false;
  result.filter = false;
  result.clean = false;
  result.beep = false;
  result.sleep = -1;
  result.clock = -1;
  return result;
}

/// Convert the current internal state into a human readable string.
/// @return A human readable string.
String IRVoltas::toString() {
  String result = "";
  result.reserve(120);  // Reserve some heap for the string to reduce fragging.
  result += addBoolToString(_.Power, kPowerStr, false);
  result += addModeToString(_.Mode, 255, kVoltasCool, kVoltasHeat,
                            kVoltasDry, kVoltasFan);
  result += addTempToString(getTemp());
  result += addFanToString(_.FanSpeed, kVoltasFanHigh, kVoltasFanLow,
                           kVoltasFanAuto, kVoltasFanAuto, kVoltasFanMed);
  result += addBoolToString(getSwingV(), kSwingVStr);
  if (getSwingHChange())
    result += addBoolToString(_.SwingH, kSwingHStr);
  else
    result += addLabeledString(kNAStr, kSwingHStr);
  result += addBoolToString(_.Turbo, kTurboStr);
  result += addBoolToString(_.Econo, kEconoStr);
  result += addBoolToString(_.Wifi, kWifiStr);
  result += addBoolToString(_.Light, kLightStr);
  return result;
}
