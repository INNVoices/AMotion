#include "amotion/amotion.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <unordered_map>

namespace {

constexpr double kEpsilon = 1e-6;

struct Body {
    am_body_desc desc{};
    am_rect current{};
    am_rect target{};
    am_vec2 velocity{};
    bool hovered = false;
    bool pressed = false;
    bool dragging = false;
    bool settling = false;
    bool committed = false;
    bool affordance = false;
    uint32_t pointer_id = 0;
    am_vec2 press_pointer{};
    am_vec2 pointer{};
    am_vec2 grab_offset{};
    double hover_started_ms = 0.0;
    uint64_t preview_surface = 0;
    am_surface_contract preview_contract = AM_SURFACE_FREE;
};

struct Surface {
    am_surface_desc desc{};
};

double clamp_dt(double dt_ms, double max_ms) {
    if (!std::isfinite(dt_ms) || dt_ms <= 0.0) return 0.0;
    return std::min(dt_ms, std::max(1.0, max_ms));
}

double center_x(const am_rect& r) { return r.x + r.width * 0.5; }
double center_y(const am_rect& r) { return r.y + r.height * 0.5; }

double point_rect_distance(double x, double y, const am_rect& r) {
    const double dx = std::max({r.x - x, 0.0, x - (r.x + r.width)});
    const double dy = std::max({r.y - y, 0.0, y - (r.y + r.height)});
    return std::sqrt(dx * dx + dy * dy);
}

double length(am_vec2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

void spring_axis(double& value, double& velocity, double target, double hz, double dt_s) {
    if (dt_s <= 0.0) return;

    // Critically-damped analytic approximation. Stable at UI-sized dt and deterministic across hosts.
    const double omega = std::max(0.01, hz) * 2.0 * 3.14159265358979323846;
    const double x = value - target;
    const double exp_term = std::exp(-omega * dt_s);
    const double temp = (velocity + omega * x) * dt_s;

    value = target + (x + temp) * exp_term;
    velocity = (velocity - omega * temp) * exp_term;
}

bool near(double a, double b, double eps = 0.05) {
    return std::abs(a - b) <= eps;
}

} // namespace

struct am_world {
    am_world_config config{};
    std::unordered_map<uint64_t, Body> bodies;
    std::unordered_map<uint64_t, Surface> surfaces;
    std::deque<am_event> events;
    double last_step_ms = 0.0;
};

static void push_event(am_world* world, am_event_type type, const Body& body, uint64_t surface_id, am_surface_contract contract) {
    am_event ev{};
    ev.type = type;
    ev.body_id = body.desc.id;
    ev.surface_id = surface_id;
    ev.surface_contract = contract;
    ev.rect = body.current;
    world->events.push_back(ev);
}

static const Surface* choose_surface(const am_world* world, const Body& body) {
    const double cx = center_x(body.target);
    const double cy = center_y(body.target);

    const Surface* best = nullptr;
    double best_score = std::numeric_limits<double>::infinity();

    for (const auto& [id, surface] : world->surfaces) {
        (void)id;
        const auto& d = surface.desc;
        const double magnet = std::max(0.0, d.magnet_px);
        const double dist = point_rect_distance(cx, cy, d.rect);
        if (dist > magnet) continue;

        // Higher priority wins first; distance resolves ties.
        const double score = dist - static_cast<double>(d.priority) * 100000.0;
        if (score < best_score) {
            best_score = score;
            best = &surface;
        }
    }
    return best;
}

static am_rect canonical_target(const Body& body, const Surface& surface) {
    am_rect out = body.target;
    const am_rect& s = surface.desc.rect;

    switch (surface.desc.contract) {
        case AM_SURFACE_SOURCE:
        case AM_SURFACE_DOCK:
            out = s;
            break;
        case AM_SURFACE_EDGE: {
            const bool horizontal = s.width >= s.height;
            if (horizontal) {
                out.height = std::max(1.0, std::min(body.desc.rect.height, s.height));
                const double ratio = body.desc.rect.width / std::max(1.0, body.desc.rect.height);
                out.width = out.height * ratio;
            } else {
                out.width = std::max(1.0, std::min(body.desc.rect.width, s.width));
                const double ratio = body.desc.rect.height / std::max(1.0, body.desc.rect.width);
                out.height = out.width * ratio;
            }
            out.x = center_x(s) - out.width * 0.5;
            out.y = center_y(s) - out.height * 0.5;
            break;
        }
        case AM_SURFACE_CORNER: {
            const double side = std::max(1.0, std::min({body.desc.rect.width, body.desc.rect.height, s.width, s.height}));
            out.width = side;
            out.height = side;
            out.x = center_x(s) - side * 0.5;
            out.y = center_y(s) - side * 0.5;
            break;
        }
        case AM_SURFACE_FREE:
        default:
            break;
    }
    return out;
}

am_world_config am_default_world_config(void) {
    am_world_config cfg{};
    cfg.policy = AM_MOTION_FULL;
    cfg.hover_dwell_ms = 300.0;
    cfg.drag_threshold_px = 7.0;
    cfg.follow_hz = 18.0;
    cfg.settle_hz = 13.0;
    cfg.max_step_ms = 33.0;
    return cfg;
}

am_world* am_world_create(const am_world_config* config) {
    auto* world = new am_world();
    world->config = config ? *config : am_default_world_config();
    return world;
}

void am_world_destroy(am_world* world) {
    delete world;
}

int am_world_upsert_body(am_world* world, const am_body_desc* desc) {
    if (!world || !desc || desc->id == 0 || desc->rect.width <= 0.0 || desc->rect.height <= 0.0) return 0;

    auto it = world->bodies.find(desc->id);
    if (it == world->bodies.end()) {
        Body body{};
        body.desc = *desc;
        body.current = desc->rect;
        body.target = desc->rect;
        world->bodies.emplace(desc->id, body);
    } else {
        it->second.desc = *desc;
        if (!it->second.dragging && !it->second.settling) {
            it->second.current = desc->rect;
            it->second.target = desc->rect;
        }
    }
    return 1;
}

int am_world_remove_body(am_world* world, uint64_t body_id) {
    return world ? static_cast<int>(world->bodies.erase(body_id) > 0) : 0;
}

int am_world_upsert_surface(am_world* world, const am_surface_desc* desc) {
    if (!world || !desc || desc->id == 0 || desc->rect.width <= 0.0 || desc->rect.height <= 0.0) return 0;
    world->surfaces[desc->id].desc = *desc;
    return 1;
}

int am_world_remove_surface(am_world* world, uint64_t surface_id) {
    return world ? static_cast<int>(world->surfaces.erase(surface_id) > 0) : 0;
}

int am_world_pointer(am_world* world, uint64_t body_id, const am_pointer_event* event) {
    if (!world || !event) return 0;
    auto it = world->bodies.find(body_id);
    if (it == world->bodies.end()) return 0;
    Body& body = it->second;

    const am_vec2 p{event->x, event->y};

    switch (event->phase) {
        case AM_POINTER_ENTER:
            body.hovered = true;
            body.hover_started_ms = event->time_ms;
            body.pointer = p;
            break;

        case AM_POINTER_LEAVE:
            body.hovered = false;
            if (!body.dragging && !body.pressed) body.affordance = false;
            break;

        case AM_POINTER_DOWN:
            body.pressed = true;
            body.committed = false;
            body.pointer_id = event->pointer_id;
            body.press_pointer = p;
            body.pointer = p;
            body.grab_offset = {p.x - body.current.x, p.y - body.current.y};
            break;

        case AM_POINTER_MOVE:
            if (body.pointer_id != 0 && event->pointer_id != body.pointer_id) break;
            body.pointer = p;
            if (body.pressed && !body.dragging) {
                const am_vec2 delta{p.x - body.press_pointer.x, p.y - body.press_pointer.y};
                if (length(delta) >= std::max(0.0, world->config.drag_threshold_px)) {
                    body.dragging = true;
                    body.settling = false;
                    push_event(world, AM_EVENT_DRAG_START, body, 0, AM_SURFACE_FREE);
                }
            }
            if (body.dragging) {
                body.target.x = p.x - body.grab_offset.x;
                body.target.y = p.y - body.grab_offset.y;

                const Surface* surface = choose_surface(world, body);
                const uint64_t next_surface = surface ? surface->desc.id : 0;
                const am_surface_contract next_contract = surface ? surface->desc.contract : AM_SURFACE_FREE;
                if (next_surface != body.preview_surface || next_contract != body.preview_contract) {
                    body.preview_surface = next_surface;
                    body.preview_contract = next_contract;
                    push_event(world, AM_EVENT_SURFACE_PREVIEW, body, next_surface, next_contract);
                }
            }
            break;

        case AM_POINTER_UP: {
            if (body.pointer_id != 0 && event->pointer_id != body.pointer_id) break;
            body.pointer = p;

            if (!body.dragging) {
                if (body.pressed) push_event(world, AM_EVENT_CLICK, body, 0, AM_SURFACE_FREE);
            } else {
                const Surface* surface = choose_surface(world, body);
                if (surface) {
                    body.target = canonical_target(body, *surface);
                    body.preview_surface = surface->desc.id;
                    body.preview_contract = surface->desc.contract;
                    body.settling = world->config.policy != AM_MOTION_OFF;
                    body.committed = world->config.policy == AM_MOTION_OFF;
                    if (world->config.policy == AM_MOTION_OFF) body.current = body.target;
                    push_event(world, AM_EVENT_COMMIT, body, surface->desc.id, surface->desc.contract);
                } else {
                    body.preview_surface = 0;
                    body.preview_contract = AM_SURFACE_FREE;
                }
                push_event(world, AM_EVENT_DRAG_END, body, body.preview_surface, body.preview_contract);
            }

            body.pressed = false;
            body.dragging = false;
            body.pointer_id = 0;
            break;
        }

        case AM_POINTER_CANCEL:
            body.pressed = false;
            body.dragging = false;
            body.pointer_id = 0;
            body.preview_surface = 0;
            body.preview_contract = AM_SURFACE_FREE;
            body.target = body.current;
            break;
    }

    return 1;
}

void am_world_step(am_world* world, double now_ms) {
    if (!world) return;

    if (world->last_step_ms <= 0.0) {
        world->last_step_ms = now_ms;
    }
    const double dt_ms = clamp_dt(now_ms - world->last_step_ms, world->config.max_step_ms);
    world->last_step_ms = now_ms;

    for (auto& [id, body] : world->bodies) {
        (void)id;

        if (body.hovered && !body.affordance && !body.dragging) {
            if (now_ms - body.hover_started_ms >= world->config.hover_dwell_ms) {
                body.affordance = true;
            }
        }

        if (world->config.policy == AM_MOTION_OFF) {
            body.current = body.target;
            body.velocity = {};
            body.settling = false;
            continue;
        }

        const double hz = body.dragging ? world->config.follow_hz : world->config.settle_hz;
        const double dt_s = dt_ms / 1000.0;

        spring_axis(body.current.x, body.velocity.x, body.target.x, hz, dt_s);
        spring_axis(body.current.y, body.velocity.y, body.target.y, hz, dt_s);

        if (body.settling) {
            const double size_hz = world->config.policy == AM_MOTION_REDUCED ? world->config.settle_hz * 2.0 : world->config.settle_hz;
            double size_vx = 0.0;
            double size_vy = 0.0;
            spring_axis(body.current.width, size_vx, body.target.width, size_hz, dt_s);
            spring_axis(body.current.height, size_vy, body.target.height, size_hz, dt_s);

            const bool done =
                near(body.current.x, body.target.x) &&
                near(body.current.y, body.target.y) &&
                near(body.current.width, body.target.width) &&
                near(body.current.height, body.target.height) &&
                length(body.velocity) < 0.08;

            if (done) {
                body.current = body.target;
                body.velocity = {};
                body.settling = false;
                body.committed = true;
            }
        }
    }
}

int am_world_get_frame(const am_world* world, uint64_t body_id, am_body_frame* out_frame) {
    if (!world || !out_frame) return 0;
    const auto it = world->bodies.find(body_id);
    if (it == world->bodies.end()) return 0;
    const Body& body = it->second;

    am_body_frame frame{};
    frame.id = body.desc.id;
    frame.rect = body.current;
    frame.scale_x = body.desc.rect.width > kEpsilon ? body.current.width / body.desc.rect.width : 1.0;
    frame.scale_y = body.desc.rect.height > kEpsilon ? body.current.height / body.desc.rect.height : 1.0;
    frame.rotation_deg = 0.0;
    frame.opacity = 1.0;
    frame.depth = body.dragging ? 1.0 : (body.settling ? 0.5 : 0.0);
    frame.surface_id = body.preview_surface;
    frame.surface_contract = body.preview_contract;

    if (body.hovered) frame.flags |= AM_FRAME_HOVERED;
    if (body.affordance) frame.flags |= AM_FRAME_AFFORDANCE;
    if (body.pressed) frame.flags |= AM_FRAME_PRESSED;
    if (body.dragging) frame.flags |= AM_FRAME_DRAGGING;
    if (body.settling) frame.flags |= AM_FRAME_SETTLING;
    if (body.committed) frame.flags |= AM_FRAME_COMMITTED;

    *out_frame = frame;
    return 1;
}

int am_world_poll_event(am_world* world, am_event* out_event) {
    if (!world || !out_event || world->events.empty()) return 0;
    *out_event = world->events.front();
    world->events.pop_front();
    return 1;
}

void am_world_set_policy(am_world* world, am_motion_policy policy) {
    if (!world) return;
    world->config.policy = policy;
}

am_motion_policy am_world_get_policy(const am_world* world) {
    return world ? world->config.policy : AM_MOTION_OFF;
}
