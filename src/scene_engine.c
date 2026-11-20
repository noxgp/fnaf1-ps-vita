/* Data-driven, deliberately partial Fusion event interpreter.
 * Unknown records do not become successful predicates or numeric zeroes.
 */
#include "scene_engine.h"
#include <stddef.h>
#include "generated_scene_data.h"

Runtime runtime;
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int rounded(float value) { return (int)(value < 0 ? value - 0.5f : value + 0.5f); }
void scene_geometry(const Instance *instance, const ImageDef *image, int *x, int *y, int *width, int *height) {
    *x = rounded((instance->x - image->hotspot_x * instance->scale_x) * 0.75f);
    *y = 2 + rounded((instance->y - image->hotspot_y * instance->scale_y) * 0.75f);
    *width = rounded(image->width * instance->scale_x * 0.75f);
    *height = rounded(image->height * instance->scale_y * 0.75f);
}

const ImageDef *scene_image(int handle) {
    int low = 0, high = COUNT(images) - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        if (images[middle].handle == handle) return &images[middle];
        if (images[middle].handle < handle) low = middle + 1;
        else high = middle - 1;
    }
    return NULL;
}

static const ObjectDef *object_definition(int handle) {
    for (int i = 0; i < COUNT(objects); ++i)
        if (objects[i].handle == handle) return &objects[i];
    return NULL;
}
int scene_art_count(int object,int animation) {
    const ObjectDef *o=object_definition(object);if(!o)return 0;
    for(int i=0;i<o->animation_count;++i)if(o->animations[i].animation==animation)return o->animations[i].count;
    return 0;
}
int scene_art(int object,int animation,int frame) {
    const ObjectDef *o=object_definition(object);if(!o)return -1;
    for(int i=0;i<o->animation_count;++i)if(o->animations[i].animation==animation){const AnimationDef *a=&o->animations[i];return a->count?a->images[(frame<0?0:frame)%a->count]:-1;}
    return o->image;
}

Instance *scene_find(int object) {
    if (object >= 32768) return NULL; /* Qualifier selection is not implemented. */
    for (int i = 0; i < runtime.count; ++i)
        if (runtime.instances[i].definition->handle == object && runtime.instances[i].create)
            return &runtime.instances[i];
    return NULL;
}

static const AnimationDef *animation_definition(const Instance *instance) {
    const ObjectDef *object = instance->definition;
    for (int i = 0; i < object->animation_count; ++i)
        if (object->animations[i].animation == instance->animation) return &object->animations[i];
    return object->animation_count ? &object->animations[0] : NULL;
}

int scene_instance_image(const Instance *instance) {
    const AnimationDef *animation = animation_definition(instance);
    if (!animation || !animation->count) return instance->definition->image;
    int frame = instance->frame;
    if (frame < 0 || frame >= animation->count) frame = 0;
    return animation->images[frame];
}

