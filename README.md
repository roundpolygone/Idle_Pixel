# Idle 64 (Idle_Pixel)

An immersive, retro-inspired incremental idle game and clockface built in C for Pebble color smartwatches (**Pebble Time / basalt**, **Pebble Time Round / chalk**, and **Pebble Time 2 / emery**).

---

## 🎮 Game Logic & Progression

Idle 64 is structured into a multi-tiered incremental progression system:

```
+---+--------------------------+
|   |  COL 1/64 | GP 0         |
|   +--------------------------+
| 6 |                          |
| 4 |  [■][■][■][■][■][■][■][■]|
|   |  [■][■][■][■][■][■][■][■]|
| S |  [■][■][■][▓][ ][ ][ ][ ]|  <-- Tier 1 Live Fill
| L |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| I |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| C |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| E |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| S |  [ ][ ][ ][ ][ ][ ][ ][ ]|
+---+--------------------------+
|      3.64 px/s [+550.0%]     |
+------------------------------+
|   ^ Tier 2 (Hold: Clock)     |
|   = Menu | v Hold for Boost  |
+------------------------------+
```

### 1. Tier 1: Solid Color Canvas (8x8 Grid)
- **The Canvas**: An 8x8 grid (64 pixels) that fills with a single, solid color selected sequentially from the 64-color Pebble hardware palette.
- **Base Speed**: Starts at 5.0 seconds per pixel (0.20 px/s), upgradeable in the Store and boostable with manual button presses or continuous hold auto-pressing.
- **Completion Flow**: When the 64th pixel completes, the watch vibrates once and enters a completion preview showing the solid color canvas. On button press, the backlight illuminates and triggers the fuse-and-shrink animation into Tier 2. Tier 1 automatically begins filling the next color.

### 2. Vertical Progress Gauge (64 Slices)
- Located directly to the left of the Tier 1 grid.
- Divided into 64 vertical slices building from bottom to top.
- Each completed Tier 1 solid grid fills 1/64th of this gauge with its corresponding color.

### 3. Tier 2: The 64-Color Master Grid
- Pressing **`UP`** from the main canvas zooms out to the Tier 2 view.
- Displays all 64 color slots of the full palette:
  - Completed colors appear as solid colored tiles.
  - The currently filling color shows live filling progress matching the selected fill pattern.
  - Future slots remain empty.
- Pressing **`DOWN`** or **`BACK`** zooms back in to the Tier 1 canvas.

### 4. Gold Pixels (`GP`) & Prestige
- Completing all 64 solid color grids fills the Tier 2 master grid completely (4,096 total pixels).
- Full completion awards **1 Gold Pixel point (`GP`)**, celebrated with a golden banner. Gold Pixels persist across prestiges.
- **Prestiging**: Accessible via the Menu. Awards **1 cent per completed solid color grid** to spend on permanent upgrades, then restarts Tier 1 and Tier 2. Currency (converting to dollars at 100¢), upgrades, unlocks, and Gold Pixels are always preserved.

### 5. Victory Celebration
- Filling all 64 colors triggers the celebratory victory screen with animated colorful firework explosions over the victory card and comprehensive stats (total time, in-game time, away time, unlocks, and all-time earnings/spending).

---

## 🎨 UI & Design

- **Liquid Wipe Animation**: Each pixel fills smoothly from bottom to top with an illuminated border according to the active fill pattern.
- **Dynamic Auto-Contrasting Background**: By default (`Auto`), the background automatically switches between dark and light modes depending on the luminance of the currently filling color, ensuring optimal visibility.
- **Liquid Ink Boost Visual**: Activating boost causes ink to rise from the bottom of the screen like a sloshing liquid. The ink color dynamically blends between the filling color and the background color.
- **Fuse-and-Shrink Animation**: On completion, the 8x8 grid melds into one solid square block, then smoothly shrinks down and flies into its destination slot on the Tier 2 grid.
- **Accurate Real-Time PPS**: Live pixels-per-second (`px/s`) telemetry calculated directly from current base speed and momentum boost multiplier.

---

## 🕹️ Controls & Navigation

### Main Screen (Tier 1 Canvas)
| Button | Action | Description |
| :--- | :--- | :--- |
| **`UP`** | **Zoom Out** | Open the Tier 2 overview. |
| **`UP (Hold)`** | **Clock Face** | Open the built-in clock face view. |
| **`SELECT`** | **Menu** | Open the main menu (Prestige, Store, Unlocks, Options, Clock). |
| **`DOWN`** | **Boost** | Add filling momentum (+5% base gain, compounds with repeat presses). |
| **`DOWN (Hold)`** | **Auto-Boost** | Rapid auto-press boost at your purchased hold boost rate. |

