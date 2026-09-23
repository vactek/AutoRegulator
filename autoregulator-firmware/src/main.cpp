/**
  AutoRegulator Firmware for Arduino Nano / Nano33BLE
  
  We toggle a relay based on a sensor reading and threshold, set by a knob.

  2026
  
  Tip: Flash a DEV_ version to wait-for-serial

*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPRLS.h>
#include <Adafruit_NeoPixel.h>

#include "user_config.h"
#include "system_constants.h"

#include "app_debug.h"
#include "util.h"


// ### GLOBALS

// peripheral objects
Adafruit_NeoPixel g_neopixel(NUM_NEOPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ400);
Adafruit_MPRLS g_mpr25 = Adafruit_MPRLS(PIN_SENSOR_MANUAL_RESET, -1, MPR_PSI_MIN, MPR_PSI_MAX, MPR_factoryDefault_Omin, MPR_factoryDefault_Omax, MPR_Kfactor); // no rst/eoc

// vars
float calibration_ref_psi = PSI_ATMOSPHERIC;
float last_known_psi      = 0.0; // ONLY used for stats
// ONLY set this by enableSuction/disableSuction functions!
// ONLY read this for controlling LEDs!
bool isSuctionActive     = false;
float targetPressure     = 99999; // used by control system, AND for stats. units mbar
float potentiometerValue = 0.0;   // pot 0.0 - 1.0
// stats
uint32_t started_this_pull_at       = 0;    // if 0, we can assume we haven't started a pull down, but it's better to check isSuctionActive to be sure.
bool waiting_for_inital_pulldown    = true; // turns false the first time the target is reached
uint32_t first_pulldown_achieved_at = 0;
uint32_t this_pulldown_achieved_at  = 0; // also used for LED animation upon pulldown
// these vars are the number of counts spent "pulling down" a vacuum in the main loop
// and the total counts where the target pressure is met while in the main loop, respectively
// where one count occurs during one loop
uint32_t vac_duty_on_post_pulldown  = 0;
uint32_t vac_duty_off_post_pulldown = 0;

// elapsed-timers used by multiple functions
uint32_t time_of_last_mech_change = 0;
uint32_t time_since_last_mech_change() { return millis() - time_of_last_mech_change; }

// ### FUNCTIONS

void led_heartbeat() {
    // simple check if main loop is running. 0x20 -> brightness
    analogWrite(LED_BUILTIN, millis() % HEARTBEAT_PERIOD > (HEARTBEAT_PERIOD >> 1) ? 0x20 : LOW);
}

void configure_pins() {

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, (MECHANICAL_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);

    pinMode(PIN_NEOPIXEL, OUTPUT);
    pinMode(PIN_POTENTIOMETER, INPUT);

    // todo: dynamically (or statically if it's possible)
    // force the ADC resolution to 10 bits
    // in a way that works on all supported boards
}

bool userInputValid() {
    // true if an input is asserted; does not sample the adc, so you must call update_pot_val() first.
    return potentiometerValue > POT_LOWER_CUTOFF_PERCENT;
}

float get_single_pot_value() { 
    // just read the ADC, doesn't touch global state
    return mapfloat(analogRead(PIN_POTENTIOMETER), 0, AR_ADC_MAX, 0.0, 1.0); 
}

void disableSuction() {
    // (ideally) no effect if suction already disabled
    uint8_t state_prev = digitalRead(PIN_RELAY);
    uint8_t state_new  = (MECHANICAL_ACTIVE_LEVEL == HIGH) ? LOW : HIGH;
    digitalWrite(PIN_RELAY, state_new);
    isSuctionActive = false;
    if (state_new != state_prev) {
        time_of_last_mech_change = millis();
        started_this_pull_at     = 0;
    }
}

void disableSuctionFS() {
    // immediately disable suction, none of the fluff
    digitalWrite(PIN_RELAY, !(MECHANICAL_ACTIVE_LEVEL));
}

void enableSuction() {
    // no effect if suction already enabled
    // enables suction as soon as possible
    // only held up for mechanical wear reasons
    uint8_t state_prev = digitalRead(PIN_RELAY);
    uint8_t state_new  = (MECHANICAL_ACTIVE_LEVEL == HIGH) ? HIGH : LOW;

    if (state_new != state_prev) {
        while (time_since_last_mech_change() <= (float(MILLIS_PER_SEC) / MECHANICAL_MAX_HZ)) {
            // while in this spin, we still want to check the potentiometer in case the user turns it back off
            // don't replace this with !userInputValid, we actually have to poll it ourselves
            // the global isn't updated, and we don't want to take an average.
            if (get_single_pot_value() < POT_LOWER_CUTOFF_PERCENT) {
                disableSuction();
                return;
            }
        }
        time_of_last_mech_change = millis();
        started_this_pull_at     = time_of_last_mech_change;
    }
    digitalWrite(PIN_RELAY, state_new);
    isSuctionActive = true;
}

void update_pot_val() {
    // updates the global variable
    uint32_t running_total = 0;
    for (unsigned k = 0; k < POT_SAMPLE_SZ; k++) {
        running_total += analogRead(PIN_POTENTIOMETER);
        // let's not complicate it further, and pretend that analogRead is instantaneous
        delayMicroseconds((MICROS_PER_MILLI * POT_SAMPLE_DELAY) / POT_SAMPLE_SZ);
    }

    float avg          = running_total / POT_SAMPLE_SZ;
    potentiometerValue = mapfloat(avg, 0, AR_ADC_MAX, 0.0, 1.0);
}

void neopixel_show_i2c_rate_limited() {
    // calls g_neopixel.show but only if we didn't call it in the last 2ms
    // todo: the actual rate limit, lmao
    g_neopixel.show();
}

void neopixel_show_color(uint32_t c) {
    g_neopixel.fill(c);
    g_neopixel.setBrightness(LED_BRIGHTNESS_PERCENT * 0xFF);
    neopixel_show_i2c_rate_limited();
}

void neopixel_flash_color(uint32_t c) {
    // non-blockingly flash a color
    static unsigned period = MILLIS_PER_SEC / LED_RAPID_FLASH_HZ; // ms

    if ((millis() % period) > (period >> 1)) {
        g_neopixel.fill(c);
        g_neopixel.setBrightness(LED_BRIGHTNESS_PERCENT * 0xFF);
    } else {
        g_neopixel.clear();
    }
    neopixel_show_i2c_rate_limited();
}

void _on_fail(void) {
    // ! invoke with EXIT_FAILURE_FN() - NOT DIRECTLY !
    // function to run when something goes wrong.
    disableSuction();
    DEBUGLN(F("on_fail called, halting"));
    while (true) {
        neopixel_flash_color(COLOR_ERROR);
    }
}
#define EXIT_FAILURE_FN()                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                          \
        disableSuctionFS();                                                                                                                                                                                                                    \
        TIMESTAMPLN();                                                                                                                                                                                                                         \
        _on_fail();                                                                                                                                                                                                                            \
    }

void periodicStatsPrint() {
#ifdef CAN_STORE_MANY_STRINGS
    static uint32_t loopcounter        = 0;
    static uint32_t time_of_last_print = 0;
    loopcounter++;
    if ((millis() - time_of_last_print) < PRINTERVAL) {
        return;
    }

    TIMESTAMPLN();

    DEBUG("> Superloop ARR: ");
    DEBUG(float(loopcounter) / ((millis() - time_of_last_print) / float(MILLIS_PER_SEC)));
    DEBUGLN(" Hz");
    loopcounter = 0;

    DEBUG("> Raw pressure (Psi): ");
    DEBUGLN(last_known_psi, 2);

    DEBUG("> Estimated Suction (mbar): ");
    float curSuction = (calibration_ref_psi - last_known_psi) * PSI_TO_MBAR; // mbar
    curSuction       = max(0.0f, curSuction);                                // clip
    DEBUG(curSuction, 2);
    static float ambient_mbar = calibration_ref_psi * PSI_TO_MBAR; // I know the controller does this calculation too, not worth making it global though
    float targetSuction       = ambient_mbar - targetPressure;
    if (isSuctionActive) {
        DEBUG(" (");
        DEBUG(100.0 * curSuction / targetSuction, 1);
        DEBUG("% of target, ");
        DEBUG(100.0 * curSuction / SUCTION_MAX_MBAR, 1);
        DEBUGLN("% of maximum)");
    } else {
        DEBUGLN();
    }

    DEBUG("> State: ");
    if (isSuctionActive) {
        if (waiting_for_inital_pulldown) {
            DEBUG("pulling down for ");
        } else {
            DEBUG("assisting pressure for ");
        }
        DEBUG((millis() - started_this_pull_at) / float(MILLIS_PER_SEC), 1);
        DEBUGLN("s");
    } else if (userInputValid()) {
        DEBUGLN("pressure holding on its own");
    } else {
        DEBUGLN("shut off");
    }

    if (waiting_for_inital_pulldown) {
        DEBUGLN("> Duty cycle will show after initial pulldown");
    } else {
        if (vac_duty_off_post_pulldown == 0) {
            vac_duty_off_post_pulldown = 1; // prevent divide by zero in edge case
        }
        DEBUG("> Duty cycle (since pulldown at t=");
        DEBUG(first_pulldown_achieved_at);
        DEBUGLN("):");
        DEBUG((100 * vac_duty_on_post_pulldown) / (vac_duty_on_post_pulldown + vac_duty_off_post_pulldown));
        DEBUGLN("%");
    }

    DEBUG("> Time remaining before operator is required to reboot: ");
    DEBUG((BACKUP_TIMER_MS - millis()) / float(MILLIS_PER_MINUTE), 1);
    DEBUGLN(" minutes");

    time_of_last_print = millis(); // put last so that long serial prints dont affect reported frequency as much
#endif
}

void doNeopixelColorBasedOnPot() {
    const int num_flashes_upon_success = 3;
    bool shouldPulsate                 = isSuctionActive;
    if (!userInputValid()) {
        g_neopixel.clear();
        neopixel_show_i2c_rate_limited();
        return;
    }

    // fancy magic color mapping from green to red
    g_neopixel.fill(g_neopixel.ColorHSV(mapfloat(potentiometerValue, POT_LOWER_CUTOFF_PERCENT, 1.0, (0.36 * 65535), (0.0 * 65535))));

    // stage 2: set brightness, based on activity
    // pulsate while suction active
    // blink green when pulldown achieved
    // solid when vacuum is holding
    if (shouldPulsate) {
        // we use both halves of the sine wave
        // so there's an invisible divided by two and times two that cancel out
        // g_neopixel.gamma8( looked bad
        g_neopixel.setBrightness(LED_BRIGHTNESS_PERCENT * 0xFF * abs(sinf(LED_FLICKER_HZ * PI * millis() / float(MILLIS_PER_SEC))));
    } else {
        if ((millis() - this_pulldown_achieved_at) <= (MILLIS_PER_SEC * num_flashes_upon_success / LED_RAPID_FLASH_HZ)) {
            // detour, we want a completely different output driver
            neopixel_flash_color(COLOR_SUCCESS);
            return;
        }
        g_neopixel.setBrightness(LED_BRIGHTNESS_PERCENT * 0xFF);
    }

    neopixel_show_i2c_rate_limited();
}

float get_mpr_sample() {
    // return a sample as soon as possible, blocks if called too rapidly
    // units set at object instantiation, should be PSI since K=1
    static uint32_t time_of_last_mpr_read = 0;
    while ((millis() - time_of_last_mpr_read) <= (float(MILLIS_PER_SEC) / MPR_MAX_HZ)) {
        yield();
    }
    delay(1); // we can yeep for a sec
    float sample          = g_mpr25.readPressure();
    time_of_last_mpr_read = millis();
    last_known_psi        = sample; // for statistics, do not use elsewhere
    time_of_last_mpr_read = millis();
    if (isnanf(sample)) {
        EXIT_FAILURE_FN();
    }
    return sample;
}

float get_mpr_average(unsigned n) {
    // sample the sensor a handful of times and return the average PSI
    if (n <= 0) {
        return 0.0f;
    }

    float accumulator = 0;
    for (unsigned k = 0; k < n; k++) {
        accumulator += get_mpr_sample();
    }
    return accumulator / n;
}

void calibrate_and_check() {
    // get the actual, local pressure today for accuracy
    DEBUGLN("Calibrating");
    uint32_t calibration_start = millis();
    disableSuction();
    // todo: open vents here if they exist
    while (millis() < (calibration_start + CALIBRATION_VENT_TIME_MS)) {
        neopixel_flash_color(COLOR_PROCESSING_WAIT);
    }

    // check if pressure reading is stable
    float accumulator = 0;
    float min         = 9999;
    float max         = -9999;
    for (int k = 0; k < STABILITY_CHECK_SAMPLES; k++) {
        float s = get_mpr_sample();
        DEBUGLN(s);
        uint32_t stab_sampl_start = millis();
        while (millis() < (stab_sampl_start + STABILITY_CHECK_TIME_MS / STABILITY_CHECK_SAMPLES)) {
            neopixel_flash_color(COLOR_PROCESSING_WAIT);
        }
        accumulator += s;
        if (s < min) {
            min = s;
        }
        if (s > max) {
            max = s;
        }
    }
    float range         = max - min;
    calibration_ref_psi = accumulator / STABILITY_CHECK_SAMPLES;

    DEBUG("Local pressure is ");
    DEBUG(calibration_ref_psi, 2);
    DEBUGLN("psi");

    DEBUG("Stability test range: ");
    DEBUGLN(range);
    if (range > STABILITY_CHECK_MAX_DEVIATION) {
        DEBUG("calibration fail: too wide of range: ");
        DEBUG(max);
        DEBUG(" - ");
        DEBUG(min);
        DEBUG(" = ");
        DEBUGLN(range);
        EXIT_FAILURE_FN();
    }

    if (calibration_ref_psi > PSI_ATMOSPHERIC_MIN) {
        DEBUGLN("Done");
    } else {
        DEBUGLN("calibration fail: atmospheric minimum");
        EXIT_FAILURE_FN();
    }
}

void wait_for_zero_input() {
    // called at the end of setup before the main loop
    // requires the user to turn the potentiometer down all the way for a short time
    // + cool little lighting bit
    const uint32_t must_stay_off_for = 200; // ms
    DEBUGLN("To start, turn dial all the way left, then to the right");
    while (get_single_pot_value() >= POT_LOWER_CUTOFF_PERCENT) {
        neopixel_flash_color(COLOR_AWAITING_USER);
    }
    neopixel_show_color(COLOR_NONE_OFF);
    while (true) {
        bool canBreak         = true;
        uint32_t t_zero_start = millis();
        while ((millis() - t_zero_start) < must_stay_off_for) {
            if (get_single_pot_value() >= POT_LOWER_CUTOFF_PERCENT) {
                canBreak = false;
                break;
            }
        }
        if (canBreak) {
            break;
        }
    }
    DEBUGLN("Ready");
    while (get_single_pot_value() < POT_LOWER_CUTOFF_PERCENT) {
        neopixel_show_color(COLOR_AWAITING_USER);
    }
}

void bang_bang_controller() {
    // try to keep variables in mbar!
    static float ambient_mbar = calibration_ref_psi * PSI_TO_MBAR;

    float currentPressure = get_mpr_average(SENSOR_READINGS_PER_CONTROL_LOOP) * PSI_TO_MBAR;
    // similarly to led control, if below a threshold, we disable our output
    if (!userInputValid()) {
        disableSuction();
        return;
    }
    targetPressure       = ambient_mbar - mapfloat(potentiometerValue, POT_LOWER_CUTOFF_PERCENT, 1.0, SUCTION_MIN_MBAR, SUCTION_MAX_MBAR);
    float lowest_allowed = targetPressure - SUCTION_HYSTERESIS;

    if (currentPressure <= lowest_allowed) {
        // target reached!
        disableSuction();
        this_pulldown_achieved_at = millis();
        // uncomment these for immediate tuning feedback
        // DEBUGLN(currentPressure); // pressure in mbar
        // DEBUGLN(calibration_ref_psi * PSI_TO_MBAR - targetPressure); // current suction in mbar
        //
        if (waiting_for_inital_pulldown) {
            waiting_for_inital_pulldown = false;
            first_pulldown_achieved_at  = millis();
        }
    }
    if (currentPressure >= targetPressure) {
        enableSuction();
    }
}

void setup() {
    // ## early setup
    Wire.begin();
    configure_pins();

    Serial.begin(SERIALBAUD);
#ifdef VERSION_DEV
    while (!Serial) {
        yield();
    }
#endif
    delay(SERIAL_ENUMERATION_DELAY);
    DEBUGLN("Serial Connected!");
    
    // ## mid setup
    bool init_neopixel_success = g_neopixel.begin();
    if (!init_neopixel_success) {
        DEBUGLN("INIT FAIL: NEOPIXELS");
        DEBUGLN("This should never happen!");
    }
    bool init_mpr_success = g_mpr25.begin(0x18, &Wire);
    if (!init_mpr_success) {
        DEBUGLN("INIT FAIL: MPR");
        DEBUGLN("The pressure sensor sometimes requires a power or reset cycle.");
    }
    if (!init_neopixel_success || !init_mpr_success) {
        DEBUGLN("One or more inits failed, halting.");
        EXIT_FAILURE_FN();
    }

    // ## late setup
    calibrate_and_check();
    wait_for_zero_input();
}

void loop() {
    led_heartbeat();

    // core functionality
    update_pot_val();
    bang_bang_controller();
    doNeopixelColorBasedOnPot();
    if (millis() > BACKUP_TIMER_MS) {
        EXIT_FAILURE_FN();
    }

    // some stats collection
    if (!waiting_for_inital_pulldown && userInputValid()) {
        if (isSuctionActive) {
            vac_duty_on_post_pulldown++;
        }
        if (!isSuctionActive) {
            vac_duty_off_post_pulldown++;
        }
    }
    periodicStatsPrint();
}