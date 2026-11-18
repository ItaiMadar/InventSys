# Interactive Physical r/place Board — Project Vision

## Vision

Build a physical, interactive model inspired by Reddit's r/place. An ESP32 controls an addressable LED grid that represents a shared canvas. A participant identifies themselves with an NFC card, selects a pixel, chooses its position and color through physical controls, and commits it to the persistent board. Existing pixels may be overwritten. Each NFC identity may place only one pixel during a configurable cooldown period.

The experience should be immediate and understandable: tap a card, move a flashing preview pixel, choose a color, and confirm the placement.

## System Components

The ESP32 is connected to:

- `k` LED strips, arranged as a two-dimensional display grid.
- Two variable-resistor knobs/potentiometers:
  - X control: selects a column.
  - Y control: selects a row.
- One or two small display screens showing:
  - Available/current color.
  - User cooldown status or remaining cooldown time.
  - Interaction prompts and feedback, where practical.
- Two color-selection buttons:
  - Previous/up.
  - Next/down.
- One confirmation button that commits a placement.
- An NFC reader that supplies a card's unique identifier to the ESP32, currently expected through a serial interface.

## Configurable Values

At minimum, the firmware should expose these configuration values:

- `GRID_WIDTH`: number of LED columns.
- `GRID_HEIGHT`: number of LED rows.
- `LED_STRIP_COUNT` (or `k`): number of physical LED strips.
- `FLASH_RATE`: rate at which the selected preview pixel flashes.
- `COOLDOWN_TIME`: seconds an NFC identity must wait between placements.
- `SESSION_TIMEOUT`: seconds of no participant input before an active placement is cancelled.
- Available color palette.
- Input pin assignments, LED brightness, and display/NFC interface settings.

## Initial State

- Every board pixel begins in the `OFF` state.
- No placement session is active.
- The screen prompts the next participant to tap an NFC card.
- The saved board snapshot is loaded from onboard flash when valid; otherwise the board starts empty.
- For the demo, cooldown records are held in RAM and reset when the ESP32 reboots.

## Participant Workflow

1. **Identify**
   - A participant taps an NFC card bearing a unique ID.
   - The ESP32 reads and validates the ID.

2. **Check cooldown**
   - If the ID is still under cooldown, placement is denied.
   - The screen displays the denial and, when available, the remaining cooldown time.
   - If the cooldown has expired or the ID has no cooldown record, a placement session begins.

3. **Initialize the preview**
   - The system chooses an `OFF` pixel nearest to the center of the board.
   - If no `OFF` pixel exists, it starts on the center pixel instead.
   - This pixel becomes the placement preview.
   - Its default preview color is `WHITE`.
   - It flashes at `FLASH_RATE` while the session is active.

4. **Select a location and color**
   - Turning the X knob moves the preview left or right by selecting a column.
   - Turning the Y knob moves the preview up or down by selecting a row.
   - The preview may move onto an occupied pixel; confirming it will overwrite that pixel.
   - The preview pixel continues flashing at its current location.
   - The two color buttons cycle backward or forward through the palette.
   - The screen displays the currently selected color.
   - Previously committed pixels remain visible and unchanged during preview.

5. **Confirm placement**
   - Pressing the confirmation button commits the preview pixel at the selected coordinate and color.
   - The pixel stops flashing and becomes part of the stationary board state.
   - The updated board state is saved.
   - The participant's NFC ID and placement time are saved for cooldown enforcement.
   - The session ends and the system returns to the NFC-tap prompt.

## Core Behavioral Requirements

- Only an NFC identity that is not under cooldown may begin a placement session.
- Only one participant may control the board at a time.
- The preview must be visually distinguishable from committed pixels by flashing.
- Preview rendering must not modify the committed board state until confirmation.
- Any pixel, occupied or `OFF`, may be selected and overwritten.
- A confirmed placement must update both the in-memory board and persistent storage.
- A confirmed placement must start or reset the participant's cooldown.
- Color-selection buttons must cycle through the palette, including wraparound at both ends.
- Input handling must be non-blocking so flashing, controls, NFC input, and screen updates remain responsive.
- Buttons should be debounced.
- Potentiometer readings should be filtered or quantized to prevent unwanted movement near cell boundaries.
- Knob movement, a valid color-button press, or another accepted placement control updates the session's last-input time.
- If no accepted participant input occurs for `SESSION_TIMEOUT` seconds, the preview is discarded and the system returns to `IDLE` without changing the board or starting a cooldown.
- Repeated NFC scans alone should not keep an abandoned placement session alive.
- The flashing preview alternates between its selected color and the committed color beneath it. For an unoccupied pixel, the underlying committed color is `OFF`.
- Only meaningful participant input resets `SESSION_TIMEOUT`: entering a different integer grid coordinate or accepting a color-button change. Potentiometer noise that remains within the same cell does not reset it.
- Expired UID entries are removed from the in-memory cooldown table.
- All elapsed-time comparisons use unsigned arithmetic so ESP32 `millis()` rollover is handled correctly.
- Board snapshots are replaced atomically so an interrupted write leaves the previous valid snapshot intact.

## Proposed Operating States

The firmware should behave as a state machine:

1. `BOOT`: initialize hardware and restore saved data.
2. `IDLE`: display the NFC prompt and wait for a card.
3. `COOLDOWN_DENIED`: show the remaining cooldown, then return to `IDLE`.
4. `PLACING`: accept knob and color input while rendering the flashing preview; cancel to `IDLE` on `SESSION_TIMEOUT`.
5. `COMMITTING`: atomically save the pixel and cooldown record.
6. `ERROR`: show a recoverable hardware or storage error and preserve the last valid board state.

