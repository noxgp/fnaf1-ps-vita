#ifndef FNAF_SCENE_ENGINE_H
#define FNAF_SCENE_ENGINE_H
#include <stdint.h>

enum { MAX_INSTANCES = 256, MAX_VARIABLES = 64, MAX_EVENT_GROUPS = 2048 };
enum { EX_CONST, EX_VAR, EX_COUNTER, EX_WIDTH, EX_HEIGHT, EX_X, EX_Y,
       EX_ADD, EX_SUB, EX_MUL, EX_DIV, EX_RANDOM, EX_TIMER, EX_NEG, EX_MIN, EX_MAX };
typedef struct { int op, object, index; float value; } Instruction;
typedef struct { const Instruction *code; int count, valid, comparison; } Expression;
typedef struct { int animation, direction, speed, repeat, back_to, count; const int *images; } AnimationDef;
typedef struct { int handle, width, height, hotspot_x, hotspot_y; } ImageDef;
typedef struct {
    int handle, type, visible, create, image, animation_count, counter;
    const AnimationDef *animations;
    const int *variables;
    int variable_count;
} ObjectDef;
typedef struct { int handle, object, x, y, layer; } InstanceDef;
typedef struct {
    int type, opcode, object, index, flags;
    int p[6];
    const Expression *a, *b;
} EventRecord;
typedef struct {
    const EventRecord *conditions, *actions;
    int condition_count, action_count, context_count;
    const int *contexts;
    int disabled_semantics;
} EventGroup;
typedef struct {
    int index, instance_count, group_count, group_definition_count;
    const InstanceDef *instances;
    const EventGroup *groups;
    const int *group_ids, *group_active;
    const char *name;
} SceneDef;
typedef struct {
    const ObjectDef *definition;
    int handle, layer, visible, create, animation, frame, alpha, counter;
    float x, y, scale_x, scale_y, animation_time;
    float variables[MAX_VARIABLES];
    uint32_t flags;
} Instance;
typedef struct {
    const SceneDef *scene;
    Instance instances[MAX_INSTANCES];
    int count, group_enabled[512], layer_visible[32], tick, pending_scene;
    int unsupported_conditions, unsupported_actions, executed_actions;
    float time_ms, last_event_ms[MAX_EVENT_GROUPS];
    unsigned char previous_match[MAX_EVENT_GROUPS];
    float globals[64];
    uint32_t random_state;
    uint32_t global_flags, reported_conditions[MAX_EVENT_GROUPS], reported_actions[MAX_EVENT_GROUPS];
} Runtime;
extern Runtime runtime;
int scene_open(int original_index);
void scene_tick(float delta_ms);
const ImageDef *scene_image(int handle);
int scene_instance_image(const Instance *instance);
void scene_geometry(const Instance *instance, const ImageDef *image, int *x, int *y, int *width, int *height);
float scene_evaluate(const Expression *expression, int *valid);
Instance *scene_find(int object);
int scene_art(int object,int animation,int frame);
int scene_art_count(int object,int animation);
#endif