float scene_evaluate(const Expression *expression, int *valid) {
    float stack[64];
    int used = 0;
    if (!expression || !expression->valid) { *valid = 0; return 0; }
    for (int i = 0; i < expression->count; ++i) {
        const Instruction *instruction = &expression->code[i];
        Instance *instance = NULL;
        float value = 0;
        if (instruction->op >= EX_VAR && instruction->op <= EX_Y && instruction->object >= 0) {
            instance = scene_find(instruction->object);
            if (!instance) { *valid = 0; return 0; }
        }
        switch (instruction->op) {
        case EX_CONST: value = instruction->value; break;
        case EX_VAR:
            if (instruction->index < 0 || instruction->index >= MAX_VARIABLES) { *valid = 0; return 0; }
            value = instruction->object == -1 ? runtime.globals[instruction->index] : instance->variables[instruction->index];
            break;
        case EX_COUNTER: value = (float)instance->counter; break;
        case EX_X: value = instance->x; break;
        case EX_Y: value = instance->y; break;
        case EX_WIDTH: case EX_HEIGHT: {
            const ImageDef *image = scene_image(scene_instance_image(instance));
            if (!image) { *valid = 0; return 0; }
            value = instruction->op == EX_WIDTH ? image->width * instance->scale_x : image->height * instance->scale_y;
            break;
        }
        case EX_TIMER: value = runtime.time_ms; break;
        case EX_RANDOM:
            if (!used || stack[used - 1] < 1 || stack[used - 1] > 2147483000.0f) { *valid = 0; return 0; }
            runtime.random_state = runtime.random_state * 1664525u + 1013904223u;
            stack[used - 1] = (float)((runtime.random_state >> 1) % (uint32_t)stack[used - 1]);
            continue;
        case EX_NEG:
            if (!used) { *valid = 0; return 0; }
            stack[used - 1] = -stack[used - 1]; continue;
        case EX_ADD: case EX_SUB: case EX_MUL: case EX_DIV: case EX_MIN: case EX_MAX: {
            if (used < 2) { *valid = 0; return 0; }
            float right = stack[--used], left = stack[used - 1];
            if (instruction->op == EX_DIV && right == 0) { *valid = 0; return 0; }
            if (instruction->op == EX_ADD) value = left + right;
            else if (instruction->op == EX_SUB) value = left - right;
            else if (instruction->op == EX_MUL) value = left * right;
            else if (instruction->op == EX_MIN) value = left < right ? left : right;
            else if (instruction->op == EX_MAX) value = left > right ? left : right;
            else value = left / right;
            stack[used - 1] = value; continue;
        }
        default: *valid = 0; return 0;
        }
        if (used >= 64) { *valid = 0; return 0; }
        stack[used++] = value;
    }
    if (used != 1) { *valid = 0; return 0; }
    return stack[0];
}

static int compare(float left, float right, int comparison) {
    switch (comparison) {
    case 0: return left == right;
    case 1: return left != right;
    case 2: return left <= right;
    case 3: return left < right;
    case 4: return left >= right;
    case 5: return left > right;
    default: return 0;
    }
}

static int condition(const EventRecord *record, int group, int *known) {
    int type = record->type, opcode = record->opcode, valid = 1;
    Instance *instance = NULL;
    if (type >= 2) instance = scene_find(record->object);
    if (type == -3 && opcode == -1) return runtime.tick == 0;
    if (type == -1 && opcode == -1) return 1;
    if (type == -1 && (opcode == -6 || opcode == -7)) return 1; /* Edge handled at group level. */
    if (type == -1 && opcode == -16 && record->index < 32)
        return (runtime.global_flags & (1u << record->index)) != 0;
    if (type == -4 && opcode == -8 && record->p[0] > 0) {
        float period = (float)record->p[0];
        return (int)(runtime.time_ms / period) > (int)(runtime.last_event_ms[group] / period);
    }
    if (type == -1 && opcode >= -33 && opcode <= -28 && record->index < 64) {
        float value = scene_evaluate(record->a, &valid);
        if (!valid) { *known = 0; return 0; }
        return compare(runtime.globals[record->index], value, record->a->comparison);
    }
    if (type >= 2 && (opcode == -42 || opcode == -27 || opcode == -43) && record->index < MAX_VARIABLES) {
        if (!instance) return 0;
        float value = scene_evaluate(record->a, &valid);
        if (!valid) { *known = 0; return 0; }
        return compare(instance->variables[record->index], value, record->a->comparison);
    }
    if (type == 7 && opcode == -81) {
        if (!instance) return 0;
        float value = scene_evaluate(record->a, &valid);
        if (!valid) { *known = 0; return 0; }
        return compare((float)instance->counter, value, record->a->comparison);
    }
    if (type >= 2 && (opcode == -24 || opcode == -25) && record->index < 32) {
        if (!instance) return 0;
        int index = record->a ? (int)scene_evaluate(record->a, &valid) : record->index;
        if (!valid || index < 0 || index >= 32) { *known = 0; return 0; }
        int flag = (instance->flags & (1u << index)) != 0;
        return opcode == -25 ? flag : !flag;
    }
    *known = 0;
    return 0;
}

