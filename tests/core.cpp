#include "amotion/amotion.h"

#include <cassert>
#include <cmath>
#include <cstdio>

static am_pointer_event pointer(am_pointer_phase phase, double x, double y, double t) {
    am_pointer_event e{};
    e.phase = phase;
    e.pointer_id = 1;
    e.x = x;
    e.y = y;
    e.buttons = phase == AM_POINTER_UP ? 0u : 1u;
    e.time_ms = t;
    return e;
}

int main() {
    am_world_config cfg = am_default_world_config();
    am_world* world = am_world_create(&cfg);
    assert(world);

    am_body_desc body{};
    body.id = 1;
    body.rect = {20, 20, 240, 60};
    body.min_scale = 0.25;
    body.max_scale = 2.0;
    assert(am_world_upsert_body(world, &body));

    am_surface_desc source{};
    source.id = 7;
    source.contract = AM_SURFACE_SOURCE;
    source.rect = {500, 200, 240, 60};
    source.magnet_px = 180;
    source.priority = 100;
    assert(am_world_upsert_surface(world, &source));

    auto e = pointer(AM_POINTER_ENTER, 40, 40, 0);
    assert(am_world_pointer(world, 1, &e));
    am_world_step(world, 301);

    am_body_frame frame{};
    assert(am_world_get_frame(world, 1, &frame));
    assert((frame.flags & AM_FRAME_AFFORDANCE) != 0);

    e = pointer(AM_POINTER_DOWN, 40, 40, 320);
    assert(am_world_pointer(world, 1, &e));
    e = pointer(AM_POINTER_MOVE, 560, 230, 360);
    assert(am_world_pointer(world, 1, &e));

    assert(am_world_get_frame(world, 1, &frame));
    assert((frame.flags & AM_FRAME_DRAGGING) != 0);

    e = pointer(AM_POINTER_UP, 560, 230, 420);
    assert(am_world_pointer(world, 1, &e));

    for (double t = 420; t <= 1000; t += 16) {
        am_world_step(world, t);
    }

    assert(am_world_get_frame(world, 1, &frame));
    assert(frame.surface_id == 7);
    assert(frame.surface_contract == AM_SURFACE_SOURCE);
    assert(std::abs(frame.rect.x - source.rect.x) < 0.5);
    assert(std::abs(frame.rect.y - source.rect.y) < 0.5);

    bool saw_drag = false;
    bool saw_commit = false;
    am_event event{};
    while (am_world_poll_event(world, &event)) {
        saw_drag |= event.type == AM_EVENT_DRAG_START;
        saw_commit |= event.type == AM_EVENT_COMMIT && event.surface_id == 7;
    }
    assert(saw_drag);
    assert(saw_commit);

    am_world_destroy(world);
    std::puts("amotion_core_test: ok");
    return 0;
}
