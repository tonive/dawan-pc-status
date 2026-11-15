#include <stddef.h>

void *memset(void *destination, int value, size_t count) {
    unsigned char *bytes = destination;
    while (count--) *bytes++ = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count) {
    unsigned char *out = destination;
    const unsigned char *in = source;
    while (count--) *out++ = *in++;
    return destination;
}
