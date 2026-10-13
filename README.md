# Idle_Pixel

An immersive, retro-inspired incremental idle game built in C for Pebble color smartwatches (**Pebble Time / basalt**, **Pebble Time Round / chalk**, and **Pebble Time 2 / emery**).

---

## 🎮 Game Logic & Progression

Idle_Pixel is structured into a multi-tiered incremental progression system:

```
+---+--------------------------+
|   |  COL 1/64 | GP 0         |
|   +--------------------------+
| 6 | [== Mini Progress Bar =] |
| 4 |                          |
|   |  [■][■][■][■][■][■][■][■]|
| S |  [■][■][■][■][■][■][■][■]|
| L |  [■][■][■][▓][ ][ ][ ][ ]|  <-- Tier 1 Liquid Wipe
| I |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| C |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| E |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| S |  [ ][ ][ ][ ][ ][ ][ ][ ]|
|   |  [ ][ ][ ][ ][ ][ ][ ][ ]|
+---+--------------------------+
|  0.20 px/s | ^Tier2 =Menu vBst|
+------------------------------+
```

### 1. Tier 1: Solid Color Canvas (8x8 Grid)
- **The Canvas**: An 8x8 grid (64 pixels) that fills with a single, solid color selected sequentially from the 64-color Pebble hardware palette.
- **Base Speed**: Starts at 5.0 seconds per pixel (0.20 px/s), upgradeable in the Store and boostable with manual button presses.
- **Completion Flow**: When the 64th pixel completes, the watch vibrates once and enters a completion preview showing the solid color canvas. On the **first button press**, the backlight illuminates and triggers the fuse-and-shrink animation into Tier 2. Once complete, Tier 1 automatically begins filling the next color.

### 2. Vertical Progress Gauge (64 Slices)
- Located directly to the left of the Tier 1 grid.
- Divided into 64 vertical slices building from bottom to top.
- Each completed Tier 1 solid grid fills 1/64th of this gauge with its corresponding color.

### 3. Tier 2: The 64-Color Master Grid
- Pressing **`UP`** from the main canvas zooms out to the Tier 2 view.
- Displays all 64 color slots of the full palette:
  - Completed colors appear as solid colored tiles.
  - The currently filling color shows live filling progress matching Tier 1.
  - Future slots remain empty.
- Pressing **`DOWN`** or **`BACK`** zooms back in to the Tier 1 canvas.

### 4. Gold Pixels (`GP`) & Prestige
- Completing all 64 solid color grids fills the Tier 2 master grid completely (4,096 total pixels).
- Full completion awards **1 Gold Pixel point (`GP`)**, celebrated with a golden banner. Gold Pixels are permanent points that persist across prestiges.
- **Prestiging**: Accessible via the Menu. Awards **1 cent per completed solid color grid** to spend on permanent upgrades, then restarts Tier 1 and Tier 2. Currency, upgrades, unlocks, and Gold Pixels are always preserved.

---

## 🎨 UI & Design

- **Liquid Wipe Animation**: Each pixel fills smoothly from bottom to top with an illuminated border.
- **Dynamic Auto-Contrasting Background**: By default (`Auto`), the background automatically switches between dark and light modes depending on the luminance of the currently filling color, ensuring optimal visibility.
- **Liquid Ink Boost Visual**: Activating boost causes ink to rise from the bottom of the screen like a sloshing liquid. The ink color dynamically blends between the filling color and the background color. The fluid level rises and falls with momentum up to the screen's top limit.
- **Fuse-and-Shrink Animation**: On completion, the 8x8 grid melds into one solid square block, then smoothly shrinks down and flies into its destination slot on the Tier 2 grid.
- **Accurate Real-Time PPS**: Live pixels-per-second (`px/s`) telemetry calculated directly from current base speed and momentum boost multiplier.

---

## 🕹️ UX & Controls

### Main Screen (Tier 1 Canvas)
| Button | Action | Description |
| :--- | :--- | :--- |
| **`UP`** | **Zoom Out** | Open the Tier 2 (64-color master grid) overview. |
| **`SELECT`** | **Menu** | Open the main menu (Prestige, Store, Unlocks, Options, Clock). |
| **`DOWN`** | **Boost** | Add filling momentum (+5% base gain, compounds with repeat presses). |
| **`ANY`** | **Start Fuse** | On fill completion: turns on backlight and plays the fuse & shrink animation. |

### Upper Tier Screen (Tier 2 Grid)
| Button | Action | Description |
| :--- | :--- | :--- |
| **`DOWN` / `BACK`** | **Zoom In** | Return to the Tier 1 filling canvas. |
| **`SELECT`** | **Menu** | Open the main menu. |

---

## 🌟 Features & Menus

### 🛒 Store (Prestige Upgrades)
Spend prestige currency (cents) on upgrades with exponential cost scaling:
1. **Auto-Fill Speed**: Accelerates baseline fill interval.
2. **Boost Amount**: Increases momentum gained per button press.
3. **Boost Acceleration**: Increases how fast momentum reaches peak.
4. **Boost Sustain Duration**: Extends the time peak boost holds before decaying.
5. **Boost Diminish Rate**: Slows down the rate at which momentum decays.
6. **Boost Max Limit**: Raises the maximum momentum capacity (screen ceiling).
7. **Max Idle Time**: Increases offline idle accumulation limit (+15 min/level).

### 🎨 Cosmetic Unlocks
Spend unlock points earned from all-time grid milestones:
- **Background Colors**: 64 colors + Auto Contrast mode with live preview swatches.
- **Fill Orders**: Normal, Reverse, Clockwise Spiral, Counter-Clockwise Spiral, Zig-Zag, and Random.
- **Fonts**: Gothic, Bitham, Leco, and Roboto.
- **Clock Styles**: Big Digital, Analog Classic, Minimalist, and Modern Dial.

### ⚙️ Options & Clock
- **Vibration**: Toggle haptic feedback on completion and milestones.
- **Clock Style**: Toggle Digital vs Analog display.
- **Time Format**: Toggle 12h vs 24h clock.
- **Hard Reset**: Protected confirmation dialog to wipe save data and restart.
- **Clock Face**: Built-in clock with live idle stats accessible from the menu.

### 🕒 Offline Idle Catch-Up
- Automatically calculates progress when returning to the app after 3+ minutes away (up to the purchased offline cap).
- Displays a summary dialog detailing time away, time credited, pixels gained, and grids completed.

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

# Install to a physical watch
pebble install --phone <PHONE_IP_ADDRESS>
```

---

## 📜 License
MIT License.
