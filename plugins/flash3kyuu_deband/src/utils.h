
static inline int clamp_pixel(int pixel, int min, int max)
{
    if (pixel > max) {
        pixel = max;
    } else if (pixel < min) {
        pixel = min;
    }
    return pixel;
}

