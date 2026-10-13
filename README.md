# Idle_Pixel

An immersive, retro-inspired incremental idle game built in C for Pebble color smartwatches (**Pebble Time / basalt**, **Pebble Time Round / chalk**, and **Pebble Time 2 / emery**).

---

## 🎮 Concept & Gameplay Overview

In **Idle_Pixel**, the core objective is to progressively fill an 8x8 display canvas one pixel at a time, climb through tiered color grids, earn currency, unlock customizable cosmetics, and collect coveted **Gold Pixels**.

```
+---+--------------------------+
|   |  COL 1/64 | GP 0         |
|   +--------------------------+
| 6 | [== Mini Progress Bar =] |
| 4 |                          |
|   |  [■][■][■][■][■][■][■][■]|
| S |  [■][■][■][■][■][■][■][■]|
| L |  [■][■][■][▓][ ][ ][ ][ ]|  <-- Liquid Wipe Animation
| I |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| C |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| E |  [ ][ ][ ][ ][ ][ ][ ][ ]|
| S |  [ ][ ][ ][ ][ ][ ][ ][ ]|
|   |  [ ][ ][ ][ ][ ][ ][ ][ ]|
+---+--------------------------+
|  0.20 px/s  | ^Clk =Menu vBst|
+------------------------------+
```

---

## ⚙️ Game Logic & Progression Loop

### 1. Tier 1: Base Solid Color Grid
- **The Canvas**: An 8x8 grid (64 pixels) that fills with a single, solid color ($C \in [0, 63]$), progressing sequentially through each color in the Pebble hardware palette.
- **Liquid Wipe Animation**: As progress builds towards each pixel fill tick ($0 \dots \text{interval}$), the active target pixel renders a smooth, bottom-up liquid wipe/rise filling effect with an illuminated outline.
- **Base Speed**: Starts at 5.0 seconds per pixel (0.20 px/s). This speed can be accelerated via manual **Boost** and permanently upgraded in the **Store**.

### 2. Vertical Progress Bar (The 64-Slice Left Gauge)
- Located directly to the left of the 8x8 grid.
- Contains **64 individual vertical color slices** building bottom-up.
- Completing a full 8x8 solid color grid fills 1/64th of the vertical gauge with that solid color.

### 3. Tier 2: The 64-Color Grid & Fuse-and-Shrink Celebration
- **Fusing Animation**: When the 64th pixel of a solid grid completes, all individual cell borders dissolve and the entire 8x8 canvas fuses into **one monolithic, glowing solid block** of that color.
- **Shrinking Down**: The Tier 2 palette appears, and the solid block smoothly shrinks down and glides into its exact target slot on the Tier 2 grid, locking in with an impact pulse.
- **Persistent Preview**: The preview modal does **not** auto-dismiss. It stays on screen indefinitely so you never miss seeing your completed color palette and progression!
- **Protected Dismissal**: To prevent accidental skipping, the preview can only be dismissed by pressing any button (`UP`, `SELECT`, or `DOWN`) **after the shrink animation has finished**, at which point Tier 1 begins filling the next color.

### 4. Gold Pixels (Milestone Currency)
- Completing all 64 solid color grids fills the Tier 2 grid completely ($64 \times 64 = 4,096$ pixels).
- Upon completing Tier 2, the player earns a **Gold Pixel point (`GP`)**, accompanied by a golden celebration banner (`GOLD PIXEL! +1`).
- Gold Pixels are **permanent prestige points** that persist across normal prestiges and will unlock special high-tier upgrades.
- After earning a Gold Pixel, Tier 2 resets to 0 and the cycle begins anew.

### 5. Dynamic Auto-Contrasting Background
- If no custom background color is chosen, the game defaults to **Auto Contrast (`BG_COLOR_AUTO`)**.
- The game continuously assesses the luminance of the actively filling color:
  - **Dark Filling Color** (e.g., Black, Navy, Forest Green, Dark Violet) $\rightarrow$ The background, panel, and empty cell interiors automatically switch to **White**.
  - **Light Filling Color** (e.g., White, Yellow, Cyan, Mint) $\rightarrow$ The background, panel, and empty cell interiors automatically switch to **Black**.
