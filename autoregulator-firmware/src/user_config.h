#pragma once

#include <Arduino.h>

// *** AUTO REGULATOR USER CONFIGURATION *** //

// --- Hardware ---

constexpr unsigned PIN_RELAY               = (A7);
constexpr unsigned PIN_POTENTIOMETER       = (A1);
constexpr unsigned PIN_NEOPIXEL            = (A2);
constexpr unsigned PIN_SENSOR_MANUAL_RESET = (-1); // (optional, leave as -1)
constexpr unsigned _PIN_SDA                = (A4); // can't be changed, just here for reference
constexpr unsigned _PIN_SCL                = (A5); // can't be changed, just here for reference

static_assert(_PIN_SDA == PIN_WIRE_SDA, "Changing I2C pins from hardware default is not supported");
static_assert(_PIN_SCL == PIN_WIRE_SCL, "Changing I2C pins from hardware default is not supported");

// --- Mechanics & Limits ---

// Suction: tune to preference, and your machine limits

constexpr float SUCTION_MAX_MBAR   = 170.0f; // 100 ~ 150mbar is ideal, could use further testing
constexpr float SUCTION_MIN_MBAR   = 55.0f;  // minimum targetable. if it's too low the noise of the pump turning on can exceed this in some situations
constexpr float SUCTION_HYSTERESIS = 30.0f;  // in mbar. low values cause rapid switching, higher values (may) reduce duty cycle at the cost of average suction
constexpr float MECHANICAL_MAX_HZ  = 0.8f;   // max (average) frequency the relay can switch on at. switching off is always immediate

static_assert(SUCTION_MAX_MBAR > SUCTION_MIN_MBAR, "Maximum suction must be greater than minimum suction");
static_assert(SUCTION_MIN_MBAR > SUCTION_HYSTERESIS, "The hysteresis cannot be larger than the minimum suction value");
static_assert(SUCTION_HYSTERESIS > 0.0f, "The controller hysteresis must be greater than zero");

// It's best not to change this. If and only if the relay driver is active low,
// AND your suction source is connected to COM+NO, you can change this to low.
// Do NOT connect the suction source to COM+NC, or power-off on reset will be impossible.
#define MECHANICAL_ACTIVE_LEVEL (HIGH)
static_assert(MECHANICAL_ACTIVE_LEVEL == HIGH, "Are you sure?");

// --- Colors ---

#define COLOR_PROCESSING_WAIT (0xFFFB08) // yellow
#define COLOR_SUCCESS         (0x00DF10) // green
#define COLOR_AWAITING_USER   (0x4EA5E0) // bluish
#define COLOR_ERROR           (0xFF0002) // red
#define COLOR_NONE_OFF        (0x000000) // (off)