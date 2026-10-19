#pragma once

#include <pebble.h>

#define GRID_SIZE 8
#define TOTAL_PIXELS (GRID_SIZE * GRID_SIZE) // 64
#define MAX_FILLS 64

#define INITIAL_BASE_INTERVAL_MS 10000 // 10.0s initial fill speed (0.10 px/s)
#define MIN_BASE_INTERVAL_MS 20        // 20ms safety floor
#define TICK_INTERVAL_MS 100           // 10 Hz game loop

// Build configuration: Set to 1 to enable developer tools & testing menu; set to 0 for release builds.
#ifndef ENABLE_DEV_MENU
#define ENABLE_DEV_MENU 0
#endif

#define SAVE_VERSION 14
#define NUM_SAVE_SLOTS 64
#define PERSIST_KEY_GLOBAL_CONFIG 96
#define PERSIST_KEY_GLOBAL_CROWNS 98
#define PERSIST_KEY_GLOBAL_GOLD_PIXELS 98 // legacy alias
#define PERSIST_KEY_ACTIVE_SLOT 99
#define PERSIST_KEY_GAME_DATA 100
#define PERSIST_KEY_SLOT_BASE 101 // Slots 101 .. 164

typedef struct __attribute__((__packed__)) {
  uint32_t version;
  bool option_24h;
  bool option_show_seconds;
  bool option_vibration;
  bool option_tap_to_wake;

  uint32_t crowns;                  // available crowns bank
  uint32_t total_crowns_earned;     // total crowns earned across all saves
  uint8_t  upgrade_auto_boost_lvl;  // Crown item: 1 auto boost every 5s..1s
  uint8_t  upgrade_clock_boost_lvl; // Crown item: +20% boost on clock per level
  uint8_t  upgrade_headstart_lvl;   // Crown item: +50¢ starting cash & speed
  uint8_t  upgrade_start_level_lvl; // Crown item: starts each upgrade at +1 lvl after prestige
  bool     option_power_saving;     // power saving mode (added at end to preserve offsets)
} GlobalAppData;

// Unlockable cosmetics: 19 background colors, 7 fill orders, 3 fonts, 3 font sizes, 5 clock styles (36 total unlocks)
#define UNLOCK_BG_COLORS_COUNT 19
#define BG_COLOR_AUTO 255
#define UNLOCK_FILL_ORDERS_COUNT 7
#define UNLOCK_FONTS_COUNT 3
#define UNLOCK_FONT_SIZES_COUNT 3
#define UNLOCK_CLOCK_STYLES_COUNT 5
#define TOTAL_MAX_UNLOCKS 36

typedef enum {
  FILL_ORDER_NORMAL = 0,
  FILL_ORDER_REVERSE = 1,
  FILL_ORDER_CW_SPIRAL = 2,
  FILL_ORDER_CCW_SPIRAL = 3,
  FILL_ORDER_ZIG_ZAG = 4,
  FILL_ORDER_UP_AND_DOWN = 5,
  FILL_ORDER_CHECKERBOARD = 6,
  FILL_ORDER_RANDOM = 7,
} FillOrderType;

typedef struct {
  bool should_show;
  uint32_t away_sec;
  uint32_t credited_sec;
  uint32_t pixels_gained;
  uint32_t fills_gained;
} OfflineSummary;

