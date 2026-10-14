#include "common.h"

double clamp(double value, double min, double max) {
    return value > max ? max : value < min ? min : value; 
}