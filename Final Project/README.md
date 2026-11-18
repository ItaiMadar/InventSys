# Physical r/place firmware

The first implementation step is the hardware-independent state machine in
`src/PlaceStateMachine.h` and `src/PlaceStateMachine.cpp`.

The core uses logical integer `(x, y)` coordinates and palette indices. It has
no Arduino, ESP32, LED, display, NFC, or storage dependencies. Future hardware
code will translate physical inputs into calls on this class and will be the
only code that maps logical coordinates to physical LED indices.

Confirmation is two-phase:

1. `confirm(nowMs)` changes the candidate pixel and enters `COMMITTING`.
2. The adapter atomically saves `board()`.
3. `finishCommit(true)` starts cooldown and returns to `IDLE`, while
   `finishCommit(false)` rolls back the pixel and enters `ERROR`.

All times are unsigned 32-bit millisecond values compatible with ESP32
`millis()`. Durations should remain below one full 32-bit timer cycle.

## ESP32 firmware skeleton

The repository is also a PlatformIO Arduino project. The provisional target is
the generic `esp32dev`; update `platformio.ini` after selecting the exact board.

- `src/main.cpp` runs the non-blocking firmware workflow.
- `include/ProjectConfig.h` contains timing, palette, and placeholder pins.
- `include/HardwareAdapter.h` defines the hardware boundary.
- `src/HardwareAdapter.cpp` contains the peripheral skeleton functions.
- `src/PlaceStateMachine.*` remains shared by ESP32 firmware and the emulator.

Build and upload after PlatformIO is installed:

```powershell
pio run
pio run --target upload
pio device monitor
```

All pins are currently `PIN_UNASSIGNED`. LED indexing, NFC serial parsing,
potentiometer filtering, button debouncing, screen drivers, LED drivers, and
atomic snapshot storage are explicit `TODO` items in `HardwareAdapter.cpp`.
Persistence is disabled, so confirmed placements currently remain in RAM and
are treated as successfully committed.

## Interaction preview

The browser emulator executes the real `PlaceStateMachine.cpp` through
WebAssembly. Rebuild it after changing the C++ source:

```powershell
.\simulator\build-simulator.ps1
```

Then open `simulator/place-simulator.html` in a browser. Tap either simulated
NFC card, use Left/Right to move through pixels in row-major order, use Up/Down
to change color, and press Space to confirm. The generated `place-core.js`
contains the compiled WebAssembly module and must remain beside the HTML file.
