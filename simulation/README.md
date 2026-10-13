# Lam Research Challenge 3.0 – Stage 2 rig simulator
Open `index.html` in a browser (works offline; Three.js is bundled as `three.min.js`).
`node run-tests.js` runs the 16 headless test cases (same code as the in-page "Tests" tab).

- `sim.js` plant (hydraulics, FT-01 pulses, float switch, faults) + ESP32-style controller state machine
- `tests.js` test cases TC-01…TC-16 · `scene.js` 3D model · `app.js` UI, charts, P&ID, calibration, theory
