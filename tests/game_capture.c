#include "game.h"
#include "game_view.h"
#include "android_menu.h"
#include "game_events.h"
#include "scare_animation.h"
#include <stdint.h>
#include <string.h>
int capture_commands[96][7],capture_count;
unsigned char capture_demanded[8192];
unsigned char capture_warmed[8192];int capture_focus_handle=-1;
void gpu_queue_reset(int display){(void)display;capture_count=0;capture_focus_handle=-1;memset(capture_demanded,0,sizeof(capture_demanded));memset(capture_warmed,0,sizeof(capture_warmed));}
void gpu_queue_image(int handle,float x,float y,float w,float h,unsigned color,int perspective){(void)perspective;if(capture_count>=96)return;if(handle>=0&&handle<8192)capture_demanded[handle]=1;int *p=capture_commands[capture_count++];p[0]=handle;p[1]=(int)x;p[2]=(int)y;p[3]=(int)w;p[4]=(int)h;p[5]=(int)color;p[6]=perspective;}
int capture_missing_handle=-1,capture_resident_count;
void gpu_queue_prefetch(int handle){(void)handle;}
void gpu_queue_warm(int handle){if(handle>=0&&handle<8192)capture_warmed[handle]=1;}
void gpu_queue_resident(int handle){if(handle>=0&&handle<8192)capture_demanded[handle]=1;++capture_resident_count;}
void gpu_queue_focus(int handle){capture_focus_handle=handle;if(handle>=0&&handle<8192)capture_demanded[handle]=1;}
void gpu_queue_prepare(void){}
int gpu_texture_ready(int handle){return handle!=capture_missing_handle;}
int gpu_texture_dimensions(int handle,unsigned *width,unsigned *height){(void)handle;(void)width;(void)height;return 0;}
int run_checks(void){extern int run_android_checks(void),run_presentation_checks(void);int result=run_android_checks();return result?result:run_presentation_checks();}
int capture_case(int which){Game g;GameSave s;game_defaults(&s);game_init(&g,&s,12345);game_start(&g,1);game_input(&g,IN_OK,0,0);
    if(which==0)g.phase=G_TITLE;
    if(which==2){g.pan=0;g.light[0]=1;g.position[1]=99;}
    if(which==3){g.pan=320;g.light[1]=1;g.position[2]=100;}
    if(which>=4&&which<=14){g.monitor=1;g.camera=which-4;}
    if(which>=15&&which<=18){g.phase=G_SCARE;g.killer=which-15;g.phase_ms=400;}
    if(which==19){g.phase=G_NEWSPAPER;g.save.options.language=8;g.phase_ms=2000;}
    if(which==20){g.monitor=1;g.save.options.language=10;}
    g.phase_ms=1200;
    if(which==21)g.phase=G_WARNING;
    if(which==22||which==23){g.phase=G_INTRO;g.night=which==22?1:6;}
    if(which==24)g.phase=G_OPTIONS;
    if(which==25)g.phase=G_AUDIO_OPTIONS;
    if(which==26){g.phase=G_EXTRAS;g.save.options.radar=1;}
    if(which==27){g.phase=G_EXTRAS;g.selection=4;g.save.options.plush[1]=1;}
    if(which==28){g.phase=G_CUSTOM;g.save.custom[0]=20;}
    if(which==29){g.phase=G_TITLE;g.save.unlocked=7;g.save.next_night=5;g.save.stars=7;g.selection=2;}
    if(which==30){g.phase=G_CONFIRM_NEW;g.save.next_night=5;}
    if(which==31){g.monitor=1;g.camera_scan=320000;}
    if(which==32){g.monitor=1;g.camera=5;g.camera_scan=320000;}
    if(which==33){g.phase=G_WIN;g.phase_ms=2000;}
    if(which==34){g.phase=G_WIN;g.phase_ms=3500;}
    if(which==35){g.phase=G_WIN;g.phase_ms=5000;}
    if(which==36)g.phase=G_DEAD;
    if(which==37){g.monitor=1;g.save.options.language=8;}
    if(which==38){g.monitor=1;g.camera=2;g.save.options.language=8;}
    if(which>=39&&which<=42){g.monitor=1;g.camera=3;g.camera_flicker=which==39||which==42?10:1;if(which>=41)g.position[1]=77;}
    if(which>=43&&which<=45){g.monitor=1;g.camera=4;g.night=4;g.position[1]=78;g.camera_twitch=which==43?24:which==44?25:29;}
    if(which>=46&&which<=48){g.monitor=1;g.camera=7;g.night=4;g.position[2]=81;g.camera_twitch=which==46?24:which==47?25:29;}
    if(which==49)game_set_date(&g,10,31);
    if(which==50){game_set_date(&g,10,31);g.phase=G_OUTAGE;g.outage_stage=0;}
    if(which==51){g.golden=2;g.golden_ms=400;}
    if(which>=52&&which<=55){g.hallucination_left=500;g.hallucination_roll=1;g.elapsed=(which-52)*23;}
    if(which==56||which==57){g.phase=G_CREEPY_START;g.phase_ms=which==56?9000:9500;}
    if(which==58)g.phase=G_CREEPY_END;
    if(which==59){g.monitor=1;g.camera=4;g.golden=1;}
    if(which==60||which==61||which==62){for(int i=0;i<4;++i)g.save.options.plush[i]=1;if(which==61)g.pan=0;if(which==62)g.phase=G_OUTAGE;}
    if(which==63){g.phase=G_OUTAGE;g.outage_stage=1;}
    if(which==64){g.phase=G_OUTAGE;g.outage_stage=2;}
    if(which>=65&&which<=69){g.phase=G_SCARE;g.killer=SCARE_POWER_OUT;const unsigned times[]={0,100,200,350,560};g.phase_ms=times[which-65];}
    if(which==70){g.phase=G_SCARE;g.killer=SCARE_POWER_OUT;g.phase_ms=0;capture_missing_handle=325;}
    if(which==71){g.phase=G_SCARE;g.killer=0;g.phase_ms=200;}
    if(which>=72&&which<=75){g.monitor=which!=75;const unsigned times[]={1,151,251,151};g.monitor_ms=times[which-72];}
    if(which==76||which==77){g.monitor=1;g.camera=which==76?0:10;g.elapsed=600;}
    if(which==78){g.monitor=1;g.camera=3;g.position[1]=77;game_set_date(&g,10,31);}
    if(which==79){for(int i=0;i<4;++i)g.save.options.plush[i]=1;game_set_date(&g,10,31);g.light[0]=1;g.position[1]=99;}
    if(which>=80&&which<=86){for(int i=0;i<4;++i)g.save.options.plush[i]=1;game_set_date(&g,10,31);}
    if(which==80){g.monitor=1;g.camera=3;g.position[1]=77;}
    if(which==81){g.monitor=1;g.camera=4;g.night=4;g.position[1]=78;}
    if(which==82){g.monitor=0;g.usage=3;g.power=432;g.hour=5;}
    if(which==83||which==84||which==85){g.phase=G_OUTAGE;g.pan=0;g.outage_stage=which==84?1:0;}
    if(which==85){g.phase=G_PAUSE;g.return_phase=G_OUTAGE;}
    if(which==86){g.power=0;g.monitor=1;g.pan=320;game_tick(&g,20);}
    if(which>=87&&which<=90){g.door[0]=g.door[1]=1;const unsigned times[]={76,226,426,626};g.door_ms[0]=g.door_ms[1]=times[which-87];}
    if(which==91){g.monitor=1;g.camera=3;g.position[1]=77;g.camera_flicker=10;capture_missing_handle=37;}
    game_view_prepare(&g,2000,22000);capture_missing_handle=-1;return capture_count;
}
