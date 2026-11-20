#ifndef FNAF_DATA_FILES_H
#define FNAF_DATA_FILES_H

#define FNAF_DATA_ROOT "ux0:data/fnaf1/"
#define FNAF_DATA_VERSION "FNAF1-SCENE-DATA-1"
enum { FNAF_DATA_READY = 0, FNAF_DATA_MISSING = 1, FNAF_DATA_INCOMPATIBLE = 2 };
int fnaf_data_check(void);

#endif
