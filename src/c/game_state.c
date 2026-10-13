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
  // Interval = 5000 / (1 + 0.10 * level) = 50000 / (10 + level)
  uint32_t interval = 50000 / (10 + (uint32_t)s_game.data.upgrade_fill_speed_lvl);
  if (interval < MIN_BASE_INTERVAL_MS) {
    interval = MIN_BASE_INTERVAL_MS; // 500ms safety floor
  }
  return interval;
}

uint32_t game_state_get_boost_amount_percent(void) {
  // Lvl 0 = 5%, +5% per lvl
  return 5 + ((uint32_t)s_game.data.upgrade_boost_amount_lvl * 5);
}

uint32_t game_state_get_boost_accel_percent_per_sec(void) {
  // Lvl 0 = 20%/s, +10%/s per lvl
  return 20 + ((uint32_t)s_game.data.upgrade_boost_accel_lvl * 10);
}

uint32_t game_state_get_boost_sustain_ms(void) {
  // Lvl 0 = 0ms, +500ms per lvl
  return (uint32_t)s_game.data.upgrade_boost_sustain_lvl * 500;
}

uint32_t game_state_get_boost_diminish_tenths_per_sec(void) {
  // Lvl 0 = 10.0%/s, decays slower per lvl: 1000 / (10 + 2 * lvl)
  return 1000 / (10 + ((uint32_t)s_game.data.upgrade_boost_diminish_lvl * 2));
}

uint32_t game_state_get_boost_max_limit_percent(void) {
  // Lvl 0 = 100%, +25% per lvl
  return 100 + ((uint32_t)s_game.data.upgrade_boost_limit_lvl * 25);
}

uint32_t game_state_get_current_boost_percent(void) {
  return s_game.current_boost_milli / 1000;
}

