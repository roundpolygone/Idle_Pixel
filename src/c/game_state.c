#include "game_state.h"

static GameState s_game;

// Precomputed 8x8 Spiral mappings
static const uint8_t s_spiral_cw[8][8] = {
  {  0,  1,  2,  3,  4,  5,  6,  7 },
  { 27, 28, 29, 30, 31, 32, 33,  8 },
  { 26, 47, 48, 49, 50, 51, 34,  9 },
  { 25, 46, 59, 60, 61, 52, 35, 10 },
  { 24, 45, 58, 63, 62, 53, 36, 11 },
  { 23, 44, 57, 56, 55, 54, 37, 12 },
  { 22, 43, 42, 41, 40, 39, 38, 13 },
  { 21, 20, 19, 18, 17, 16, 15, 14 }
};

static const uint8_t s_spiral_ccw[8][8] = {
  {  0, 27, 26, 25, 24, 23, 22, 21 },
  {  1, 28, 47, 46, 45, 44, 43, 20 },
  {  2, 29, 48, 59, 58, 57, 42, 19 },
  {  3, 30, 49, 60, 63, 56, 41, 18 },
  {  4, 31, 50, 61, 62, 55, 40, 17 },
  {  5, 32, 51, 52, 53, 54, 39, 16 },
  {  6, 33, 34, 35, 36, 37, 38, 15 },
  {  7,  8,  9, 10, 11, 12, 13, 14 }
};

static void prv_shuffle_random_order(void) {
  for (int i = 0; i < TOTAL_PIXELS; i++) {
    s_game.random_grid_order[i] = (uint8_t)i;
  }
  for (int i = TOTAL_PIXELS - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    uint8_t temp = s_game.random_grid_order[i];
    s_game.random_grid_order[i] = s_game.random_grid_order[j];
    s_game.random_grid_order[j] = temp;
  }
}

uint32_t game_state_get_base_interval_ms(void) {
  int reduction = (int)s_game.data.upgrade_fill_speed_lvl * 250;
  int interval = (int)INITIAL_BASE_INTERVAL_MS - reduction;
  if (interval < 400) {
    interval = 400; // minimum interval cap
  }
  return (uint32_t)interval;
}

bool game_state_is_boost_unlocked(void) {
  return true; // Available from start (Level 0)
}

uint32_t game_state_get_boost_duration_ms(void) {
  return 1000 + ((uint32_t)s_game.data.upgrade_boost_len_lvl * 250);
}

uint32_t game_state_get_boost_percent(void) {
  return 20 + ((uint32_t)s_game.data.upgrade_boost_power_lvl * 20);
}

bool game_state_is_offline_unlocked(void) {
  return (s_game.data.upgrade_offline_cap_lvl > 0);
}

uint32_t game_state_get_max_offline_sec(void) {
  if (!game_state_is_offline_unlocked()) {
    return 0; // locked at start
  }
  return 900 + ((uint32_t)(s_game.data.upgrade_offline_cap_lvl - 1) * 900); // 15m base + 15m/lvl
}

void game_state_get_pps(uint32_t *out_whole, uint32_t *out_hundredths) {
  uint32_t interval = game_state_get_base_interval_ms();
  uint32_t speed_num = 100;
  if (s_game.boost_remaining_ms > 0) {
    speed_num = 100 + game_state_get_boost_percent();
  }
  uint32_t scaled_pps = (100000 * speed_num) / (interval * 100);
  if (out_whole) {
    *out_whole = scaled_pps / 100;
  }
  if (out_hundredths) {
    *out_hundredths = scaled_pps % 100;
  }
}

uint8_t game_state_get_order_index(uint8_t row, uint8_t col) {
  switch (s_game.data.selected_fill_order) {
    case FILL_ORDER_REVERSE:
      return (uint8_t)(63 - (row * 8 + col));
    case FILL_ORDER_CW_SPIRAL:
      return s_spiral_cw[row][col];
    case FILL_ORDER_CCW_SPIRAL:
      return s_spiral_ccw[row][col];
    case FILL_ORDER_ZIG_ZAG:
      if (row % 2 == 0) {
        return (uint8_t)(row * 8 + col);
      } else {
        return (uint8_t)(row * 8 + (7 - col));
      }
    case FILL_ORDER_RANDOM:
      return s_game.random_grid_order[row * 8 + col];
    case FILL_ORDER_NORMAL:
    default:
      return (uint8_t)(row * 8 + col);
  }
}