- This guarantees maximum visual contrast so the liquid wipe animation is always crisp and visible.

---

## 🕹️ Controls & Navigation

| Button | Action | Description |
| :--- | :--- | :--- |
| **`UP`** | **Clock Shortcut** | Jump directly to the built-in clock face. |
| **`SELECT`** | **Main Menu** | Open the menu (Prestige, Store, Unlocks, Options, Clock). |
| **`DOWN`** | **Boost** | Accelerate fill rate by +20% (upgradeable) for 1.0s (upgradeable). |
| **`ANY`** | **Skip Modal** | Pressing any button dismisses the Tier 2 celebration popup. |

---

## 🏪 Menus & Economy

### 🔁 Prestige
- **Payout**: Awards **1 cent per completed solid color grid**.
- **Reset**: Resets current run progress (Tier 1 pixels, active color, and Tier 2 progress back to 0).
- **Preserved**: Currency, purchased store upgrades, unlocked cosmetics, and **Gold Pixels** are completely preserved.

### 🛒 Store (Currency Upgrades)
Currency earned from prestiging can be spent on four core upgrades with exponential cost scaling:
1. **Fill Speed**: Multiplicative speed curve ($5000\text{ms} / (1 + 0.10 \times \text{lvl})$).
2. **Boost Power**: Increases boost bonus (+20% per level).
3. **Boost Duration**: Extends boost length (+250ms per level).
4. **Offline Cap**: Increases maximum idle catch-up time (+15 minutes per level, doubling cost).

### 🎨 Unlocks (Cosmetics & Customization)
Reaching new all-time solid grid milestones grants **Unlock Points**, which can be freely spent to unlock customizations:
- **Background Colors**: 64 colors + the default **Auto Contrast** option (featuring an active real-time preview and split swatch).
- **Fill Orders**: Normal, Reverse, Clockwise Spiral, Counter-Clockwise Spiral, Zig-Zag, and Random.
- **Fonts**: Gothic, Bitham, Leco, and Roboto.
- **Clock Styles**: Big Digital, Analog Classic, Minimalist, and Modern Dial.

### ⚙️ Options
- **Vibration**: Toggle haptic feedback on/off for pixel completions and milestones.
- **Clock Mode**: Toggle Digital vs Analog display.
- **Time Format**: Toggle 12h vs 24h clock.
- **Hard Reset**: Protected confirmation dialog to wipe all save data and restart from Day 1.

### 🕒 Offline Idle Catch-Up
- When returning to the watch after being away for more than 3 minutes, the game calculates idle progress up to your purchased offline cap.
- A summary screen reports total credited time, pixels filled, and solid grids completed while away.

---

## 🛠️ Building & Installation

### Requirements
- Pebble SDK (v4.3 or modern RePebble toolchain)
- ARM Embedded GCC (`arm-none-eabi-gcc`)

### Compilation Commands
```bash
# Build the .pbw package for basalt, chalk, and emery
pebble build

# Run in emulator
pebble install --emulator basalt
pebble install --emulator chalk
pebble install --emulator emery

# Install to a paired physical Pebble watch
pebble install --phone <PHONE_IP_ADDRESS>
```

---

## 📂 Source Architecture

```
Idle_Pixel/
├── src/c/
│   ├── Idle_Pixel.c          # Entry point, event loop, click config
│   ├── game_state.h / .c     # Core logic, math curves, save data, prestige
│   ├── ui_grid.h / .c        # 8x8 Tier 1 canvas, liquid wipe, vertical bar, popup
│   ├── ui_menu.h / .c        # Hub menu navigation
│   ├── ui_store.h / .c       # Store upgrades with sticky balance header
│   ├── ui_unlocks.h / .c     # Cosmetics shop with real-time previews
│   ├── ui_prestige.h / .c    # Prestige calculation and confirmation dialog
│   ├── ui_options.h / .c     # Settings & hard reset modal
│   ├── ui_clock.h / .c       # Digital/analog clock face with live idle stats
│   └── ui_offline_summary.h  # Offline catch-up report modal
├── appinfo.json / package.json
└── README.md
```

---

## 📜 License
MIT License. Crafted with ❤️ for Pebble enthusiasts.
