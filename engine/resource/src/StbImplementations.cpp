// Single home for the stb implementations. Defining these in a header-using
// translation unit twice is a duplicate-symbol link error waiting to happen.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