void game_state_init(void) {
  memset(&s_game, 0, sizeof(GameState));
  prv_shuffle_random_order();
  bool loaded = false;

  if (persist_exists(PERSIST_KEY_GAME_DATA)) {
    int bytes_read = persist_read_data(PERSIST_KEY_GAME_DATA, &s_game.data, sizeof(GameSaveData));
    if (bytes_read == sizeof(GameSaveData) && s_game.data.version == SAVE_VERSION) {
      loaded = true;
    }
  }

  if (!loaded) {
    s_game.data.version = SAVE_VERSION;
    s_game.data.current_pixels = 0;
    s_game.data.fills_count = 0;
    s_game.data.max_unlock_milestone = 0;
    s_game.data.progress_ms = 0;
    s_game.data.currency = 0;
    s_game.data.prestige_level = 0;
    s_game.data.unlock_points = 0;
    s_game.data.upgrade_fill_speed_lvl = 0;
    s_game.data.upgrade_boost_power_lvl = 0;
    s_game.data.upgrade_boost_len_lvl = 0;    // Level 0: 1.0s, +20% boost
    s_game.data.upgrade_offline_cap_lvl = 0;  // 0 = no idle at start
    s_game.data.unlocked_bg_colors = 1;       // Color 0 unlocked
    s_game.data.selected_bg_color_index = 0;
    s_game.data.unlocked_fill_orders = 1;     // Normal order unlocked
    s_game.data.selected_fill_order = FILL_ORDER_NORMAL;
    s_game.data.unlocked_fonts = 1;           // Gothic unlocked
    s_game.data.selected_font = 0;
    s_game.data.unlocked_clock_styles = 1;    // Big Digital unlocked
    s_game.data.selected_clock_style = 0;
    s_game.data.option_vibration = true;
    s_game.data.option_analog_clock = false;
    s_game.data.option_24h = false;
    s_game.data.last_save_time = time(NULL);
  } else {
    // Process offline idle catch-up
    time_t now = time(NULL);
    if (s_game.data.last_save_time > 0 && now > s_game.data.last_save_time) {
      time_t elapsed_sec = now - s_game.data.last_save_time;
      uint32_t max_offline = game_state_get_max_offline_sec();
      uint32_t credited_sec = (elapsed_sec < (time_t)max_offline) ? (uint32_t)elapsed_sec : max_offline;

      uint32_t interval = game_state_get_base_interval_ms();
      uint64_t total_offline_ms = ((uint64_t)credited_sec * 1000) + s_game.data.progress_ms;
      uint32_t pixels_gained = (uint32_t)(total_offline_ms / interval);
      s_game.data.progress_ms = (uint32_t)(total_offline_ms % interval);

      uint32_t total_pixels = s_game.data.current_pixels + pixels_gained;
      uint32_t fills_gained = total_pixels / TOTAL_PIXELS;
      s_game.data.current_pixels = total_pixels % TOTAL_PIXELS;
      s_game.data.fills_count += fills_gained;

      for (uint32_t f = 0; f < fills_gained; f++) {
        uint32_t current_fill_num = s_game.data.fills_count - fills_gained + 1 + f;
        if (current_fill_num > s_game.data.max_unlock_milestone) {
          s_game.data.max_unlock_milestone = current_fill_num;
          s_game.data.unlock_points++;
        }
      }

      if (elapsed_sec > 180) {
        s_game.offline_summary.should_show = true;
        s_game.offline_summary.away_sec = (uint32_t)elapsed_sec;
        s_game.offline_summary.credited_sec = credited_sec;
        s_game.offline_summary.pixels_gained = pixels_gained;
        s_game.offline_summary.fills_gained = fills_gained;
      }
    }
  }

  s_game.boost_remaining_ms = 0;
}

void game_state_save(void) {
  s_game.data.last_save_time = time(NULL);
  persist_write_data(PERSIST_KEY_GAME_DATA, &s_game.data, sizeof(GameSaveData));
}

void game_state_activate_boost(void) {
  s_game.boost_remaining_ms = game_state_get_boost_duration_ms();
}

void game_state_tick(uint32_t dt_ms, bool *out_pixel_added, bool *out_fill_completed) {
  if (out_pixel_added) {
    *out_pixel_added = false;
  }
  if (out_fill_completed) {
    *out_fill_completed = false;
  }

  uint32_t speed_num = 100;
  if (s_game.boost_remaining_ms > 0) {
    if (s_game.boost_remaining_ms > dt_ms) {
      s_game.boost_remaining_ms -= dt_ms;
    } else {
      s_game.boost_remaining_ms = 0;
    }
    speed_num = 100 + game_state_get_boost_percent();
  }

  s_game.data.progress_ms += (dt_ms * speed_num) / 100;
  uint32_t interval = game_state_get_base_interval_ms();

  while (s_game.data.progress_ms >= interval) {
    s_game.data.progress_ms -= interval;
    s_game.data.current_pixels++;
    if (out_pixel_added) {
      *out_pixel_added = true;
    }
    if (s_game.data.current_pixels >= TOTAL_PIXELS) {
      s_game.data.current_pixels = 0;
      s_game.data.fills_count++;

      // Award unlock point if reaching a new peak milestone
      if (s_game.data.fills_count > s_game.data.max_unlock_milestone) {
        s_game.data.max_unlock_milestone = s_game.data.fills_count;
        s_game.data.unlock_points++;
      }

      prv_shuffle_random_order();
      if (out_fill_completed) {
        *out_fill_completed = true;
      }
    }
  }
}

bool game_state_can_prestige(void) {
  return (s_game.data.fills_count > 0);
}