typedef struct __attribute__((__packed__)) {
  uint32_t version;
  uint32_t current_pixels;       // 0 to 63 (pixels in active solid grid)
  uint8_t  current_color_idx;    // 0 to 63 (active solid color filling Tier 1)
  uint8_t  tier2_pixels;         // 0 to 63 (completed solid grids towards Tier 2 grid)
  uint16_t reserved;             // alignment padding
  uint32_t solid_grids_count;    // current run solid grids (resets on prestige, 1¢ each)
  uint32_t crowns;               // total crowns earned (formerly gold_pixels)
  uint32_t max_unlock_milestone; // highest solid grids achieved across runs
  uint32_t progress_ms;          // 0 to interval - 1
  uint32_t currency;             // currency in cents
  uint16_t prestige_level;       // number of prestiges
  uint32_t unlock_points;        // currency to spend on unlocks

  // Store Upgrades
  uint8_t upgrade_fill_speed_lvl;    // multiplicative curve (5000 / (1 + 0.10 * lvl))
  uint8_t upgrade_boost_amount_lvl;  // lvl 0 = +5% gain (+5% per lvl)
  uint8_t upgrade_boost_accel_lvl;   // lvl 0 = 20%/s (+10%/s per lvl)
  uint8_t upgrade_boost_sustain_lvl; // lvl 0 = 0.0s (+500ms per lvl)
  uint8_t upgrade_boost_diminish_lvl;// lvl 0 = 10.0%/s (slower decay per lvl)
  uint8_t upgrade_boost_limit_lvl;   // lvl 0 = 100% max limit (+25% per lvl)
  uint8_t upgrade_offline_cap_lvl;   // lvl 0 = 0m (locked), lvl 1 = 15m (+15m per lvl)
  uint8_t upgrade_hold_boost_lvl;    // +10% clicks/sec per level (base 1.0 click/s)
  uint8_t upgrade_offline_speed_lvl; // closed game speed (base 10%, max lvl 6 = 100%)
  uint8_t upgrade_layer_cents_lvl;   // earn +1¢ per 8 pixels per level (base 1¢/layer)
  uint8_t upgrade_auto_boost_lvl;    // Crown item: 1 auto boost every 5s..1s
  uint8_t upgrade_clock_boost_lvl;   // Crown item: +20% boost on clock per level
  uint8_t upgrade_headstart_lvl;     // Crown item: +50¢ starting cash & speed
  uint8_t upgrade_start_level_lvl;   // Crown item: starts each upgrade at +1 lvl after prestige
  bool    crown_spent;               // true if crown from this completed save has been spent

  // Unlocks & Selections
  uint8_t unlocked_bg_colors;      // 1 to 54 (1 default + up to 53 unlocked)
  uint8_t selected_bg_color_index; // 0 to 63 or BG_COLOR_AUTO

  uint8_t unlocked_fill_orders;    // 1 to 6 (1 default + up to 5 unlocked)
  uint8_t selected_fill_order;     // FillOrderType

  uint8_t unlocked_fonts;          // 1 to 4 (1 default + up to 3 unlocked)
  uint8_t selected_font;           // 0 to 3

  uint8_t unlocked_font_sizes;     // 1 to 3 (1 default + up to 2 unlocked)
  uint8_t selected_font_size;      // 0: Medium, 1: Large, 2: Small

  uint8_t unlocked_clock_styles;   // 1 to 6 (1 default + up to 5 unlocked)
  uint8_t selected_clock_style;    // 0 to 5

  // Options
  bool option_vibration;           // vibration toggle (default true)
  bool option_analog_clock;        // false = digital, true = analog
  bool option_24h;                 // false = 12h, true = 24h
  bool option_show_seconds;        // show seconds in clock (digital or analogue)
  bool option_tap_to_wake;         // wake watch on tap without interaction

  time_t last_save_time;           // epoch seconds

  // Stats & Progress tracking
  time_t   date_started;           // When this save was started
  uint32_t active_play_sec;        // In-game time
  uint32_t offline_play_sec;       // Away time
  uint32_t completion_time_sec;    // Time when 64 colors completed (0 if not yet)
  uint32_t total_currency_earned;  // Lifetime currency earned
  uint32_t total_currency_spent;   // Lifetime currency spent
  bool     is_completed;           // Game won flag
  bool     is_used;                // Slot used flag
  uint64_t favourite_colors;       // 64-bit mask for favourite colors
  bool     option_power_saving;    // power saving mode (added at end to preserve offsets)
} GameSaveData;

#define gold_pixels crowns

typedef enum {
  VIEW_MODE_TIER1 = 0,
  VIEW_MODE_TIER2 = 1,
  VIEW_MODE_PIXELS = 0,
  VIEW_MODE_COLOURS = 1,
} GridViewMode;

typedef struct {
  GameSaveData data;
  uint32_t current_boost_milli;            // current momentum in milli-percent (e.g. 20000 = 20.0%)
  uint32_t target_boost_milli;             // target peak boost
  uint32_t boost_sustain_remaining_ms;     // time remaining holding peak
  uint32_t offline_pixels_gained;
  uint8_t random_grid_order[TOTAL_PIXELS]; // shuffled order for RANDOM fill mode
  OfflineSummary offline_summary;
  GridViewMode view_mode;                  // active view: Pixels (Tier 1) vs Colours (Tier 2)
  bool meld_anim_running;                  // true while meld & grow-to-fill screen animation is running
  uint32_t meld_anim_ms;                   // meld animation elapsed time
  uint8_t meld_anim_color;                 // color index that completed and is melding/growing
  bool crown_just_earned;                  // flag for crown celebration banner
#define gold_pixel_just_earned crown_just_earned

  // Legacy transition compatibility
  bool transition_active;
  bool transition_anim_running;
  uint32_t transition_anim_ms;
  uint8_t transition_completed_color;
  uint32_t preview_background_pixels;      // pixels filled behind completed color preview
  bool is_clock_active;                    // true when clock face is showing
  uint8_t active_slot;                     // 0, 1, or 2
  bool win_just_triggered;                 // true when 64 colors completed
} GameState;

