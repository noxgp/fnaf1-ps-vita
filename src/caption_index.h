#ifndef FNAF_CAPTION_INDEX_H
#define FNAF_CAPTION_INDEX_H
#include <stddef.h>
typedef struct {unsigned offset,size;unsigned short width,height;} CaptionEntry;
int caption_index_decode(const unsigned char *bytes,size_t size,CaptionEntry *out,unsigned count);
#endif
