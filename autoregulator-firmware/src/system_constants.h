#pragma once

#include <Arduino.h>

// *** AUTO REGULATOR SYSTEM CONFIGURATION *** //

// --- Hardware ---

#define AR_ADC_MAX      (1023)  // don't change this
#define AR_ADC_BITS     (10)    // don't change this: nano 328 default

// --- Limits ---
#define BACKUP_TIMER_MS    (3600000UL) // 1h in ms. machine will shut off at this time. not for solo; it's if the operator is struck by lightning.

const float PSI_ATMOSPHERIC               = 14.6959; // only used if calibration is ever bypassed for some reason
const float PSI_ATMOSPHERIC_MIN           = 10.20;   // 10kft@22C@11atm, used as a check in calibration, won't start if below this
const float STABILITY_CHECK_MAX_DEVIATION = 0.70;    // I get values around 0.02-0.05 sometimes, but up to 0.3 on occasion. this margin is probably generous
const int STABILITY_CHECK_SAMPLES         = 100;
const uint32_t STABILITY_CHECK_TIME_MS    = 2000;


// --- World Constants ---
const float PSI_TO_MBAR             = PSI_to_HPA;
const float MPR_MAX_HZ              = 160.0;
const float MPR_PSI_MIN             = 0.0;
const float MPR_PSI_MAX             = 25.0;
const float MPR_factoryDefault_Omin = 10.0;
const float MPR_factoryDefault_Omax = 90.0;
const float MPR_Kfactor             = 1.0; // set to one to force output in PSI

const uint32_t MILLIS_PER_SEC       = 1000;
const uint32_t MICROS_PER_MILLI     = 1000;
const uint32_t MILLIS_PER_MINUTE    = 60000;


// --- Application Config ---

#define SERIAL_ENUMERATION_DELAY         (100u) // way shorter than esp32 sweet
#define HEARTBEAT_PERIOD                 (750u)
#define PRINTERVAL                       (1000u) // stats printing period
#define POT_SAMPLE_SZ                    (10u)   // adc samples for pot averaging
#define POT_SAMPLE_DELAY                 (10u)   // total (minimum) time in ms over which to take the samples
#define POT_LOWER_CUTOFF_PERCENT         (0.05f) // pot will be considered switched off below this precentile of its range
#define LED_BRIGHTNESS_PERCENT           (0.32f) // neopixel brightness
#define LED_RAPID_FLASH_HZ               (5.0f)  // flashing rate while calibrating
#define LED_FLICKER_HZ                   (0.80f) // flickering rate while machine active
#define SENSOR_READINGS_PER_CONTROL_LOOP (4u)    // integer, readings are averaged, 2-8 seems the right balance
#define NUM_NEOPIXELS                    (1u)
#define CALIBRATION_VENT_TIME_MS         (1000u)