typedef struct {
  bool is_used;
  bool is_completed;
  bool crown_spent;
  time_t date_started;
  uint32_t total_time_sec;
  uint32_t completion_time_sec;
  uint8_t tier2_pixels;
  uint32_t crowns;
} SaveSlotInfo;

void game_state_init(void);
void game_state_save(void);
void game_state_hard_reset(void);

// Currency formatting helper
void game_state_format_currency(uint32_t cents, char *buf, size_t buf_size);

// Save slot management
uint8_t game_state_get_active_slot(void);
void game_state_get_slot_info(uint8_t slot_idx, SaveSlotInfo *out_info);
bool game_state_switch_to_slot(uint8_t slot_idx);
bool game_state_save_to_slot(uint8_t slot_idx);
bool game_state_clear_slot(uint8_t slot_idx);

// View mode controls
GridViewMode game_state_get_view_mode(void);
void game_state_set_view_mode(GridViewMode mode);

// Meld & screen grow animation controls
bool game_state_is_meld_anim_running(void);
uint32_t game_state_get_meld_anim_ms(void);
uint8_t game_state_get_meld_anim_color(void);

// Transition controls (legacy compatibility)
bool game_state_is_transition_active(void);
bool game_state_is_transition_anim_running(void);
void game_state_start_transition_anim(void);
void game_state_finish_transition(void);
uint32_t game_state_get_transition_anim_ms(void);
uint8_t game_state_get_transition_completed_color(void);

// Clock active mode
void game_state_set_clock_active(bool active);
bool game_state_is_clock_active(void);

// Win game trigger
bool game_state_check_and_clear_win_trigger(void);

GColor game_state_get_active_solid_color(void);
GColor game_state_get_ink_color(void);
void game_state_tick(uint32_t dt_ms, bool *out_pixel_added, bool *out_fill_completed);
void game_state_activate_boost(void);
bool game_state_can_prestige(void);
uint32_t game_state_prestige(void);

// Calculated stats
uint32_t game_state_get_base_interval_ms(void);
uint32_t game_state_get_boost_amount_percent(void);
uint32_t game_state_get_boost_accel_percent_per_sec(void);
uint32_t game_state_get_boost_sustain_ms(void);
uint32_t game_state_get_boost_diminish_tenths_per_sec(void);
uint32_t game_state_get_boost_max_limit_percent(void);
uint32_t game_state_get_current_boost_percent(void);
void game_state_get_current_boost_parts(uint32_t *out_whole, uint32_t *out_tenths);
bool game_state_is_boost_at_max(void);
bool game_state_is_boost_animating(void);
uint32_t game_state_get_max_offline_sec(void);
bool game_state_is_offline_unlocked(void);

// Hold boost rate calculation
uint32_t game_state_get_hold_boost_clicks_whole(void);
uint32_t game_state_get_hold_boost_clicks_tenths(void);
uint32_t game_state_get_hold_boost_interval_ms(void);

// Away idle speed percentage
uint32_t game_state_get_offline_speed_percent(void);

// Pixels per second calculation
void game_state_get_pps(uint32_t *out_whole, uint32_t *out_hundredths);

// Fill order mapping
uint8_t game_state_get_order_index(uint8_t row, uint8_t col);

// Exponential cost calculators
uint32_t game_state_calc_cost_1_5x(uint8_t level);
uint32_t game_state_calc_cost_2x(uint8_t level);

typedef enum {
  UPGRADE_FILL_SPEED = 0,
  UPGRADE_BOOST_AMOUNT,
  UPGRADE_BOOST_ACCEL,
  UPGRADE_BOOST_SUSTAIN,
  UPGRADE_BOOST_DIMINISH,
  UPGRADE_BOOST_LIMIT,
  UPGRADE_HOLD_BOOST,
  UPGRADE_OFFLINE_CAP,
  UPGRADE_OFFLINE_SPEED,
  UPGRADE_LAYER_BONUS,
  UPGRADE_AUTO_BOOST,
  UPGRADE_CLOCK_BOOST,
  UPGRADE_STARTING_HEADSTART,
  UPGRADE_START_LEVEL,
  UPGRADE_COUNT
} UpgradeId;

typedef enum {
  COST_CURVE_1_5X = 0,
  COST_CURVE_2X,
  COST_CURVE_CROWN_FLAT,
  COST_CURVE_CROWN_PROGRESSIVE,
} CostCurveType;

typedef enum {
  CURRENCY_TYPE_CENTS = 0,
  CURRENCY_TYPE_CROWNS,
} CurrencyType;

// Unified Data-Driven Upgrade API
uint8_t      game_state_get_upgrade_level(UpgradeId id);
uint32_t     game_state_get_upgrade_cost(UpgradeId id);
CurrencyType game_state_get_upgrade_currency_type(UpgradeId id);
bool         game_state_is_upgrade_maxed(UpgradeId id);
bool         game_state_buy_upgrade(UpgradeId id);
const char*  game_state_get_upgrade_title(UpgradeId id);
void         game_state_format_upgrade_sub(UpgradeId id, char *buf, size_t buf_size);

