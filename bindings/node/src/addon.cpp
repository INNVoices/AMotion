#include <napi.h>
#include "amotion/amotion.h"

#include <cstdint>
#include <string>

namespace {

am_motion_policy parse_policy(const Napi::Value& value) {
    if (!value.IsString()) return AM_MOTION_FULL;
    const std::string v = value.As<Napi::String>().Utf8Value();
    if (v == "off") return AM_MOTION_OFF;
    if (v == "reduced") return AM_MOTION_REDUCED;
    return AM_MOTION_FULL;
}

am_surface_contract parse_contract(const Napi::Value& value) {
    if (!value.IsString()) return AM_SURFACE_FREE;
    const std::string v = value.As<Napi::String>().Utf8Value();
    if (v == "source") return AM_SURFACE_SOURCE;
    if (v == "dock") return AM_SURFACE_DOCK;
    if (v == "edge") return AM_SURFACE_EDGE;
    if (v == "corner") return AM_SURFACE_CORNER;
    return AM_SURFACE_FREE;
}

const char* contract_name(am_surface_contract value) {
    switch (value) {
        case AM_SURFACE_SOURCE: return "source";
        case AM_SURFACE_DOCK: return "dock";
        case AM_SURFACE_EDGE: return "edge";
        case AM_SURFACE_CORNER: return "corner";
        case AM_SURFACE_FREE:
        default: return "free";
    }
}

am_pointer_phase parse_phase(const Napi::Value& value) {
    if (!value.IsString()) return AM_POINTER_MOVE;
    const std::string v = value.As<Napi::String>().Utf8Value();
    if (v == "enter") return AM_POINTER_ENTER;
    if (v == "leave") return AM_POINTER_LEAVE;
    if (v == "down") return AM_POINTER_DOWN;
    if (v == "up") return AM_POINTER_UP;
    if (v == "cancel") return AM_POINTER_CANCEL;
    return AM_POINTER_MOVE;
}

const char* event_name(am_event_type value) {
    switch (value) {
        case AM_EVENT_CLICK: return "click";
        case AM_EVENT_DRAG_START: return "drag-start";
        case AM_EVENT_DRAG_END: return "drag-end";
        case AM_EVENT_SURFACE_PREVIEW: return "surface-preview";
        case AM_EVENT_COMMIT: return "commit";
        case AM_EVENT_NONE:
        default: return "none";
    }
}

double number_or(const Napi::Object& o, const char* key, double fallback) {
    const Napi::Value v = o.Get(key);
    return v.IsNumber() ? v.As<Napi::Number>().DoubleValue() : fallback;
}

std::uint64_t id_or(const Napi::Object& o, const char* key, std::uint64_t fallback = 0) {
    const Napi::Value v = o.Get(key);
    if (v.IsBigInt()) {
        bool lossless = false;
        const auto id = v.As<Napi::BigInt>().Uint64Value(&lossless);
        return lossless ? id : fallback;
    }
    if (v.IsNumber()) {
        const double n = v.As<Napi::Number>().DoubleValue();
        return n >= 0.0 ? static_cast<std::uint64_t>(n) : fallback;
    }
    return fallback;
}

am_rect read_rect(const Napi::Object& o) {
    am_rect r{};
    r.x = number_or(o, "x", 0.0);
    r.y = number_or(o, "y", 0.0);
    r.width = number_or(o, "width", 0.0);
    r.height = number_or(o, "height", 0.0);
    return r;
}

Napi::Object write_rect(Napi::Env env, const am_rect& r) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("x", r.x);
    o.Set("y", r.y);
    o.Set("width", r.width);
    o.Set("height", r.height);
    return o;
}

class World final : public Napi::ObjectWrap<World> {
public:
    static Napi::Function Init(Napi::Env env) {
        return DefineClass(env, "World", {
            InstanceMethod("upsertBody", &World::UpsertBody),
            InstanceMethod("removeBody", &World::RemoveBody),
            InstanceMethod("upsertSurface", &World::UpsertSurface),
            InstanceMethod("removeSurface", &World::RemoveSurface),
            InstanceMethod("pointer", &World::Pointer),
            InstanceMethod("step", &World::Step),
            InstanceMethod("frame", &World::Frame),
            InstanceMethod("pollEvent", &World::PollEvent),
            InstanceMethod("setPolicy", &World::SetPolicy)
        });
    }

    explicit World(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<World>(info) {
        am_world_config cfg = am_default_world_config();

        if (info.Length() > 0 && info[0].IsObject()) {
            const Napi::Object o = info[0].As<Napi::Object>();
            cfg.policy = parse_policy(o.Get("policy"));
            cfg.hover_dwell_ms = number_or(o, "hoverDwellMs", cfg.hover_dwell_ms);
            cfg.drag_threshold_px = number_or(o, "dragThresholdPx", cfg.drag_threshold_px);
            cfg.follow_hz = number_or(o, "followHz", cfg.follow_hz);
            cfg.settle_hz = number_or(o, "settleHz", cfg.settle_hz);
            cfg.max_step_ms = number_or(o, "maxStepMs", cfg.max_step_ms);
        }

        world_ = am_world_create(&cfg);
        if (!world_) {
            Napi::Error::New(info.Env(), "am_world_create failed").ThrowAsJavaScriptException();
        }
    }

    ~World() override {
        am_world_destroy(world_);
        world_ = nullptr;
    }

private:
    am_world* world_ = nullptr;

    Napi::Value UpsertBody(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!world_ || info.Length() < 1 || !info[0].IsObject()) return Napi::Boolean::New(env, false);

        const Napi::Object o = info[0].As<Napi::Object>();
        am_body_desc d{};
        d.id = id_or(o, "id");
        d.rect = read_rect(o);
        d.min_scale = number_or(o, "minScale", 0.25);
        d.max_scale = number_or(o, "maxScale", 2.0);
        return Napi::Boolean::New(env, am_world_upsert_body(world_, &d) != 0);
    }