void game_state_get_current_boost_parts(uint32_t *out_whole, uint32_t *out_tenths) {
  if (out_whole) {
    *out_whole = s_game.current_boost_milli / 1000;
  }
  if (out_tenths) {
    *out_tenths = (s_game.current_boost_milli % 1000) / 100;
  }
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
  if (interval == 0) interval = 1;
  uint64_t mult = 100000ULL + s_game.current_boost_milli;
  // True PPS in hundredths = (1000 * 100 * mult) / (interval * 100000) = mult / interval
  uint64_t scaled_pps = mult / (uint64_t)interval;
  if (out_whole) {
    *out_whole = (uint32_t)(scaled_pps / 100);
  }
  if (out_hundredths) {
    *out_hundredths = (uint32_t)(scaled_pps % 100);
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

// -------------------------------------------------------------
// Exponential Cost Scaling
// -------------------------------------------------------------
uint32_t game_state_calc_cost_1_5x(uint8_t level) {
  uint32_t cost = 1;
  for (uint8_t i = 0; i < level; i++) {
    cost = (cost * 3 + 1) / 2; // iterative 1.5x with rounding
  }
  return cost;
}

uint32_t game_state_calc_cost_2x(uint8_t level) {
  if (level >= 30) return 1000000000;
  return (uint32_t)(1UL << level);
}

uint32_t game_state_get_fill_speed_cost(void) {
  return game_state_calc_cost_1_5x(s_game.data.upgrade_fill_speed_lvl);
}

uint32_t game_state_get_boost_amount_cost(void) {
  return game_state_calc_cost_1_5x(s_game.data.upgrade_boost_amount_lvl);
}

uint32_t game_state_get_boost_accel_cost(void) {
  return game_state_calc_cost_1_5x(s_game.data.upgrade_boost_accel_lvl);
}

uint32_t game_state_get_boost_sustain_cost(void) {
  return game_state_calc_cost_1_5x(s_game.data.upgrade_boost_sustain_lvl);
}

uint32_t game_state_get_boost_diminish_cost(void) {
  return game_state_calc_cost_1_5x(s_game.data.upgrade_boost_diminish_lvl);
}

uint32_t game_state_get_boost_limit_cost(void) {
  return game_state_calc_cost_1_5x(s_game.data.upgrade_boost_limit_lvl);
}

uint32_t game_state_get_offline_cap_cost(void) {
  return game_state_calc_cost_2x(s_game.data.upgrade_offline_cap_lvl);
}

void game_state_init(void) {
  memset(&s_game, 0, sizeof(GameState));
  prv_shuffle_random_order();
  bool loaded = false;

  if (persist_exists(PERSIST_KEY_GAME_DATA)) {
    int bytes_read = persist_read_data(PERSIST_KEY_GAME_DATA, &s_game.data, sizeof(GameSaveData));
    if (bytes_read == sizeof(GameSaveData) && s_game.data.version == SAVE_VERSION) {
      loaded = true;
    } else if (bytes_read > 0) {
      uint32_t old_version = 0;
      persist_read_data(PERSIST_KEY_GAME_DATA, &old_version, sizeof(uint32_t));
      if (old_version == 6) {
        struct __attribute__((__packed__)) {
          uint32_t version;
          uint32_t current_pixels;
          uint8_t  current_color_idx;
          uint8_t  tier2_pixels;
          uint16_t reserved;
          uint32_t solid_grids_count;
          uint32_t gold_pixels;
          uint32_t max_unlock_milestone;
          uint32_t progress_ms;
          uint32_t currency;
          uint16_t prestige_level;
          uint32_t unlock_points;
          uint8_t  upgrade_fill_speed_lvl;
          uint8_t  upgrade_boost_amount_lvl;
          uint8_t  upgrade_boost_accel_lvl;
          uint8_t  upgrade_boost_sustain_lvl;
          uint8_t  upgrade_boost_diminish_lvl;
          uint8_t  upgrade_offline_cap_lvl;
          uint8_t  unlocked_bg_colors;
          uint8_t  selected_bg_color_index;
          uint8_t  unlocked_fill_orders;
          uint8_t  selected_fill_order;
          uint8_t  unlocked_fonts;
          uint8_t  selected_font;
          uint8_t  unlocked_clock_styles;
          uint8_t  selected_clock_style;
          bool     option_vibration;
          bool     option_analog_clock;
          bool     option_24h;
          time_t   last_save_time;
        } v6_save;
        if (persist_read_data(PERSIST_KEY_GAME_DATA, &v6_save, sizeof(v6_save)) >= (int)sizeof(v6_save)) {
          s_game.data.current_pixels = v6_save.current_pixels;
          s_game.data.current_color_idx = v6_save.current_color_idx;
          s_game.data.tier2_pixels = v6_save.tier2_pixels;
          s_game.data.solid_grids_count = v6_save.solid_grids_count;
          s_game.data.gold_pixels = v6_save.gold_pixels;
          s_game.data.max_unlock_milestone = v6_save.max_unlock_milestone;
          s_game.data.progress_ms = v6_save.progress_ms;
          s_game.data.currency = v6_save.currency;
          s_game.data.prestige_level = v6_save.prestige_level;
          s_game.data.unlock_points = v6_save.unlock_points;
          s_game.data.upgrade_fill_speed_lvl = v6_save.upgrade_fill_speed_lvl;
          s_game.data.upgrade_boost_amount_lvl = v6_save.upgrade_boost_amount_lvl;
          s_game.data.upgrade_boost_accel_lvl = v6_save.upgrade_boost_accel_lvl;
          s_game.data.upgrade_boost_sustain_lvl = v6_save.upgrade_boost_sustain_lvl;
          s_game.data.upgrade_boost_diminish_lvl = v6_save.upgrade_boost_diminish_lvl;
          s_game.data.upgrade_boost_limit_lvl = 0;
          s_game.data.upgrade_offline_cap_lvl = v6_save.upgrade_offline_cap_lvl;
          s_game.data.unlocked_bg_colors = v6_save.unlocked_bg_colors;
          s_game.data.selected_bg_color_index = v6_save.selected_bg_color_index;
          s_game.data.unlocked_fill_orders = v6_save.unlocked_fill_orders;
          s_game.data.selected_fill_order = v6_save.selected_fill_order;
          s_game.data.unlocked_fonts = v6_save.unlocked_fonts;
          s_game.data.selected_font = v6_save.selected_font;
          s_game.data.unlocked_clock_styles = v6_save.unlocked_clock_styles;
          s_game.data.selected_clock_style = v6_save.selected_clock_style;
          s_game.data.option_vibration = v6_save.option_vibration;
          s_game.data.option_analog_clock = v6_save.option_analog_clock;
          s_game.data.option_24h = v6_save.option_24h;
          s_game.data.last_save_time = v6_save.last_save_time;
          s_game.data.version = SAVE_VERSION;
          loaded = true;
        }
      } else if (old_version == 5) {
        struct __attribute__((__packed__)) {
          uint32_t version;
          uint32_t current_pixels;
          uint8_t  current_color_idx;
          uint8_t  tier2_pixels;
          uint16_t reserved;
          uint32_t solid_grids_count;
          uint32_t gold_pixels;
          uint32_t max_unlock_milestone;
          uint32_t progress_ms;
          uint32_t currency;
          uint16_t prestige_level;
          uint32_t unlock_points;
          uint8_t  upgrade_fill_speed_lvl;
          uint8_t  upgrade_boost_power_lvl;
          uint8_t  upgrade_boost_len_lvl;
          uint8_t  upgrade_offline_cap_lvl;
          uint8_t  unlocked_bg_colors;
          uint8_t  selected_bg_color_index;
          uint8_t  unlocked_fill_orders;
          uint8_t  selected_fill_order;
          uint8_t  unlocked_fonts;
          uint8_t  selected_font;
          uint8_t  unlocked_clock_styles;
          uint8_t  selected_clock_style;
          bool     option_vibration;
          bool     option_analog_clock;
          bool     option_24h;
          time_t   last_save_time;
        } v5_save;
        if (persist_read_data(PERSIST_KEY_GAME_DATA, &v5_save, sizeof(v5_save)) >= (int)sizeof(v5_save)) {
          s_game.data.current_pixels = v5_save.current_pixels;
          s_game.data.current_color_idx = v5_save.current_color_idx;
          s_game.data.tier2_pixels = v5_save.tier2_pixels;
          s_game.data.solid_grids_count = v5_save.solid_grids_count;
          s_game.data.gold_pixels = v5_save.gold_pixels;
          s_game.data.max_unlock_milestone = v5_save.max_unlock_milestone;
          s_game.data.progress_ms = v5_save.progress_ms;
          s_game.data.currency = v5_save.currency;
          s_game.data.prestige_level = v5_save.prestige_level;
          s_game.data.unlock_points = v5_save.unlock_points;
          s_game.data.upgrade_fill_speed_lvl = v5_save.upgrade_fill_speed_lvl;
          s_game.data.upgrade_boost_amount_lvl = v5_save.upgrade_boost_power_lvl;
          s_game.data.upgrade_boost_accel_lvl = 0;
          s_game.data.upgrade_boost_sustain_lvl = v5_save.upgrade_boost_len_lvl;
          s_game.data.upgrade_boost_diminish_lvl = 0;
          s_game.data.upgrade_boost_limit_lvl = 0;
          s_game.data.upgrade_offline_cap_lvl = v5_save.upgrade_offline_cap_lvl;
          s_game.data.unlocked_bg_colors = v5_save.unlocked_bg_colors;
          s_game.data.selected_bg_color_index = v5_save.selected_bg_color_index;
          s_game.data.unlocked_fill_orders = v5_save.unlocked_fill_orders;
          s_game.data.selected_fill_order = v5_save.selected_fill_order;
          s_game.data.unlocked_fonts = v5_save.unlocked_fonts;
          s_game.data.selected_font = v5_save.selected_font;
          s_game.data.unlocked_clock_styles = v5_save.unlocked_clock_styles;
          s_game.data.selected_clock_style = v5_save.selected_clock_style;
          s_game.data.option_vibration = v5_save.option_vibration;
          s_game.data.option_analog_clock = v5_save.option_analog_clock;
          s_game.data.option_24h = v5_save.option_24h;
          s_game.data.last_save_time = v5_save.last_save_time;
          s_game.data.version = SAVE_VERSION;
          loaded = true;
        }
      } else if (old_version == 4) {
        // Read old struct prefix
        struct __attribute__((__packed__)) {
          uint32_t version;
          uint32_t current_pixels;
          uint32_t fills_count;
          uint32_t max_unlock_milestone;
          uint32_t progress_ms;
          uint32_t currency;
          uint16_t prestige_level;
          uint32_t unlock_points;
        } v4_header;
        if (persist_read_data(PERSIST_KEY_GAME_DATA, &v4_header, sizeof(v4_header)) >= (int)sizeof(v4_header)) {
          s_game.data.current_pixels = v4_header.current_pixels % TOTAL_PIXELS;
          s_game.data.solid_grids_count = v4_header.fills_count;
          s_game.data.current_color_idx = (uint8_t)(v4_header.fills_count % 64);
          s_game.data.tier2_pixels = (uint8_t)(v4_header.fills_count % 64);
          s_game.data.gold_pixels = v4_header.fills_count / 64;
          s_game.data.max_unlock_milestone = v4_header.max_unlock_milestone;
          s_game.data.progress_ms = v4_header.progress_ms;
          s_game.data.currency = v4_header.currency;
          s_game.data.prestige_level = v4_header.prestige_level;
          s_game.data.unlock_points = v4_header.unlock_points;
          s_game.data.version = SAVE_VERSION;
          loaded = true;
        }
      }
    }
  }

  if (!loaded) {
    s_game.data.version = SAVE_VERSION;
    s_game.data.current_pixels = 0;
    s_game.data.current_color_idx = 0;
    s_game.data.tier2_pixels = 0;
    s_game.data.solid_grids_count = 0;
    s_game.data.gold_pixels = 0;
    s_game.data.max_unlock_milestone = 0;
    s_game.data.progress_ms = 0;
    s_game.data.currency = 0;
    s_game.data.prestige_level = 0;
    s_game.data.unlock_points = 0;
    s_game.data.upgrade_fill_speed_lvl = 0;
    s_game.data.upgrade_boost_amount_lvl = 0;   // Level 0: +5% gain
    s_game.data.upgrade_boost_accel_lvl = 0;    // Level 0: 20%/s accel
    s_game.data.upgrade_boost_sustain_lvl = 0;  // Level 0: 0s sustain
    s_game.data.upgrade_boost_diminish_lvl = 0; // Level 0: 10%/s decay
    s_game.data.upgrade_boost_limit_lvl = 0;    // Level 0: 100% max limit
    s_game.data.upgrade_offline_cap_lvl = 0;    // 0 = no idle at start
    s_game.data.unlocked_bg_colors = 1;         // Color 0 unlocked
    s_game.data.selected_bg_color_index = BG_COLOR_AUTO; // Default auto-contrasting background
    s_game.data.unlocked_fill_orders = 1;       // Normal order unlocked
    s_game.data.selected_fill_order = FILL_ORDER_NORMAL;
    s_game.data.unlocked_fonts = 1;             // Gothic unlocked
    s_game.data.selected_font = 0;
    s_game.data.unlocked_clock_styles = 1;      // Big Digital unlocked
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
      uint32_t solid_grids_gained = total_pixels / TOTAL_PIXELS;
      s_game.data.current_pixels = total_pixels % TOTAL_PIXELS;
      s_game.data.solid_grids_count += solid_grids_gained;

      for (uint32_t f = 0; f < solid_grids_gained; f++) {
        uint32_t current_solid_num = s_game.data.solid_grids_count - solid_grids_gained + 1 + f;
        if (current_solid_num > s_game.data.max_unlock_milestone) {
          s_game.data.max_unlock_milestone = current_solid_num;
          s_game.data.unlock_points++;
        }
      }

      uint32_t total_tier2 = s_game.data.tier2_pixels + solid_grids_gained;
      s_game.data.gold_pixels += (total_tier2 / 64);
      s_game.data.tier2_pixels = (uint8_t)(total_tier2 % 64);
      s_game.data.current_color_idx = (uint8_t)((s_game.data.current_color_idx + solid_grids_gained) % 64);

      if (elapsed_sec > 180) {
        s_game.offline_summary.should_show = true;
        s_game.offline_summary.away_sec = (uint32_t)elapsed_sec;
        s_game.offline_summary.credited_sec = credited_sec;
        s_game.offline_summary.pixels_gained = pixels_gained;
        s_game.offline_summary.fills_gained = solid_grids_gained;
      }
    }

    if (s_game.data.selected_bg_color_index == 0 && s_game.data.unlocked_bg_colors <= 1) {
      s_game.data.selected_bg_color_index = BG_COLOR_AUTO;
    }
  }

  s_game.current_boost_milli = 0;
  s_game.target_boost_milli = 0;
  s_game.boost_sustain_remaining_ms = 0;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.transition_anim_ms = 0;
  s_game.transition_completed_color = 0;
  s_game.gold_pixel_just_earned = false;
}

void game_state_save(void) {
  s_game.data.last_save_time = time(NULL);
  persist_write_data(PERSIST_KEY_GAME_DATA, &s_game.data, sizeof(GameSaveData));
}

void game_state_hard_reset(void) {
  persist_delete(PERSIST_KEY_GAME_DATA);
  memset(&s_game, 0, sizeof(GameState));
  prv_shuffle_random_order();

  s_game.data.version = SAVE_VERSION;
  s_game.data.current_pixels = 0;
  s_game.data.current_color_idx = 0;
  s_game.data.tier2_pixels = 0;
  s_game.data.solid_grids_count = 0;
  s_game.data.gold_pixels = 0;
  s_game.data.max_unlock_milestone = 0;
  s_game.data.progress_ms = 0;
  s_game.data.currency = 0;
  s_game.data.prestige_level = 0;
  s_game.data.unlock_points = 0;
  s_game.data.upgrade_fill_speed_lvl = 0;
  s_game.data.upgrade_boost_amount_lvl = 0;
  s_game.data.upgrade_boost_accel_lvl = 0;
  s_game.data.upgrade_boost_sustain_lvl = 0;
  s_game.data.upgrade_boost_diminish_lvl = 0;
  s_game.data.upgrade_boost_limit_lvl = 0;
  s_game.data.upgrade_offline_cap_lvl = 0;
  s_game.data.unlocked_bg_colors = 1;
  s_game.data.selected_bg_color_index = BG_COLOR_AUTO;
  s_game.data.unlocked_fill_orders = 1;
  s_game.data.selected_fill_order = FILL_ORDER_NORMAL;
  s_game.data.unlocked_fonts = 1;
  s_game.data.selected_font = 0;
  s_game.data.unlocked_clock_styles = 1;
  s_game.data.selected_clock_style = 0;
  s_game.data.option_vibration = true;
  s_game.data.option_analog_clock = false;
  s_game.data.option_24h = false;
  s_game.data.last_save_time = time(NULL);

  s_game.current_boost_milli = 0;
  s_game.target_boost_milli = 0;
  s_game.boost_sustain_remaining_ms = 0;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.transition_anim_ms = 0;
  s_game.transition_completed_color = 0;
  s_game.gold_pixel_just_earned = false;
  s_game.offline_summary.should_show = false;

  game_state_save();
}

GridViewMode game_state_get_view_mode(void) {
  return s_game.view_mode;
}

void game_state_set_view_mode(GridViewMode mode) {
  s_game.view_mode = mode;
}

bool game_state_is_transition_active(void) {
  return s_game.transition_active;
}

bool game_state_is_transition_anim_running(void) {
  return s_game.transition_anim_running;
}

void game_state_start_transition_anim(void) {
  s_game.transition_anim_running = true;
  s_game.transition_anim_ms = 0;
}

void game_state_finish_transition(void) {
  s_game.data.current_pixels = 0;
  s_game.data.solid_grids_count++;
  s_game.data.tier2_pixels++;

  if (s_game.data.solid_grids_count > s_game.data.max_unlock_milestone) {
    s_game.data.max_unlock_milestone = s_game.data.solid_grids_count;
    s_game.data.unlock_points++;
  }

  if (s_game.data.tier2_pixels >= 64) {
    s_game.data.tier2_pixels = 0;
    s_game.data.gold_pixels++;
    s_game.gold_pixel_just_earned = true;
  }

  s_game.data.current_color_idx = (s_game.data.current_color_idx + 1) % 64;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.transition_anim_ms = 0;

  prv_shuffle_random_order();
  game_state_save();
}

uint32_t game_state_get_transition_anim_ms(void) {
  return s_game.transition_anim_ms;
}

uint8_t game_state_get_transition_completed_color(void) {
  return s_game.transition_completed_color;
}

GColor game_state_get_active_solid_color(void) {
  return game_state_get_pixel_color(s_game.data.current_color_idx);
}

GColor game_state_get_ink_color(void) {
  GColor fill_col = game_state_get_active_solid_color();
  GColor bg_col = game_state_get_bg_color();

  uint8_t r_fill = (fill_col.argb >> 4) & 0x03;
  uint8_t g_fill = (fill_col.argb >> 2) & 0x03;
  uint8_t b_fill = fill_col.argb & 0x03;

  uint8_t r_bg = (bg_col.argb >> 4) & 0x03;
  uint8_t g_bg = (bg_col.argb >> 2) & 0x03;
  uint8_t b_bg = bg_col.argb & 0x03;

  // Intermediate color between fill color and background color
  uint8_t r_ink = (uint8_t)((r_fill + r_bg) / 2);
  uint8_t g_ink = (uint8_t)((g_fill + g_bg) / 2);
  uint8_t b_ink = (uint8_t)((b_fill + b_bg) / 2);

  if (r_ink == r_bg && g_ink == g_bg && b_ink == b_bg && fill_col.argb != bg_col.argb) {
    r_ink = (uint8_t)((r_fill + r_bg + 1) / 2);
    g_ink = (uint8_t)((g_fill + g_bg + 1) / 2);
    b_ink = (uint8_t)((b_fill + b_bg + 1) / 2);
  }

  return (GColor8){ .argb = (uint8_t)(0b11000000 | (r_ink << 4) | (g_ink << 2) | b_ink) };
}

void game_state_activate_boost(void) {
  // Adds filling momentum to the fill power.
  // Compounds effect taking over the increase at current boost state.
  uint32_t amount_milli = game_state_get_boost_amount_percent() * 1000;
  uint32_t max_milli = game_state_get_boost_max_limit_percent() * 1000;

  if (s_game.current_boost_milli >= max_milli) {
    s_game.current_boost_milli = max_milli;
    s_game.target_boost_milli = max_milli;
    s_game.boost_sustain_remaining_ms = game_state_get_boost_sustain_ms();
    return;
  }

  s_game.target_boost_milli = s_game.current_boost_milli + amount_milli;
  if (s_game.target_boost_milli > max_milli) {
    s_game.target_boost_milli = max_milli;
  }
  s_game.boost_sustain_remaining_ms = 0;
}

void game_state_tick(uint32_t dt_ms, bool *out_pixel_added, bool *out_fill_completed) {
  if (out_pixel_added) {
    *out_pixel_added = false;
  }
  if (out_fill_completed) {
    *out_fill_completed = false;
  }

  // Handle Tier 2 completion preview and animation
  if (s_game.transition_active) {
    if (!s_game.transition_anim_running) {
      // Waiting for user button press to illuminate watch & start meld/shrink animation
      return;
    }
    s_game.transition_anim_ms += dt_ms;
    if (s_game.transition_anim_ms >= 1600) {
      game_state_finish_transition();
    }
    return;
  }

  uint32_t max_milli = game_state_get_boost_max_limit_percent() * 1000;
  if (s_game.target_boost_milli > max_milli) {
    s_game.target_boost_milli = max_milli;
  }
  if (s_game.current_boost_milli > max_milli) {
    s_game.current_boost_milli = max_milli;
  }

  // Update boost momentum physics
  if (s_game.current_boost_milli < s_game.target_boost_milli) {
    // Accelerating towards target peak
    uint32_t accel_milli_sec = game_state_get_boost_accel_percent_per_sec() * 1000;
    uint32_t accel_step = (accel_milli_sec * dt_ms) / 1000;
    if (s_game.current_boost_milli + accel_step >= s_game.target_boost_milli) {
      s_game.current_boost_milli = s_game.target_boost_milli;
      s_game.boost_sustain_remaining_ms = game_state_get_boost_sustain_ms();
    } else {
      s_game.current_boost_milli += accel_step;
    }
  } else if (s_game.boost_sustain_remaining_ms > 0) {
    // Holding peak speed for sustain duration
    if (s_game.boost_sustain_remaining_ms > dt_ms) {
      s_game.boost_sustain_remaining_ms -= dt_ms;
    } else {
      s_game.boost_sustain_remaining_ms = 0;
    }
  } else if (s_game.current_boost_milli > 0) {
    // Diminishing speed at diminishing rate
    uint32_t decay_tenths_sec = game_state_get_boost_diminish_tenths_per_sec();
    uint32_t decay_milli_sec = decay_tenths_sec * 100; // tenths to milli (e.g. 100 tenths = 10,000 milli = 10.0%/s)
    uint32_t decay_step = (decay_milli_sec * dt_ms) / 1000;
    if (s_game.current_boost_milli > decay_step) {
      s_game.current_boost_milli -= decay_step;
    } else {
      s_game.current_boost_milli = 0;
    }
    s_game.target_boost_milli = s_game.current_boost_milli;
  }

  // Progress advancement
  uint64_t mult = 100000ULL + s_game.current_boost_milli;
  uint32_t progress_delta = (uint32_t)(((uint64_t)dt_ms * mult) / 100000ULL);
  s_game.data.progress_ms += progress_delta;
  uint32_t interval = game_state_get_base_interval_ms();

  while (s_game.data.progress_ms >= interval) {
    s_game.data.progress_ms -= interval;
    s_game.data.current_pixels++;
    if (out_pixel_added) {
      *out_pixel_added = true;
    }
    if (s_game.data.current_pixels >= TOTAL_PIXELS) {
      s_game.data.current_pixels = TOTAL_PIXELS; // Keep 64 so preview shows complete solid grid
      s_game.transition_completed_color = s_game.data.current_color_idx;
      s_game.transition_active = true;
      s_game.transition_anim_running = false;
      s_game.transition_anim_ms = 0;
      s_game.view_mode = VIEW_MODE_TIER1;

      if (out_fill_completed) {
        *out_fill_completed = true;
      }
      return;
    }
  }
}

bool game_state_can_prestige(void) {
  return (s_game.data.solid_grids_count > 0);
}

uint32_t game_state_prestige(void) {
  uint32_t earned = s_game.data.solid_grids_count;
  s_game.data.currency += earned;
  s_game.data.current_pixels = 0;
  s_game.data.current_color_idx = 0;
  s_game.data.tier2_pixels = 0;
  s_game.data.solid_grids_count = 0; // resets in current run
  s_game.data.progress_ms = 0;
  s_game.data.prestige_level++;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.transition_anim_ms = 0;
  s_game.gold_pixel_just_earned = false;
  prv_shuffle_random_order();
  game_state_save();
  return earned;
}

// -------------------------------------------------------------
// Store Upgrades (Exponential Cost Scaling)
// -------------------------------------------------------------
bool game_state_buy_fill_speed(void) {
  uint32_t cost = game_state_get_fill_speed_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.upgrade_fill_speed_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_boost_amount(void) {
  uint32_t cost = game_state_get_boost_amount_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.upgrade_boost_amount_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_boost_accel(void) {
  uint32_t cost = game_state_get_boost_accel_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.upgrade_boost_accel_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_boost_sustain(void) {
  uint32_t cost = game_state_get_boost_sustain_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.upgrade_boost_sustain_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_boost_diminish(void) {
  uint32_t cost = game_state_get_boost_diminish_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.upgrade_boost_diminish_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_boost_limit(void) {
  uint32_t cost = game_state_get_boost_limit_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.upgrade_boost_limit_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_offline_cap(void) {
  uint32_t cost = game_state_get_offline_cap_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
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
  if (s_game.data.selected_bg_color_index == BG_COLOR_AUTO) {
    GColor fill_col = game_state_get_active_solid_color();
    if (game_state_is_color_dark(fill_col)) {
      return GColorWhite;
    } else {
      return GColorBlack;
    }
  }
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