### Upper Tier Screen (Tier 2 Grid)
| Button | Action | Description |
| :--- | :--- | :--- |
| **`DOWN` / `BACK`** | **Zoom In** | Return to the Tier 1 filling canvas. |
| **`SELECT`** | **Menu** | Open the main menu. |
| **`UP (Hold)`** | **Clock Face** | Open the clock face view. |

### Clock Face Screen
| Button | Action | Description |
| :--- | :--- | :--- |
| **`UP`** | **Cycle BG Color** | Cycle through all unlocked background colors and Auto Contrast. |
| **`SELECT`** | **Cycle Style** | Cycle through unlocked clock styles on the fly. |
| **`DOWN` (Press/Hold)**| **Boost** | Add filling boost with live liquid wave animation behind the clock face. |
| **`BACK`** | **Exit Clock** | Return to the game canvas. |

*Note: While on the clock face, game progression calculates continuously without preview interruptions or idle caps.*

---

## 🌟 Features & Menus

### 🛒 Store (Prestige Upgrades)
Spend prestige currency (cents/dollars) on upgrades with exponential cost scaling:
1. **Auto-Fill Speed**: Accelerates baseline fill interval.
2. **Boost Amount**: Increases momentum gained per button press.
3. **Boost Acceleration**: Increases how fast momentum reaches peak.
4. **Boost Sustain Duration**: Extends the time peak boost holds before decaying.
5. **Boost Diminish Rate**: Slows down the rate at which momentum decays.
6. **Boost Max Limit**: Raises the maximum momentum capacity (screen ceiling).
7. **Hold Press Boost**: Increases auto-click rate when holding down the boost button (+10% clicks/sec/level).
8. **Max Idle Time**: Increases offline idle accumulation limit (+15 min/level).

### 🎨 Cosmetic Unlocks
Spend unlock points earned from all-time grid milestones:
- **Background Colors**: 13 curated colorways + Auto Contrast mode with live preview swatches.
- **Fill Orders**: Normal, Reverse, CW Spiral, CCW Spiral, Zig-Zag, Up and Down, Checkerboard, and Random.
- **Fonts**: Gothic, Bitham, Leco, and Roboto.
- **Clock Styles**: Big Digital, Square Analogue, Bold Minimalist Analogue, 7-Segment Digital, Word Clock, and "Now" Clock.

### ⚙️ Options ("Format & Saves")
- **Time Format**: Toggle 12-Hour vs 24-Hour time.
- **Seconds**: Toggle seconds display across digital clocks (in-line `:SS` / 7-segment digits) and analogue clocks (sweeping second hand). Automatically switches tick subscription to minute units when off for battery efficiency.
- **Vibration**: Toggle haptic feedback on completion and milestones.
- **Save & Load Slots**: 3 enumerated save slots showing creation date, play duration, and crown completion badge. Includes Load, Overwrite, and Clear actions.
- **Reset Game**: Protected confirmation dialog to wipe save data and restart.

### 🕒 Offline Idle Catch-Up
- Automatically calculates progress when returning to the app after 3+ minutes away (up to the purchased offline cap).
- Displays a summary dialog detailing time away, time credited, pixels gained, and fills completed.

---

## 💻 Developer Configuration

To toggle the in-game developer menu (currency and unlock point tickers):
- Open `src/c/game_state.h`
- Set `#define ENABLE_DEV_MENU 1` for local development and testing.
- Set `#define ENABLE_DEV_MENU 0` for release builds to cleanly exclude the developer menu and minimize binary footprint.

---

## 🛠️ Building & Installation

### Requirements
- Pebble SDK (v4.3 or modern RePebble toolchain)
- ARM Embedded GCC (`arm-none-eabi-gcc`)

### Commands
```bash
# Build the .pbw package for basalt, chalk, and emery
pebble build

# Run in Pebble emulator
pebble install --emulator basalt
pebble install --emulator chalk
pebble install --emulator emery

# Install to a physical watch via CloudPebble / phone proxy
pebble install --cloudpebble
pebble install --phone <PHONE_IP_ADDRESS>
```

---

## 📜 License
MIT License.
