/* Remaining room events: hallucinations, poster encounter and independent
 * ambience channels. The source event/group provenance is documented in docs. */
#include "game_events.h"
unsigned game_event_random(Game *g,unsigned n){
    if(!g->event_rng)g->event_rng=0x6d2b79f5u;
    g->event_rng^=g->event_rng<<13;g->event_rng^=g->event_rng>>17;g->event_rng^=g->event_rng<<5;return n?g->event_rng%n:0;
}
void game_set_date(Game *g,int month,int day){g->halloween=month==10&&day==31;}
void game_sound_volume(Game *g,int channel,int volume){
    if(channel<0||channel>=AUDIO_VOICES)return;
    if(g->event_volumes[channel]==volume+1)return;
    g->event_volumes[channel]=volume+1;game_sound(g,-2,channel,0,volume);
}
void game_hallucinate(Game *g){g->hallucination_left=1667;}
static int near_office(int pos){return (pos>=77&&pos<=81)||pos==99||pos==100;}
void game_events_levels(Game *g){
    int threats=near_office(g->position[1])+near_office(g->position[2])+(g->fox>=2);
    const int danger[]={0,30,50,75};
    game_sound_volume(g,A_DANGER,g->position[0]==122?100:danger[threats]);
    game_sound_volume(g,A_PIRATE,g->monitor&&game_camera_view(g)==99?15:5);
    game_sound_volume(g,2,g->monitor?50:100);
    game_sound_volume(g,A_KITCHEN,g->position[2]!=83?0:!g->monitor?10:game_camera_view(g)==6?75:20);
    game_sound_volume(g,A_FREDDY_MUSIC,g->position[0]!=83?0:g->monitor&&game_camera_view(g)==6?50:5);
    game_sound_volume(g,A_CAMERA,g->monitor?100:0);
    game_sound_volume(g,A_HALLUCINATION,g->hallucination_left?100:g->murmur_volume);
}
void game_events_start(Game *g){
    game_sound(g,37,A_DANGER,1,0);game_sound(g,40,A_HALLUCINATION,1,0);
    g->last_freddy_position=g->position[0];game_events_levels(g);
}
static void intruder(Game *g,int side){
    if(g->monitor&&g->jam[side]&&game_event_random(g,3)==0)game_sound(g,22+(int)game_event_random(g,4),A_INTRUDER,0,50);
}
void game_events_tick(Game *g,unsigned ms){
    if(g->phase!=G_PLAY)return;
    if(g->hallucination_left>ms)g->hallucination_left-=ms;else g->hallucination_left=0;
    /* Visibility roll is sampled by the 60 Hz room visual clock. */
    if(g->golden==1&&g->monitor&&game_camera_view(g)==22&&g->position[1]!=78){
        g->golden=2;game_sound(g,38,A_GOLDEN,0,100);
    }
    if(g->golden==2&&!g->monitor&&!g->golden_ms)game_hallucinate(g);
    if(g->last_freddy_position!=g->position[0]){
        if(g->position[0]==83){game_sound(g,30,A_FREDDY_MUSIC,0,5);g->freddy_music_ms=0;}
        if(g->position[0]==122)game_sound(g,59,A_INSIDE,0,100);
        g->last_freddy_position=g->position[0];
    }
    g->event_second_ms+=ms;
    while(g->event_second_ms>=1000){g->event_second_ms-=1000;
        if(game_event_random(g,1000)==1)game_hallucinate(g);
        if(!g->golden&&game_event_random(g,100000)==1)g->golden=1;
    }
    g->event_four_ms+=ms;
    while(g->event_four_ms>=4000){g->event_four_ms-=4000;
        if(g->fox==0&&game_event_random(g,30)==0)game_sound(g,21,A_PIRATE,0,g->monitor&&game_camera_view(g)==99?15:5);
        if(g->position[2]==83){unsigned roll=game_event_random(g,10)+1;
            if(roll<=5){const int sounds[]={16,17,18,18,19};game_sound(g,sounds[roll-1],A_KITCHEN,0,g->monitor&&game_camera_view(g)==6?75:g->monitor?20:10);}}
    }
    g->event_five_ms+=ms;
    while(g->event_five_ms>=5000){g->event_five_ms-=5000;
        if(game_event_random(g,30)==0)game_sound(g,29,A_CREEK,0,100);
        intruder(g,0);intruder(g,1);
    }
    g->event_ten_ms+=ms;
    while(g->event_ten_ms>=10000){g->event_ten_ms-=10000;
        if(game_event_random(g,50)==0)game_sound(g,39,A_DRONE,0,10+(int)game_event_random(g,40));
    }
    g->murmur_ms+=ms;
    while(g->murmur_ms>=100){g->murmur_ms-=100;g->murmur_volume=0;
        if(g->night>=4&&(g->position[1]==78||g->position[2]==81)){
            int watching=g->monitor&&((g->position[1]==78&&game_camera_view(g)==22)||(g->position[2]==81&&game_camera_view(g)==42));
            g->murmur_volume=1+(int)game_event_random(g,5)*(watching?20:5);
        }
    }
    if(g->position[0]==83){g->freddy_music_ms+=ms;if(g->freddy_music_ms>=300000){g->freddy_music_ms-=300000;game_sound(g,30,A_FREDDY_MUSIC,0,5);}}
    game_events_levels(g);
}
