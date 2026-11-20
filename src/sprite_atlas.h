#ifndef FNAF_SPRITE_ATLAS_H
#define FNAF_SPRITE_ATLAS_H
typedef struct {int handle,page,x,y,width,height;} SpriteAtlasItem;
typedef struct {int handle,width,height;} SpriteAtlasPage;
#include "generated_sprite_atlas.h"
static inline const SpriteAtlasItem *sprite_atlas_item(int handle){
    int lo=0,hi=(int)(sizeof(sprite_atlas_items)/sizeof(*sprite_atlas_items))-1;
    while(lo<=hi){int mid=(lo+hi)/2;const SpriteAtlasItem *p=&sprite_atlas_items[mid];
        if(p->handle==handle)return p;
        if(p->handle<handle)lo=mid+1;else hi=mid-1;
    }return 0;
}
static inline int sprite_texture_handle(int handle){const SpriteAtlasItem *p=sprite_atlas_item(handle);return p?p->page:handle;}
static inline const SpriteAtlasPage *sprite_atlas_page(int handle){
    for(unsigned i=0;i<sizeof(sprite_atlas_pages)/sizeof(*sprite_atlas_pages);++i)if(sprite_atlas_pages[i].handle==handle)return &sprite_atlas_pages[i];
    return 0;
}
#endif