## Data That Must Be Stored

### Board state

For every grid coordinate:

- Whether the pixel is `OFF` or occupied.
- Its committed color/palette index.

### Cooldown state (demo runtime only)

For every known NFC identity:

- A stable representation of the card UID.
- The last successful placement time, or the time at which the cooldown expires.

For the first demo, these records live in RAM and are cleared by a restart. The board snapshot is persistent even though cooldowns are not. If a real-time clock or network time is added later, cooldown records can also be persisted safely.

### Optional placement history

A future append-only history record may contain the UID, coordinate, palette index, and timestamp/sequence number. History is not required for the first demo and should use a dedicated filesystem or partition rather than repeatedly enlarging an NVS value.

The persistent storage format should be versioned and include grid dimensions plus an integrity check so firmware can reject an incompatible or incomplete snapshot.

## Persistence Feasibility and Demo Policy

Onboard persistence is feasible. A compact snapshot needs one palette index per pixel: 1 KiB for a 32×32 grid, 4 KiB for 64×64, or 10 KiB for 100×100 when each cell uses one byte. This is small relative to the flash commonly fitted to ESP32 modules.

The demo policy is:

- Keep the working board and cooldown table in RAM.
- Save a versioned board snapshot after each confirmed placement.
- Restore a valid board snapshot during boot.
- Reset all cooldowns on reboot because the demo has no trustworthy wall clock.
- Never write flash for preview movement, flashing, color changes, denied scans, or timed-out sessions.
- Treat persistence failures as errors without corrupting the last valid snapshot.

NVS/Arduino `Preferences` is suitable for an infrequently updated snapshot blob. If the board or history becomes substantially larger, use a dedicated LittleFS or data partition with an atomic replacement strategy. Exact capacity depends on the selected ESP32 module and its partition table and must be verified before hardware integration.

## Hardware Abstraction Boundary

LED wiring topology remains intentionally undecided. Core logic must not depend on strip order or serpentine wiring. Hardware-specific code should be isolated behind small functions such as:

- `beginHardware()`
- `readNfcUid()`
- `readControls()`
- `renderBoardAndPreview()`
- `mapCoordinateToLedIndex(int x, int y)`
- `showIdleScreen()` / `showPlacementScreen()` / `showCooldownScreen()`
- `loadBoardSnapshot()` / `saveBoardSnapshot()`

Initial implementations may be stubs or host-side fakes. Clear `TODO` markers should identify the future pin definitions, LED library calls, display driver calls, NFC serial parsing, and physical coordinate mapping.

All logical X and Y coordinates are integers. `mapCoordinateToLedIndex(int x, int y)` is the only function allowed to know the eventual physical strip topology; core board and placement logic operates only on logical coordinates.

## Confirmed Design Decisions

- Occupied pixels may be overwritten, including when the board has no `OFF` pixels.
- The preferred starting preview is the nearest `OFF` pixel to center; a full board falls back to the center pixel.
- An inactive placement is cancelled after `SESSION_TIMEOUT` seconds and returns to the pre-scan `IDLE` state.
- Ordinary NFC UIDs provide sufficient identity for the demo; clone resistance is out of scope.
- LED strip wiring and coordinate topology will be supplied later through the hardware abstraction boundary.
- Board state persists onboard when capacity permits; demo cooldowns reset on reboot.
- Preview flashing preserves context by alternating the selected color with the underlying committed color.
- Only a change of integer coordinate or an accepted button action counts as session activity; raw analog noise does not.
- Expired RAM cooldown records are pruned.
- Timing uses rollover-safe unsigned elapsed-time arithmetic.
- Snapshot replacement is atomic and preserves the last valid snapshot on failure.

## Important Open Design Decisions

- **Exact board and palette size:** These determine RAM use, snapshot size, rendering cost, and power needs.
- **Selected ESP32 module and partition table:** These determine the flash capacity actually available to the application and snapshot/history storage.
- **History retention:** Decide whether placement history is needed beyond the latest snapshot and, if so, its maximum size and rollover policy.
- **LED topology:** Define strip type, grid dimensions, serpentine/linear wiring order, power injection, maximum current, and coordinate mapping.
- **Display topology:** Choose one combined display or two dedicated displays and select the communication bus/addressing scheme.
- **Accessibility and feedback:** Decide whether to add labels, sound, animations, or confirmation/error signals.

## Success Criteria for a First Prototype

A first prototype is successful when it can:

1. Initialize a small LED grid with all pixels off.
2. Read a repeatable UID from an NFC card.
3. Admit an eligible UID and reject one still under cooldown.
4. Select a center-nearest starting pixel.
5. Move a stable flashing preview with two potentiometers.
6. Cycle a small color palette with debounced buttons.
7. Commit one pixel with the confirmation button without corrupting other pixels.
8. Enforce cooldown for the UID.
9. Cancel an inactive placement after `SESSION_TIMEOUT` without committing a pixel or starting cooldown.
10. Overwrite an occupied pixel, including on a completely full board.
11. Recover the committed board after a restart while beginning with an empty demo cooldown table.

## Out of Scope Until Explicitly Added

- Internet-connected multiplayer synchronization.
- User accounts beyond NFC identity.
- A remote administration interface.
- Moderation, undo, or placement history playback.
- Cryptographically secure identity or anti-cloning guarantees.