    Napi::Value RemoveBody(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!world_ || info.Length() < 1) return Napi::Boolean::New(env, false);

        std::uint64_t id = 0;
        if (info[0].IsBigInt()) {
            bool lossless = false;
            id = info[0].As<Napi::BigInt>().Uint64Value(&lossless);
            if (!lossless) return Napi::Boolean::New(env, false);
        } else if (info[0].IsNumber()) {
            id = static_cast<std::uint64_t>(info[0].As<Napi::Number>().DoubleValue());
        }
        return Napi::Boolean::New(env, am_world_remove_body(world_, id) != 0);
    }

    Napi::Value UpsertSurface(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!world_ || info.Length() < 1 || !info[0].IsObject()) return Napi::Boolean::New(env, false);

        const Napi::Object o = info[0].As<Napi::Object>();
        am_surface_desc d{};
        d.id = id_or(o, "id");
        d.contract = parse_contract(o.Get("contract"));
        d.rect = read_rect(o);
        d.magnet_px = number_or(o, "magnetPx", 48.0);
        d.priority = static_cast<std::int32_t>(number_or(o, "priority", 0.0));
        return Napi::Boolean::New(env, am_world_upsert_surface(world_, &d) != 0);
    }

    Napi::Value RemoveSurface(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!world_ || info.Length() < 1) return Napi::Boolean::New(env, false);

        std::uint64_t id = info[0].IsNumber()
            ? static_cast<std::uint64_t>(info[0].As<Napi::Number>().DoubleValue())
            : 0;
        return Napi::Boolean::New(env, am_world_remove_surface(world_, id) != 0);
    }

    Napi::Value Pointer(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!world_ || info.Length() < 2 || !info[1].IsObject()) return Napi::Boolean::New(env, false);

        std::uint64_t body_id = info[0].IsNumber()
            ? static_cast<std::uint64_t>(info[0].As<Napi::Number>().DoubleValue())
            : 0;

        const Napi::Object o = info[1].As<Napi::Object>();
        am_pointer_event e{};
        e.phase = parse_phase(o.Get("phase"));
        e.pointer_id = static_cast<std::uint32_t>(number_or(o, "pointerId", 1.0));
        e.x = number_or(o, "x", 0.0);
        e.y = number_or(o, "y", 0.0);
        e.buttons = static_cast<std::uint32_t>(number_or(o, "buttons", 0.0));
        e.time_ms = number_or(o, "timeMs", 0.0);

        return Napi::Boolean::New(env, am_world_pointer(world_, body_id, &e) != 0);
    }

    Napi::Value Step(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (world_ && info.Length() > 0 && info[0].IsNumber()) {
            am_world_step(world_, info[0].As<Napi::Number>().DoubleValue());
        }
        return env.Undefined();
    }

    Napi::Value Frame(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!world_ || info.Length() < 1 || !info[0].IsNumber()) return env.Null();

        const auto id = static_cast<std::uint64_t>(info[0].As<Napi::Number>().DoubleValue());
        am_body_frame f{};
        if (!am_world_get_frame(world_, id, &f)) return env.Null();

        Napi::Object o = Napi::Object::New(env);
        o.Set("id", Napi::BigInt::New(env, f.id));
        o.Set("rect", write_rect(env, f.rect));
        o.Set("scaleX", f.scale_x);
        o.Set("scaleY", f.scale_y);
        o.Set("rotationDeg", f.rotation_deg);
        o.Set("opacity", f.opacity);
        o.Set("depth", f.depth);
        o.Set("flags", f.flags);
        o.Set("surfaceId", Napi::BigInt::New(env, f.surface_id));
        o.Set("surfaceContract", contract_name(f.surface_contract));
        return o;
    }

    Napi::Value PollEvent(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        am_event e{};
        if (!world_ || !am_world_poll_event(world_, &e)) return env.Null();

        Napi::Object o = Napi::Object::New(env);
        o.Set("type", event_name(e.type));
        o.Set("bodyId", Napi::BigInt::New(env, e.body_id));
        o.Set("surfaceId", Napi::BigInt::New(env, e.surface_id));
        o.Set("surfaceContract", contract_name(e.surface_contract));
        o.Set("rect", write_rect(env, e.rect));
        return o;
    }

    Napi::Value SetPolicy(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (world_ && info.Length() > 0) am_world_set_policy(world_, parse_policy(info[0]));
        return env.Undefined();
    }
};

Napi::Object init(Napi::Env env, Napi::Object exports) {
    exports.Set("World", World::Init(env));
    return exports;
}

} // namespace

NODE_API_MODULE(amotion, init)
