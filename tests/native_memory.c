/* Test-only replacements for Vita OS memory imports. */
#include <stddef.h>
void *memcpy(void *destination, const void *source, size_t count) {
    volatile unsigned char *out = (volatile unsigned char *)destination;
    const volatile unsigned char *in = (const volatile unsigned char *)source;
    for (size_t i = 0; i < count; ++i) out[i] = in[i];
    return destination;
}
void *memset(void *destination, int value, size_t count) {
    volatile unsigned char *out = (volatile unsigned char *)destination;
    for (size_t i = 0; i < count; ++i) out[i] = (unsigned char)value;
    return destination;
}
int strcmp(const char *left, const char *right) {
    while (*left && *left == *right) { ++left; ++right; }
    return (unsigned char)*left - (unsigned char)*right;
}
int memcmp(const void *left,const void *right,size_t count){
    const volatile unsigned char *a=left,*b=right;
    for(size_t i=0;i<count;++i)if(a[i]!=b[i])return (int)a[i]-(int)b[i];
    return 0;
}
