#include "stdafx.h"

#include "random.h"

#include <math.h>

#include <assert.h>

#include <stdint.h>

typedef double (*rand_impl_t)(int& seed, double param);

double rand_old(int& seed, double param);

double rand_uniform(int& seed, double param);

double rand_gaussian(int& seed, double param);

static const rand_impl_t rand_algorithms[] = {
    rand_old,
    rand_uniform,
    rand_gaussian
};

inline double round(double r) {
    return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}

int random(RANDOM_ALGORITHM algo, int& seed, int range, double param)
{
    assert(algo >= 0 && algo < RANDOM_ALGORITHM_COUNT);

    double num = rand_algorithms[algo](seed, param);
    assert(num >= -1.0 && num <= 1.0);
    return (int)round(num * range);
}

// most algorithms below are stolen from AddGrainC

double rand_to_double(int rand_num)
{
    // convert the number to 52 bit, use high 12 bits to fill lower space 
    // (otherwise the upper bound will be significantly less than 1.0)
    union
    {
        uint64_t itemp;
        double result;
    };
    itemp = ((uint64_t)rand_num) & 0xffffffffULL;
    itemp = itemp << 20 | itemp >> 12;

    // fill exponent with 1
    itemp |= 0x3ff0000000000000ULL;

    // itemp is now in [1.0, 2.0), convert to [-1.0, 1.0)
    return (result - 1.0) * 2 - 1.0;
}

double rand_old(int& seed, double)
{
    int seed_tmp = (((seed << 13) ^ (unsigned int)seed) >> 17) ^ (seed << 13) ^ seed;
    seed = 32 * seed_tmp ^ seed_tmp;
    return rand_to_double(seed);
}

double rand_uniform(int& seed, double)
{
    seed = 1664525 * seed + 1013904223;
    return rand_to_double(seed);
}

// http://www.bearcave.com/misl/misl_tech/wavelets/hurst/random.html
double rand_gaussian(int& seed, double param)
{
    double ret;
    double x, y, r2;

    do
    {
        do
        {
            /* choose x,y in uniform square (-1,-1) to (+1,+1) */

            x = rand_uniform (seed, param);
            y = rand_uniform (seed, param);

            /* see if it is in the unit circle */
            r2 = x * x + y * y;
        }
        while (r2 > 1.0 || r2 == 0);
        /* Box-Muller transform */

        // sigma = param
        ret = param * y * sqrt (-2.0 * log (r2) / r2);

    } while (ret <= -1.0 || ret >= 1.0);
    // we need to clip the result because the wrapper accepts [-1.0, 1.0] only

    return ret;
}
