#include "util.h"


float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
    float run   = in_max - in_min;
    if (run == 0.0f) {
        return 0.0f;
    }
    float rise  = out_max - out_min;
    float delta = x - in_min;
    return (delta * rise) / run + out_min;
}