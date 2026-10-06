#include "amotion/amotion.h"

#include <cstdio>

int main() {
    am_world_config cfg = am_default_world_config();
    am_world* world = am_world_create(&cfg);

    am_body_desc body{};
    body.id = 1;
    body.rect = {40, 40, 260, 64};
    body.min_scale = 0.25;
    body.max_scale = 2.0;
    am_world_upsert_body(world, &body);

    am_surface_desc source{};
    source.id = 10;
    source.contract = AM_SURFACE_SOURCE;
    source.rect = {420, 220, 260, 64};
    source.magnet_px = 120;
    source.priority = 100;
    am_world_upsert_surface(world, &source);

    am_pointer_event e{};
    e.pointer_id = 1;
    e.buttons = 1;

    e.phase = AM_POINTER_ENTER; e.x = 80; e.y = 60; e.time_ms = 0;
    am_world_pointer(world, 1, &e);

    e.phase = AM_POINTER_DOWN; e.time_ms = 320;
    am_world_pointer(world, 1, &e);

    e.phase = AM_POINTER_MOVE; e.x = 500; e.y = 240; e.time_ms = 360;
    am_world_pointer(world, 1, &e);

    for (double t = 360; t < 600; t += 16.0) {
        am_world_step(world, t);
    }

    e.phase = AM_POINTER_UP; e.time_ms = 610;
    am_world_pointer(world, 1, &e);

    for (double t = 610; t < 1000; t += 16.0) {
        am_world_step(world, t);
    }

    am_body_frame frame{};
    am_world_get_frame(world, 1, &frame);

    std::printf("body=%llu rect=(%.1f %.1f %.1f %.1f) flags=%u surface=%llu contract=%d\n",
        static_cast<unsigned long long>(frame.id),
        frame.rect.x, frame.rect.y, frame.rect.width, frame.rect.height,
        frame.flags,
        static_cast<unsigned long long>(frame.surface_id),
        static_cast<int>(frame.surface_contract));

    am_event event{};
    while (am_world_poll_event(world, &event)) {
        std::printf("event=%d body=%llu surface=%llu contract=%d\n",
            static_cast<int>(event.type),
            static_cast<unsigned long long>(event.body_id),
            static_cast<unsigned long long>(event.surface_id),
            static_cast<int>(event.surface_contract));
    }

    am_world_destroy(world);
    return 0;
}
