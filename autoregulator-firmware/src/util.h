#pragma once

// --- Pure utility functions ---

// like map() but for floats.
// if in_max == in_min, the operation is invalid, returns 0
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max);