uint32_t game_state_prestige(void) {
  uint32_t earned = s_game.data.fills_count;
  s_game.data.currency += earned;
  s_game.data.current_pixels = 0;
  s_game.data.fills_count = 0; // resets in current run
  s_game.data.progress_ms = 0;
  s_game.data.prestige_level++;
  prv_shuffle_random_order();
  game_state_save();
  return earned;
}

bool game_state_buy_fill_speed(void) {
  if (s_game.data.currency >= 1) {
    s_game.data.currency -= 1;
    s_game.data.upgrade_fill_speed_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_boost_power(void) {
  if (s_game.data.currency >= 1) {
    s_game.data.currency -= 1;
    s_game.data.upgrade_boost_power_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_boost_len(void) {
  if (s_game.data.currency >= 1) {
    s_game.data.currency -= 1;
    s_game.data.upgrade_boost_len_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_offline_cap(void) {
  if (s_game.data.currency >= 1) {
    s_game.data.currency -= 1;
    s_game.data.upgrade_offline_cap_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

// -------------------------------------------------------------
// Unlock Currency Purchasing
// -------------------------------------------------------------
bool game_state_can_unlock_bg_color(void) {
  return (s_game.data.unlock_points >= 1 && s_game.data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
}

bool game_state_buy_next_bg_color(void) {
  if (game_state_can_unlock_bg_color()) {
    s_game.data.unlock_points -= 1;
    s_game.data.unlocked_bg_colors++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_can_unlock_fill_order(void) {
  return (s_game.data.unlock_points >= 1 && s_game.data.unlocked_fill_orders < UNLOCK_FILL_ORDERS_COUNT);
}

bool game_state_buy_next_fill_order(void) {
  if (game_state_can_unlock_fill_order()) {
    s_game.data.unlock_points -= 1;
    s_game.data.unlocked_fill_orders++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_can_unlock_font(void) {
  return (s_game.data.unlock_points >= 1 && s_game.data.unlocked_fonts < UNLOCK_FONTS_COUNT);
}

bool game_state_buy_next_font(void) {
  if (game_state_can_unlock_font()) {
    s_game.data.unlock_points -= 1;
    s_game.data.unlocked_fonts++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_can_unlock_clock_style(void) {
  return (s_game.data.unlock_points >= 1 && s_game.data.unlocked_clock_styles < UNLOCK_CLOCK_STYLES_COUNT);
}

bool game_state_buy_next_clock_style(void) {
  if (game_state_can_unlock_clock_style()) {
    s_game.data.unlock_points -= 1;
    s_game.data.unlocked_clock_styles++;
    game_state_save();
    return true;
  }
  return false;
}

GameState* game_state_get(void) {
  return &s_game;
}

OfflineSummary* game_state_get_offline_summary(void) {
  return &s_game.offline_summary;
}

GColor game_state_get_pixel_color(uint8_t index) {
  return (GColor8){ .argb = (uint8_t)(0b11000000 | (index & 0x3F)) };
}

GColor game_state_get_bg_color(void) {
  return game_state_get_pixel_color(s_game.data.selected_bg_color_index);
}

bool game_state_is_color_dark(GColor color) {
  uint8_t r = (color.argb >> 4) & 0x03;
  uint8_t g = (color.argb >> 2) & 0x03;
  uint8_t b = color.argb & 0x03;
  int lum = (r * 300) + (g * 590) + (b * 110);
  return (lum < 1400);
}

const char* game_state_get_fill_order_name(FillOrderType order) {
  switch (order) {
    case FILL_ORDER_NORMAL: return "Normal";
    case FILL_ORDER_REVERSE: return "Reverse";
    case FILL_ORDER_CW_SPIRAL: return "CW Spiral";
    case FILL_ORDER_CCW_SPIRAL: return "CCW Spiral";
    case FILL_ORDER_ZIG_ZAG: return "Zig-Zag";
    case FILL_ORDER_RANDOM: return "Random";
    default: return "Normal";
  }
}

const char* game_state_get_font_name(uint8_t font_idx) {
  switch (font_idx) {
    case 0: return "Gothic";
    case 1: return "Bitham";
    case 2: return "Leco";
    case 3: return "Roboto";
    default: return "Gothic";
  }
}

const char* game_state_get_clock_style_name(uint8_t style_idx) {
  switch (style_idx) {
    case 0: return "Big Digital";
    case 1: return "Analog Classic";
    case 2: return "Minimalist";
    case 3: return "Modern Dial";
    default: return "Big Digital";
  }
}

GFont game_state_get_font(uint8_t font_idx, bool large) {
  switch (font_idx) {
    case 1:
      return fonts_get_system_font(large ? FONT_KEY_BITHAM_30_BLACK : FONT_KEY_GOTHIC_18_BOLD);
    case 2:
      return fonts_get_system_font(large ? FONT_KEY_LECO_32_BOLD_NUMBERS : FONT_KEY_GOTHIC_18_BOLD);
    case 3:
      return fonts_get_system_font(large ? FONT_KEY_ROBOTO_CONDENSED_21 : FONT_KEY_GOTHIC_18);
    case 0:
    default:
      return fonts_get_system_font(large ? FONT_KEY_BITHAM_42_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  }
}
