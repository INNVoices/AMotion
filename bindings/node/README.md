# AMotion Node host adapter

Thin Node-API binding over the native AMotion core.

This is the integration path for Electron / NW.js and any Node host that wants AMotion as an internal native module rather than a JS physics package.

## Build

```bash
cd bindings/node
npm install
npm run build
```

## Example

```js
const { World } = require('./build/Release/amotion.node');

const world = new World({
  policy: 'full',
  hoverDwellMs: 300,
  dragThresholdPx: 7
});

world.upsertBody({ id: 1, x: 0, y: 0, width: 260, height: 64 });
world.upsertSurface({
  id: 7,
  contract: 'source',
  x: 500,
  y: 200,
  width: 260,
  height: 64,
  magnetPx: 120,
  priority: 100
});

world.pointer(1, { phase: 'enter', pointerId: 1, x: 20, y: 20, timeMs: 0 });
world.step(301);
console.log(world.frame(1));
```

The adapter intentionally exposes native frames/events. DOM integration belongs in the host layer, not here.
