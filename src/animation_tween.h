#ifndef FNAF_ANIMATION_TWEEN_H
#define FNAF_ANIMATION_TWEEN_H
typedef struct {int frame,next;float fraction;} SpriteTween;
static inline SpriteTween sprite_tween(unsigned ms,unsigned frames,unsigned fps){
    unsigned position=ms?(ms-1)*fps:0,last=(frames-1)*1000;
    if(position>last)position=last;
    int frame=(int)(position/1000);
    return (SpriteTween){frame,frame+1<(int)frames?frame+1:frame,(position%1000)/1000.0f};
}
static inline unsigned sprite_reverse(unsigned ms,unsigned frames,unsigned fps){
    unsigned position=ms?(ms-1)*fps:0,last=(frames-1)*1000;
    if(position>last)position=last;
    return 1+(last-position+fps/2)/fps;
}
#endif
