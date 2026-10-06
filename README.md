# AMotion

Native behavioural motion runtime for UI hosts.

AMotion is not a product and not a JS animation library. It is the native core behind **AnchorMotion**: a small runtime that takes geometry, pointer input and surface rules, performs the heavy interaction math outside a WebView, and returns compact transform/state frames to the host.

**Doctrine:** rigid geometry + viscous relationship.

The object's identity and hull stay disciplined. Adhesion, weight, lag, docking, absorption and inertia live in the relationship between the object, pointer and surfaces.

## M0

This first cut intentionally does only the reusable core:

- C++20 native runtime;
- stable C ABI;
- body + surface registry;
- pointer intent and click-vs-drag threshold;
- 300 ms hover affordance state;
- FREE / SOURCE / DOCK / EDGE / CORNER contracts;
- viscous follow and settle;
- canonical geometry transition on commit;
- Full / Reduced / Off;
- compact frame output + semantic event queue.

The host owns DOM/native rendering, business actions, persistence, permissions and product state.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the tiny native demo:

```bash
./build/amotion_demo
```

On multi-config generators (Visual Studio):

```powershell
.\build\Release\amotion_demo.exe
```

## Integration shape

A WebView/Electron/Tauri/Wails/etc. adapter should be thin:

```text
pointer + geometry + surface rules
              |
              v
         AMotion native
              |
              v
     frame packet + events
              |
              v
    CSS/native transforms
```

Do not put product logic into AMotion. Do not make the core know Electron, React, Anchor, launchers or server shortcuts.

The next work is a real host adapter and richer rule solving behind this ABI.
