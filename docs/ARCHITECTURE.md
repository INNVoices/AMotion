# AMotion M0 architecture

AMotion is not a UI framework and not a collection of easing helpers.

The native core owns the expensive and reusable part of direct manipulation:

- pointer intent;
- motion state;
- relationship physics;
- canonical geometry transitions;
- surface discovery input;
- surface contract resolution;
- deterministic frame output.

The host owns rendering and business semantics.

## Boundary

Host -> AMotion:
- body geometry;
- surface geometry + contract;
- pointer events;
- time;
- policy.

AMotion -> host:
- one compact body frame;
- semantic events: click, drag start/end, preview, commit.

The host should batch bridge traffic. WebView/Electron integration should avoid per-property IPC. A host adapter sends input/geometry changes to native code and applies one frame packet per body during rendering.

## Rule direction

M0 begins with a tiny rule vocabulary:
- FREE
- SOURCE
- DOCK
- EDGE
- CORNER
- Full / Reduced / Off
- hover dwell
- click-vs-drag threshold
- rigid geometry + viscous follow/settle

The next versions improve the solver and add rules behind the same ABI. They do not grow product-specific behavior into core.
