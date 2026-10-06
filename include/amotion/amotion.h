#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct am_world am_world;

typedef enum am_motion_policy {
    AM_MOTION_FULL = 0,
    AM_MOTION_REDUCED = 1,
    AM_MOTION_OFF = 2
} am_motion_policy;

typedef enum am_surface_contract {
    AM_SURFACE_FREE = 0,
    AM_SURFACE_SOURCE = 1,
    AM_SURFACE_DOCK = 2,
    AM_SURFACE_EDGE = 3,
    AM_SURFACE_CORNER = 4
} am_surface_contract;

typedef enum am_pointer_phase {
    AM_POINTER_ENTER = 0,
    AM_POINTER_LEAVE = 1,
    AM_POINTER_DOWN = 2,
    AM_POINTER_MOVE = 3,
    AM_POINTER_UP = 4,
    AM_POINTER_CANCEL = 5
} am_pointer_phase;

typedef enum am_event_type {
    AM_EVENT_NONE = 0,
    AM_EVENT_CLICK = 1,
    AM_EVENT_DRAG_START = 2,
    AM_EVENT_DRAG_END = 3,
    AM_EVENT_SURFACE_PREVIEW = 4,
    AM_EVENT_COMMIT = 5
} am_event_type;

enum {
    AM_FRAME_HOVERED = 1u << 0,
    AM_FRAME_AFFORDANCE = 1u << 1,
    AM_FRAME_PRESSED = 1u << 2,
    AM_FRAME_DRAGGING = 1u << 3,
    AM_FRAME_SETTLING = 1u << 4,
    AM_FRAME_COMMITTED = 1u << 5
};

typedef struct am_vec2 {
    double x;
    double y;
} am_vec2;

typedef struct am_rect {
    double x;
    double y;
    double width;
    double height;
} am_rect;

typedef struct am_world_config {
    am_motion_policy policy;
    double hover_dwell_ms;
    double drag_threshold_px;
    double follow_hz;
    double settle_hz;
    double max_step_ms;
} am_world_config;

typedef struct am_body_desc {
    uint64_t id;
    am_rect rect;
    double min_scale;
    double max_scale;
} am_body_desc;

typedef struct am_surface_desc {
    uint64_t id;
    am_surface_contract contract;
    am_rect rect;
    double magnet_px;
    int32_t priority;
} am_surface_desc;

typedef struct am_pointer_event {
    am_pointer_phase phase;
    uint32_t pointer_id;
    double x;
    double y;
    uint32_t buttons;
    double time_ms;
} am_pointer_event;

typedef struct am_body_frame {
    uint64_t id;
    am_rect rect;
    double scale_x;
    double scale_y;
    double rotation_deg;
    double opacity;
    double depth;
    uint32_t flags;
    uint64_t surface_id;
    am_surface_contract surface_contract;
} am_body_frame;

typedef struct am_event {
    am_event_type type;
    uint64_t body_id;
    uint64_t surface_id;
    am_surface_contract surface_contract;
    am_rect rect;
} am_event;

am_world_config am_default_world_config(void);

am_world* am_world_create(const am_world_config* config);
void am_world_destroy(am_world* world);

int am_world_upsert_body(am_world* world, const am_body_desc* desc);
int am_world_remove_body(am_world* world, uint64_t body_id);

int am_world_upsert_surface(am_world* world, const am_surface_desc* desc);
int am_world_remove_surface(am_world* world, uint64_t surface_id);

int am_world_pointer(am_world* world, uint64_t body_id, const am_pointer_event* event);
void am_world_step(am_world* world, double now_ms);

int am_world_get_frame(const am_world* world, uint64_t body_id, am_body_frame* out_frame);
int am_world_poll_event(am_world* world, am_event* out_event);

void am_world_set_policy(am_world* world, am_motion_policy policy);
am_motion_policy am_world_get_policy(const am_world* world);

#ifdef __cplusplus
}
#endif
