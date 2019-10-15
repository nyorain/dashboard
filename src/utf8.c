#include <stdlib.h>

// Taken from glib's gutf8.c.
// Does not validate characters.
static const size_t utf8_skip_data[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

unsigned utf8_length(const char* src) {
	return utf8_skip_data[*((unsigned char *)src)];
}

char* strlcpy_utf8(char* dst, const char* src, size_t maxncpy) {
    char *dst_r = dst;
    size_t utf8_size;
    if(maxncpy > 0) {
        while(*src != '\0' && (utf8_size = utf8_length(src)) < maxncpy) {
            maxncpy -= utf8_size;
            switch (utf8_size) {
                case 6: *dst ++ = *src++; // fallthrough
                case 5: *dst ++ = *src++; // fallthrough
                case 4: *dst ++ = *src++; // fallthrough
                case 3: *dst ++ = *src++; // fallthrough
                case 2: *dst ++ = *src++; // fallthrough
                case 1: *dst ++ = *src++; // fallthrough
            }
        }
        *dst= '\0';
    }

    return dst_r;
}
