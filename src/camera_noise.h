#ifndef FNAF_CAMERA_NOISE_H
#define FNAF_CAMERA_NOISE_H
#include "generated_camera_noise.h"
static inline int camera_noise_image(int handle){
    for(unsigned i=0;i<sizeof(camera_noise_images)/sizeof(*camera_noise_images);++i)if(camera_noise_images[i]==handle)return 1;
    return 0;
}
static inline unsigned texture_pixel_bytes(int handle){return camera_noise_image(handle)?1u:4u;}
#endif