static int action(const EventRecord *record) {
    int type = record->type, opcode = record->opcode, valid = 1;
    float a = 0;
    if (record->a) a = scene_evaluate(record->a, &valid);
    if (record->b) (void)scene_evaluate(record->b, &valid);
    if (!valid) return 0;
    if (type == -1) {
        if (opcode == 0) return 1;
        if (opcode == 6 || opcode == 7) {
            if (record->p[0] < 0 || record->p[0] >= 512) return 0;
            runtime.group_enabled[record->p[0]] = opcode == 6;
            return 1;
        }
        if (opcode == 14 && record->index < 32) {
            if (a != 0) runtime.global_flags |= 1u << record->index;
            else runtime.global_flags &= ~(1u << record->index);
            return 1;
        }
        if (record->index >= 64) return 0;
        if (opcode == 3 || (opcode >= 27 && opcode <= 30)) runtime.globals[record->index] = a;
        else if (opcode == 4 || (opcode >= 32 && opcode <= 35)) runtime.globals[record->index] += a;
        else if (opcode == 5 || opcode == 31 || (opcode >= 36 && opcode <= 38)) runtime.globals[record->index] -= a;
        else return 0;
        return 1;
    }
    if (type == -3 && opcode == 2) {
        /* Record transition requests, but never enter unimplemented gameplay. */
        runtime.pending_scene = record->p[0];
        return 1;
    }
    if (type == 44 && (opcode == 111 || opcode == 112)) {
        int layer = (int)a - 1;
        if (layer < 0 || layer >= 32) return 0;
        runtime.layer_visible[layer] = opcode == 111;
        return 1;
    }
    if (type < 2 || record->object >= 32768) return 0;
    int matched = 0;
    for (int i = 0; i < runtime.count; ++i) {
        Instance *instance = &runtime.instances[i];
        if (instance->definition->handle != record->object || !instance->create) continue;
        matched = 1;
        switch (opcode) {
        case 1:
            if (record->p[0] != 65535) return 0;
            instance->x = (float)record->p[2]; instance->y = (float)record->p[3];
            if (record->p[4] > 0) instance->layer = record->p[4] - 1;
            break;
        case 2: instance->x = a; break;
        case 3: instance->y = a; break;
        case 17:
            if (instance->animation != record->index) {
                instance->animation = record->index; instance->frame = 0; instance->animation_time = 0;
            }
            break;
        case 26: instance->visible = 0; break;
        case 27: instance->visible = 1; break;
        case 31: case 32: case 33:
            if (record->index >= MAX_VARIABLES) return 0;
            if (opcode == 31) instance->variables[record->index] = a;
            if (opcode == 32) instance->variables[record->index] += a;
            if (opcode == 33) instance->variables[record->index] -= a;
            break;
        case 35: case 36:
            if (record->index >= 32) return 0;
            if (opcode == 36) instance->flags |= 1u << record->index;
            else instance->flags &= ~(1u << record->index);
            break;
        case 65: instance->alpha = a < 0 ? 0 : a > 255 ? 255 : (int)a; break;
        case 80: case 81: case 82:
            if (type != 7) return 0;
            if (opcode == 80) instance->counter = (int)a;
            if (opcode == 81) instance->counter += (int)a;
            if (opcode == 82) instance->counter -= (int)a;
            break;
        case 85: case 86: case 87:
            if (a <= 0 || a > 256) return 0;
            if (opcode != 87) instance->scale_x = a;
            if (opcode != 86) instance->scale_y = a;
            break;
        default: return 0;
        }
    }
    return matched;
}

