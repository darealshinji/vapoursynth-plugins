#pragma once

#include "constants.h"
#include <f3kdb.h>

#define DEFAULT_RANDOM_PARAM 1.0

// returns a random number in [-range, range]
int random(RANDOM_ALGORITHM algo, int& seed, int range, double param);