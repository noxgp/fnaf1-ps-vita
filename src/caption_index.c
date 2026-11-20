#include "caption_index.h"
#include <string.h>
static unsigned le16(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned le32(const unsigned char *p){return le16(p)|le16(p+2)<<16;}
int caption_index_decode(const unsigned char *b,size_t size,CaptionEntry *out,unsigned count){
    if(size<12||memcmp(b,"FCIX",4)||le32(b+4)!=1||le32(b+8)!=count||count>4096||size!=12u+count*12u)return -1;
    for(unsigned i=0;i<count;++i){const unsigned char *p=b+12+i*12;CaptionEntry c={le32(p),le32(p+4),(unsigned short)le16(p+8),(unsigned short)le16(p+10)};
        if(!c.width||c.width>1024||!c.height||c.height>512||!c.size||c.size>4*1024*1024||c.offset>64*1024*1024-c.size)return -1;
        out[i]=c;
    }return 0;
}
