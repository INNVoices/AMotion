# AMotion

AMotion is the motion/runtime shell behind **AnchorMotion**.

This repository starts deliberately small: a working host-contract plugin, not a finished product. The first goal is to make the interaction contour usable from any WebView-based desktop shell without coupling it to Electron, Tauri, Wails, NW.js or Neutralinojs.

## M0

M0 provides:

- one host-neutral motion engine;
- one browser/WebView adapter;
- rigid object geometry with viscous pointer/surface relationship;
- click vs drag intent separation;
- 300 ms hover affordance;
- FREE / SOURCE / DOCK / EDGE / CORNER surface contracts;
- Full / Reduced / Off motion policy;
- canonical transform state hand-off through callbacks;
- no product/business logic.

A host owns persistence, commands, permissions and side effects. AMotion only owns interaction and motion.

## Quick start

```ts
import { createAMotion, createWebHost } from "@innvoices/amotion";

const host = createWebHost(document);
const motion = createAMotion(host, { policy: "full" });

motion.registerSurface(document.querySelector("#source")!, {
  id: "source",
  contract: "source",
});

motion.attach(document.querySelector("#sticker")!, {
  id: "sticker",
  hoverDwellMs: 300,
  dragThresholdPx: 7,
  onClick: () => console.log("launch intent"),
  onCommit: (result) => console.log(result),
});
```

## Doctrine

**Rigid geometry + viscous relationship.**

The object's hull remains disciplined. Weight, adhesion, lag and surface tension live in the relationship between object, pointer and surface.

## Status

M0 shell. Improve by replacing internals behind the same host contract instead of growing one-off host-specific motion systems.

© 2026 INNVoices. All rights reserved.
