#ifndef FNAF_CAMERA_MAP_H
#define FNAF_CAMERA_MAP_H
typedef struct { int object,view,x,y,label; } CameraInfo;
extern const CameraInfo camera_map[11];
int camera_map_move(int current,int dx,int dy);
int camera_map_hit(int x,int y);
#endif
