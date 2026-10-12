#pragma once

#include <pebble.h>

#define GRID_SIZE 8
#define TOTAL_PIXELS (GRID_SIZE * GRID_SIZE) // 64
#define MAX_FILLS 64

#define INITIAL_BASE_INTERVAL_MS 5000 // 5.0s initial fill speed
#define TICK_INTERVAL_MS 100          // 10 Hz game loop

#define SAVE_VERSION 4
#define PERSIST_KEY_GAME_DATA 100

#define UNLOCK_BG_COLORS_COUNT 64
#define UNLOCK_FILL_ORDERS_COUNT 6
#define UNLOCK_FONTS_COUNT 4
#define UNLOCK_CLOCK_STYLES_COUNT 4

typedef enum {
  FILL_ORDER_NORMAL = 0,
  FILL_ORDER_REVERSE = 1,
  FILL_ORDER_CW_SPIRAL = 2,
  FILL_ORDER_CCW_SPIRAL = 3,
  FILL_ORDER_ZIG_ZAG = 4,
  FILL_ORDER_RANDOM = 5,
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
  uint32_t current_pixels;       // 0 to 63
  uint32_t fills_count;          // current run fills (resets on prestige)
  uint32_t max_unlock_milestone; // highest fills achieved across runs
  uint32_t progress_ms;          // 0 to interval - 1
  uint32_t currency;             // currency in cents
  uint16_t prestige_level;       // number of prestiges
  uint32_t unlock_points;        // currency to spend on unlocks

  // Store Upgrades
  uint8_t upgrade_fill_speed_lvl;  // lvl 0 = 5000ms (-250ms per lvl, min 400ms)
  uint8_t upgrade_boost_power_lvl; // lvl 0 = +20% boost (+20% per lvl)
  uint8_t upgrade_boost_len_lvl;   // lvl 0 = 1.0s (+250ms per lvl)
  uint8_t upgrade_offline_cap_lvl; // lvl 0 = 0m (locked), lvl 1 = 15m (+15m per lvl)

  // Unlocks & Selections
  uint8_t unlocked_bg_colors;      // 1 to 64
  uint8_t selected_bg_color_index; // 0 to 63 (ARGB)

  uint8_t unlocked_fill_orders;    // 1 to 6
  uint8_t selected_fill_order;     // FillOrderType

  uint8_t unlocked_fonts;          // 1 to 4
  uint8_t selected_font;           // 0 to 3

  uint8_t unlocked_clock_styles;   // 1 to 4
  uint8_t selected_clock_style;    // 0 to 3

  // Options
  bool option_vibration;           // vibration toggle (default true)
  bool option_analog_clock;        // false = digital, true = analog
  bool option_24h;                 // false = 12h, true = 24h

  time_t last_save_time;           // epoch seconds
} GameSaveData;

typedef struct {
  GameSaveData data;
  uint32_t boost_remaining_ms;
  uint32_t offline_pixels_gained;
  uint8_t random_grid_order[TOTAL_PIXELS]; // shuffled order for RANDOM fill mode
  OfflineSummary offline_summary;
} GameState;

void game_state_init(void);
void game_state_save(void);
void game_state_tick(uint32_t dt_ms, bool *out_pixel_added, bool *out_fill_completed);
void game_state_activate_boost(void);
bool game_state_can_prestige(void);
uint32_t game_state_prestige(void);

// Calculated stats
uint32_t game_state_get_base_interval_ms(void);
uint32_t game_state_get_boost_duration_ms(void);
uint32_t game_state_get_boost_percent(void);
uint32_t game_state_get_max_offline_sec(void);
bool game_state_is_boost_unlocked(void);
bool game_state_is_offline_unlocked(void);

// Pixels per second calculation
void game_state_get_pps(uint32_t *out_whole, uint32_t *out_hundredths);

// Fill order mapping
uint8_t game_state_get_order_index(uint8_t row, uint8_t col);

// Store actions
bool game_state_buy_fill_speed(void);
bool game_state_buy_boost_power(void);
bool game_state_buy_boost_len(void);
bool game_state_buy_offline_cap(void);

// Unlock currency spending actions
bool game_state_can_unlock_bg_color(void);
bool game_state_buy_next_bg_color(void);

bool game_state_can_unlock_fill_order(void);
bool game_state_buy_next_fill_order(void);

bool game_state_can_unlock_font(void);
bool game_state_buy_next_font(void);

bool game_state_can_unlock_clock_style(void);
bool game_state_buy_next_clock_style(void);

GameState* game_state_get(void);
OfflineSummary* game_state_get_offline_summary(void);
GColor game_state_get_pixel_color(uint8_t index);
GColor game_state_get_bg_color(void);
bool game_state_is_color_dark(GColor color);
const char* game_state_get_fill_order_name(FillOrderType order);
const char* game_state_get_font_name(uint8_t font_idx);
const char* game_state_get_clock_style_name(uint8_t style_idx);
GFont game_state_get_font(uint8_t font_idx, bool large);