int scene_open(int original_index) {
    const SceneDef *scene = NULL;
    for (int i = 0; i < COUNT(scenes); ++i) if (scenes[i].index == original_index) scene = &scenes[i];
    if (!scene || scene->instance_count > MAX_INSTANCES || scene->group_count > MAX_EVENT_GROUPS) return -1;
    volatile unsigned char *bytes = (volatile unsigned char *)&runtime;
    for (unsigned i = 0; i < sizeof(runtime); ++i) bytes[i] = 0;
    runtime.scene = scene;
    runtime.count = scene->instance_count;
    runtime.pending_scene = -1;
    runtime.random_state = 0x12345678u;
    for (int i = 0; i < COUNT(initial_globals) && i < 64; ++i) runtime.globals[i] = initial_globals[i];
    for (int i = 0; i < 32; ++i) runtime.layer_visible[i] = 1;
    for (int i = 0; i < scene->group_definition_count; ++i)
        if (scene->group_ids[i] >= 0 && scene->group_ids[i] < 512)
            runtime.group_enabled[scene->group_ids[i]] = scene->group_active[i];
    for (int i = 0; i < runtime.count; ++i) {
        const InstanceDef *source = &scene->instances[i];
        const ObjectDef *definition = object_definition(source->object);
        if (!definition) return -2;
        Instance *instance = &runtime.instances[i];
        instance->definition = definition;
        instance->handle = source->handle; instance->layer = source->layer;
        instance->x = (float)source->x; instance->y = (float)source->y;
        instance->scale_x = instance->scale_y = 1;
        instance->visible = definition->visible; instance->create = definition->create;
        instance->counter = definition->counter;
        for (int j = 0; j < definition->variable_count && j < MAX_VARIABLES; ++j)
            instance->variables[j] = (float)definition->variables[j];
    }
    return 0;
}

void scene_tick(float delta_ms) {
    if (!runtime.scene || delta_ms < 0 || delta_ms > 1000) return;
    runtime.time_ms += delta_ms;
    for (int g = 0; g < runtime.scene->group_count; ++g) {
        const EventGroup *group = &runtime.scene->groups[g];
        int active = 1;
        for (int c = 0; c < group->context_count; ++c) {
            int identity = group->contexts[c];
            if (identity < 0 || identity >= 512 || !runtime.group_enabled[identity]) active = 0;
        }
        if (!active) continue;
        if (group->disabled_semantics) {
            if (!(runtime.reported_conditions[g] & 0x80000000u)) {
                ++runtime.unsupported_conditions; runtime.reported_conditions[g] |= 0x80000000u;
            }
            continue;
        }
        int match = 1, edge = 0;
        for (int c = 0; c < group->condition_count; ++c) {
            int known = 1;
            const EventRecord *record = &group->conditions[c];
            if (record->type == -1 && (record->opcode == -6 || record->opcode == -7)) edge = 1;
            if (!condition(record, g, &known)) match = 0;
            if (!known && c < 31 && !(runtime.reported_conditions[g] & (1u << c))) {
                ++runtime.unsupported_conditions; runtime.reported_conditions[g] |= 1u << c;
            }
        }
        int execute = match && (!edge || !runtime.previous_match[g]);
        runtime.previous_match[g] = (unsigned char)match;
        if (execute) {
            for (int a = 0; a < group->action_count; ++a) {
                if (action(&group->actions[a])) ++runtime.executed_actions;
                else if (a < 32 && !(runtime.reported_actions[g] & (1u << a))) {
                    ++runtime.unsupported_actions; runtime.reported_actions[g] |= 1u << a;
                }
            }
        }
        runtime.last_event_ms[g] = runtime.time_ms;
    }
    for (int i = 0; i < runtime.count; ++i) {
        Instance *instance = &runtime.instances[i];
        const AnimationDef *animation = animation_definition(instance);
        if (!animation || animation->count < 2 || !instance->create) continue;
        instance->animation_time += delta_ms * (float)animation->speed / 2000.0f;
        while (instance->animation_time >= 1) {
            instance->animation_time -= 1;
            ++instance->frame;
            if (instance->frame >= animation->count) instance->frame = animation->repeat == 0 ? animation->back_to : animation->count - 1;
        }
    }
    ++runtime.tick;
}