uint32_t game_state_get_fill_speed_cost(void);
uint32_t game_state_get_boost_amount_cost(void);
uint32_t game_state_get_boost_accel_cost(void);
uint32_t game_state_get_boost_sustain_cost(void);
uint32_t game_state_get_boost_diminish_cost(void);
uint32_t game_state_get_boost_limit_cost(void);
uint32_t game_state_get_offline_cap_cost(void);
uint32_t game_state_get_hold_boost_cost(void);
uint32_t game_state_get_offline_speed_cost(void);

// Store actions
bool game_state_buy_fill_speed(void);
bool game_state_buy_boost_amount(void);
bool game_state_buy_boost_accel(void);
bool game_state_buy_boost_sustain(void);
bool game_state_buy_boost_diminish(void);
bool game_state_buy_boost_limit(void);
bool game_state_buy_offline_cap(void);
bool game_state_buy_hold_boost(void);
bool game_state_buy_offline_speed(void);

// Unlock currency spending actions
uint32_t game_state_get_total_unlocks_count(void);
bool game_state_can_unlock_bg_color(void);
bool game_state_buy_next_bg_color(void);

bool game_state_can_unlock_fill_order(void);
bool game_state_buy_next_fill_order(void);

bool game_state_can_unlock_font(void);
bool game_state_buy_next_font(void);

bool game_state_can_unlock_font_size(void);
bool game_state_buy_next_font_size(void);

bool game_state_can_unlock_clock_style(void);
bool game_state_buy_next_clock_style(void);

GameState* game_state_get(void);
OfflineSummary* game_state_get_offline_summary(void);
GColor game_state_get_pixel_color(uint8_t index);
GColor game_state_get_bg_color(void);
bool game_state_is_color_dark(GColor color);
const char* game_state_get_color_name(uint8_t index);
bool game_state_is_color_favourite(uint8_t index);
void game_state_toggle_color_favourite(uint8_t index);
const char* game_state_get_fill_order_name(FillOrderType order);
const char* game_state_get_font_name(uint8_t font_idx);
const char* game_state_get_font_size_name(uint8_t size_idx);
const char* game_state_get_clock_style_name(uint8_t style_idx);
GFont game_state_get_font(uint8_t font_idx, bool large);
GFont game_state_get_clock_digit_font(uint8_t font_idx, uint8_t size_idx);
GFont game_state_get_clock_text_font(uint8_t font_idx, uint8_t size_idx, uint8_t size_type);
uint8_t game_state_get_unlockable_bg_color(uint8_t index);
const char* game_state_get_unlockable_bg_color_name(uint8_t index);
#if ENABLE_DEV_MENU
void game_state_dev_add_cents(uint32_t cents);
void game_state_dev_add_unlock_points(uint32_t points);
#endif
void game_state_weak_vibe(void);
void game_state_vibe_short(void);
void game_state_vibe_double(void);
void game_state_vibe_long(void);
bool game_state_is_vibration_enabled(void);
bool game_state_get_power_saving(void);
void game_state_set_power_saving(bool enabled);
void game_state_cycle_bg_color(void);

// New layer bonus upgrade
uint32_t game_state_get_layer_bonus_cents(void);
uint32_t game_state_get_layer_bonus_cost(void);
bool game_state_buy_layer_bonus(void);

// Crown store purchases & management
uint32_t game_state_get_available_crowns(void);
uint32_t game_state_get_total_crowns_earned(void);
uint32_t game_state_get_total_crowns_spent(void);
bool game_state_spend_crown(void);
bool game_state_spend_crowns(uint32_t count);
uint32_t game_state_get_auto_boost_cost_crowns(void);
uint32_t game_state_get_auto_boost_interval_ms(void);
bool game_state_buy_auto_boost(void);
uint32_t game_state_get_clock_boost_percent(void);
uint32_t game_state_get_clock_boost_cost_crowns(void);
bool game_state_buy_clock_boost(void);

uint32_t game_state_get_prestige_multiplier_percent(void);
uint32_t game_state_get_prestige_payout(void);
uint32_t game_state_get_starting_headstart_cents(void);
bool game_state_buy_starting_headstart(void);
uint8_t game_state_get_start_level(void);
bool game_state_buy_start_level(void);

uint32_t game_state_get_global_crowns(void);
#if ENABLE_DEV_MENU
void game_state_dev_add_crown(void);
#define game_state_dev_add_gold_pixel game_state_dev_add_crown
#endif
bool game_state_start_new_run_in_slot(uint8_t slot_idx);

#define game_state_get_global_gold_pixels game_state_get_global_crowns
