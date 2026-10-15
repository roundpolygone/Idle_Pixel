#include "game_state.h"

static GameState s_game;
static uint32_t s_active_play_ms_accum = 0;

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

// -------------------------------------------------------------
// Currency Formatting Helper: Transitions to $ at 100 cents
// -------------------------------------------------------------
void game_state_format_currency(uint32_t cents, char *buf, size_t buf_size) {
  if (!buf || buf_size == 0) return;
  if (cents < 100) {
    snprintf(buf, buf_size, "%lu¢", (unsigned long)cents);
  } else {
    snprintf(buf, buf_size, "$%lu.%02lu", (unsigned long)(cents / 100), (unsigned long)(cents % 100));
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
  return 5 + ((uint32_t)s_game.data.upgrade_boost_amount_lvl * 5);
}

uint32_t game_state_get_boost_accel_percent_per_sec(void) {
  return 20 + ((uint32_t)s_game.data.upgrade_boost_accel_lvl * 10);
}

uint32_t game_state_get_boost_sustain_ms(void) {
  return (uint32_t)s_game.data.upgrade_boost_sustain_lvl * 500;
}

uint32_t game_state_get_boost_diminish_tenths_per_sec(void) {
  return 1000 / (10 + ((uint32_t)s_game.data.upgrade_boost_diminish_lvl * 2));
}

uint32_t game_state_get_boost_max_limit_percent(void) {
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

// Hold boost rate calculation: 1.0 click/s base + 10% per level
uint32_t game_state_get_hold_boost_clicks_whole(void) {
  return 1 + ((uint32_t)s_game.data.upgrade_hold_boost_lvl / 10);
}

uint32_t game_state_get_hold_boost_clicks_tenths(void) {
  return (uint32_t)s_game.data.upgrade_hold_boost_lvl % 10;
}

uint32_t game_state_get_hold_boost_interval_ms(void) {
  // 1.0 click/s at lvl 0 = 1000ms. Lvl L = 10000 / (10 + L) ms.
  return 10000 / (10 + (uint32_t)s_game.data.upgrade_hold_boost_lvl);
}

// Away idle speed percentage: base 10% + 10% per level (max 100%)
uint32_t game_state_get_offline_speed_percent(void) {
  uint32_t pct = 10 + ((uint32_t)s_game.data.upgrade_offline_speed_lvl * 10);
  return (pct > 100) ? 100 : pct;
}

void game_state_get_pps(uint32_t *out_whole, uint32_t *out_hundredths) {
  uint32_t interval = game_state_get_base_interval_ms();
  if (interval == 0) interval = 1;
  uint64_t mult = 100000ULL + s_game.current_boost_milli;
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
    case FILL_ORDER_UP_AND_DOWN:
      if (col % 2 == 0) {
        return (uint8_t)(col * 8 + row);
      } else {
        return (uint8_t)(col * 8 + (7 - row));
      }
    case FILL_ORDER_CHECKERBOARD:
      if ((row + col) % 2 == 0) {
        return (uint8_t)((row * 4) + (col / 2));
      } else {
        return (uint8_t)(32 + (row * 4) + (col / 2));
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
    cost = (cost * 3 + 1) / 2;
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

uint32_t game_state_get_hold_boost_cost(void) {
  return game_state_calc_cost_1_5x(s_game.data.upgrade_hold_boost_lvl);
}

uint32_t game_state_get_offline_speed_cost(void) {
  if (s_game.data.upgrade_offline_speed_lvl >= 9) return 0; // maxed at 100%
  return game_state_calc_cost_2x(s_game.data.upgrade_offline_speed_lvl) * 2;
}

// -------------------------------------------------------------
// Save Slot Management
// -------------------------------------------------------------
static uint32_t prv_get_slot_key(uint8_t slot_idx) {
  if (slot_idx >= NUM_SAVE_SLOTS) slot_idx = 0;
  return PERSIST_KEY_SLOT_BASE + slot_idx;
}

uint8_t game_state_get_active_slot(void) {
  return s_game.active_slot;
}

void game_state_get_slot_info(uint8_t slot_idx, SaveSlotInfo *out_info) {
  if (!out_info) return;
  memset(out_info, 0, sizeof(SaveSlotInfo));
  uint32_t key = prv_get_slot_key(slot_idx);

  if (slot_idx == s_game.active_slot && s_game.data.is_used) {
    out_info->is_used = true;
    out_info->is_completed = s_game.data.is_completed;
    out_info->date_started = s_game.data.date_started;
    out_info->total_time_sec = s_game.data.active_play_sec + s_game.data.offline_play_sec;
    out_info->completion_time_sec = s_game.data.completion_time_sec;
    out_info->tier2_pixels = s_game.data.tier2_pixels;
    out_info->gold_pixels = s_game.data.gold_pixels;
    return;
  }

  if (persist_exists(key)) {
    GameSaveData slot_data;
    if (persist_read_data(key, &slot_data, sizeof(GameSaveData)) == sizeof(GameSaveData)) {
      out_info->is_used = slot_data.is_used;
      out_info->is_completed = slot_data.is_completed;
      out_info->date_started = slot_data.date_started;
      out_info->total_time_sec = slot_data.active_play_sec + slot_data.offline_play_sec;
      out_info->completion_time_sec = slot_data.completion_time_sec;
      out_info->tier2_pixels = slot_data.tier2_pixels;
      out_info->gold_pixels = slot_data.gold_pixels;
    }
  }
}

static void prv_init_default_save_data(void) {
  memset(&s_game.data, 0, sizeof(GameSaveData));
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
  s_game.data.upgrade_hold_boost_lvl = 0;
  s_game.data.upgrade_offline_speed_lvl = 0;
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
  s_game.data.option_show_seconds = true;
  s_game.data.last_save_time = time(NULL);
  s_game.data.date_started = time(NULL);
  s_game.data.active_play_sec = 0;
  s_game.data.offline_play_sec = 0;
  s_game.data.completion_time_sec = 0;
  s_game.data.total_currency_earned = 0;
  s_game.data.total_currency_spent = 0;
  s_game.data.is_completed = false;
  s_game.data.is_used = true;
  s_game.data.favourite_colors = 0;
}

bool game_state_switch_to_slot(uint8_t slot_idx) {
  if (slot_idx >= NUM_SAVE_SLOTS) return false;
  game_state_save();

  s_game.active_slot = slot_idx;
  persist_write_int(PERSIST_KEY_ACTIVE_SLOT, slot_idx);

  uint32_t key = prv_get_slot_key(slot_idx);
  if (persist_exists(key)) {
    persist_read_data(key, &s_game.data, sizeof(GameSaveData));
  } else {
    prv_init_default_save_data();
    game_state_save();
  }

  s_game.current_boost_milli = 0;
  s_game.target_boost_milli = 0;
  s_game.boost_sustain_remaining_ms = 0;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.preview_background_pixels = 0;
  prv_shuffle_random_order();
  return true;
}

bool game_state_save_to_slot(uint8_t slot_idx) {
  if (slot_idx >= NUM_SAVE_SLOTS) return false;
  s_game.data.is_used = true;
  s_game.data.last_save_time = time(NULL);
  uint32_t key = prv_get_slot_key(slot_idx);
  persist_write_data(key, &s_game.data, sizeof(GameSaveData));
  return true;
}

bool game_state_clear_slot(uint8_t slot_idx) {
  if (slot_idx >= NUM_SAVE_SLOTS) return false;
  uint32_t key = prv_get_slot_key(slot_idx);
  persist_delete(key);

  if (slot_idx == s_game.active_slot) {
    prv_init_default_save_data();
    game_state_save();
  }
  return true;
}

void game_state_init(void) {
  memset(&s_game, 0, sizeof(GameState));
  prv_shuffle_random_order();

  uint8_t active_slot = 0;
  if (persist_exists(PERSIST_KEY_ACTIVE_SLOT)) {
    active_slot = (uint8_t)persist_read_int(PERSIST_KEY_ACTIVE_SLOT);
    if (active_slot >= NUM_SAVE_SLOTS) active_slot = 0;
  }
  s_game.active_slot = active_slot;

  uint32_t active_key = prv_get_slot_key(active_slot);
  bool loaded = false;

  if (persist_exists(active_key)) {
    int bytes_read = persist_read_data(active_key, &s_game.data, sizeof(GameSaveData));
    if (bytes_read == sizeof(GameSaveData) && s_game.data.version == SAVE_VERSION) {
      loaded = true;
    }
  } else if (active_slot == 0 && persist_exists(PERSIST_KEY_GAME_DATA)) {
    // Legacy migration from single-save key
    uint32_t old_version = 0;
    persist_read_data(PERSIST_KEY_GAME_DATA, &old_version, sizeof(uint32_t));
    if (old_version >= 5 && old_version <= 7) {
      persist_read_data(PERSIST_KEY_GAME_DATA, &s_game.data, sizeof(GameSaveData));
      s_game.data.version = SAVE_VERSION;
      s_game.data.upgrade_hold_boost_lvl = 0;
      s_game.data.upgrade_offline_speed_lvl = 0;
      s_game.data.date_started = s_game.data.last_save_time > 0 ? s_game.data.last_save_time : time(NULL);
      s_game.data.active_play_sec = 0;
      s_game.data.offline_play_sec = 0;
      s_game.data.completion_time_sec = 0;
      s_game.data.total_currency_earned = s_game.data.currency;
      s_game.data.total_currency_spent = 0;
      s_game.data.is_completed = (s_game.data.gold_pixels > 0 || s_game.data.tier2_pixels >= 64);
      s_game.data.is_used = true;

      // Cap unlocks if previously overflowed
      if (s_game.data.unlocked_bg_colors > UNLOCK_BG_COLORS_COUNT + 1) {
        s_game.data.unlocked_bg_colors = UNLOCK_BG_COLORS_COUNT + 1;
      }
      if (s_game.data.unlocked_fill_orders > UNLOCK_FILL_ORDERS_COUNT + 1) {
        s_game.data.unlocked_fill_orders = UNLOCK_FILL_ORDERS_COUNT + 1;
      }
      if (s_game.data.unlocked_fonts > UNLOCK_FONTS_COUNT + 1) {
        s_game.data.unlocked_fonts = UNLOCK_FONTS_COUNT + 1;
      }
      if (s_game.data.unlocked_clock_styles > UNLOCK_CLOCK_STYLES_COUNT + 1) {
        s_game.data.unlocked_clock_styles = UNLOCK_CLOCK_STYLES_COUNT + 1;
      }
      loaded = true;
      persist_write_data(active_key, &s_game.data, sizeof(GameSaveData));
    }
  }

  if (!loaded) {
    prv_init_default_save_data();
    persist_write_data(active_key, &s_game.data, sizeof(GameSaveData));
  } else {
    // Process offline idle catch-up with nerfed closed-game idle speed (10% base + 10%/lvl)
    time_t now = time(NULL);
    if (s_game.data.last_save_time > 0 && now > s_game.data.last_save_time) {
      time_t elapsed_sec = now - s_game.data.last_save_time;
      uint32_t max_offline = game_state_get_max_offline_sec();
      uint32_t credited_sec = (elapsed_sec < (time_t)max_offline) ? (uint32_t)elapsed_sec : max_offline;

      uint32_t efficiency_pct = game_state_get_offline_speed_percent();
      uint64_t effective_offline_ms = ((uint64_t)credited_sec * 1000 * efficiency_pct) / 100;
      uint32_t interval = game_state_get_base_interval_ms();
      uint64_t total_offline_ms = effective_offline_ms + s_game.data.progress_ms;
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

      s_game.data.offline_play_sec += credited_sec;

      // Check win condition
      if (!s_game.data.is_completed && (s_game.data.gold_pixels > 0 || total_tier2 >= 64)) {
        s_game.data.is_completed = true;
        s_game.data.completion_time_sec = s_game.data.active_play_sec + s_game.data.offline_play_sec;
        s_game.win_just_triggered = true;
      }

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
  s_game.preview_background_pixels = 0;
  s_game.is_clock_active = false;

  APP_LOG(APP_LOG_LEVEL_INFO, "=== USER FAVORITE COLORS ===");
  uint8_t fav_count = 0;
  for (uint8_t i = 0; i < 64; i++) {
    if (s_game.data.favourite_colors & (1ULL << i)) {
      fav_count++;
      APP_LOG(APP_LOG_LEVEL_INFO, "FAVORITE: #%02d %s (0x%02X)",
              (int)i, game_state_get_color_name(i), (unsigned int)game_state_get_pixel_color(i).argb);
    }
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Total Favorites: %u/64", (unsigned int)fav_count);
}

void game_state_save(void) {
  s_game.data.last_save_time = time(NULL);
  s_game.data.is_used = true;
  uint32_t key = prv_get_slot_key(s_game.active_slot);
  persist_write_data(key, &s_game.data, sizeof(GameSaveData));
  persist_write_int(PERSIST_KEY_ACTIVE_SLOT, s_game.active_slot);
}

void game_state_hard_reset(void) {
  game_state_clear_slot(s_game.active_slot);
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

    if (!s_game.data.is_completed) {
      s_game.data.is_completed = true;
      s_game.data.completion_time_sec = s_game.data.active_play_sec + s_game.data.offline_play_sec;
      s_game.win_just_triggered = true;
    }
  }

  s_game.data.current_color_idx = (s_game.data.current_color_idx + 1) % 64;
  s_game.data.current_pixels = s_game.preview_background_pixels;
  s_game.preview_background_pixels = 0;
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

void game_state_set_clock_active(bool active) {
  s_game.is_clock_active = active;
}

bool game_state_is_clock_active(void) {
  return s_game.is_clock_active;
}

bool game_state_check_and_clear_win_trigger(void) {
  if (s_game.win_just_triggered) {
    s_game.win_just_triggered = false;
    return true;
  }
  return false;
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
  if (out_pixel_added) *out_pixel_added = false;
  if (out_fill_completed) *out_fill_completed = false;

  // Track in-game playtime seconds
  s_active_play_ms_accum += dt_ms;
  if (s_active_play_ms_accum >= 1000) {
    s_game.data.active_play_sec += s_active_play_ms_accum / 1000;
    s_active_play_ms_accum %= 1000;
  }

  // Handle shrink/expand celebration animation
  if (s_game.transition_active && s_game.transition_anim_running) {
    s_game.transition_anim_ms += dt_ms;
    if (s_game.transition_anim_ms >= 1600) {
      game_state_finish_transition();
    }
    return;
  }

  uint32_t max_milli = game_state_get_boost_max_limit_percent() * 1000;
  if (s_game.target_boost_milli > max_milli) s_game.target_boost_milli = max_milli;
  if (s_game.current_boost_milli > max_milli) s_game.current_boost_milli = max_milli;

  // Update boost momentum physics
  if (s_game.current_boost_milli < s_game.target_boost_milli) {
    uint32_t accel_milli_sec = game_state_get_boost_accel_percent_per_sec() * 1000;
    uint32_t accel_step = (accel_milli_sec * dt_ms) / 1000;
    if (s_game.current_boost_milli + accel_step >= s_game.target_boost_milli) {
      s_game.current_boost_milli = s_game.target_boost_milli;
      s_game.boost_sustain_remaining_ms = game_state_get_boost_sustain_ms();
    } else {
      s_game.current_boost_milli += accel_step;
    }
  } else if (s_game.boost_sustain_remaining_ms > 0) {
    if (s_game.boost_sustain_remaining_ms > dt_ms) {
      s_game.boost_sustain_remaining_ms -= dt_ms;
    } else {
      s_game.boost_sustain_remaining_ms = 0;
    }
  } else if (s_game.current_boost_milli > 0) {
    uint32_t decay_tenths_sec = game_state_get_boost_diminish_tenths_per_sec();
    uint32_t decay_milli_sec = decay_tenths_sec * 100;
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

    // Check if we are currently holding on a completed preview
    if (s_game.transition_active && !s_game.transition_anim_running) {
      // Background ticking behind preview:
      s_game.preview_background_pixels++;
      if (out_pixel_added) *out_pixel_added = true;

      // If another color fills completely on that preview page:
      // Automatically transition to higher tier and continue without previews!
      if (s_game.preview_background_pixels >= TOTAL_PIXELS) {
        // Complete the previewed color:
        s_game.data.solid_grids_count++;
        s_game.data.tier2_pixels++;
        if (s_game.data.solid_grids_count > s_game.data.max_unlock_milestone) {
          s_game.data.max_unlock_milestone = s_game.data.solid_grids_count;
          s_game.data.unlock_points++;
        }

        // Complete the second background-filled color:
        s_game.data.solid_grids_count++;
        s_game.data.tier2_pixels++;
        if (s_game.data.solid_grids_count > s_game.data.max_unlock_milestone) {
          s_game.data.max_unlock_milestone = s_game.data.solid_grids_count;
          s_game.data.unlock_points++;
        }

        if (s_game.data.tier2_pixels >= 64) {
          s_game.data.tier2_pixels %= 64;
          s_game.data.gold_pixels++;
          s_game.gold_pixel_just_earned = true;
          if (!s_game.data.is_completed) {
            s_game.data.is_completed = true;
            s_game.data.completion_time_sec = s_game.data.active_play_sec + s_game.data.offline_play_sec;
            s_game.win_just_triggered = true;
          }
        }

        // Advance active solid color by 2 and reset preview
        s_game.data.current_color_idx = (s_game.data.current_color_idx + 2) % 64;
        s_game.data.current_pixels = 0;
        s_game.preview_background_pixels = 0;
        s_game.transition_active = false;
        s_game.transition_anim_running = false;
        s_game.view_mode = VIEW_MODE_TIER2; // Automatically transitions to higher tier!
        prv_shuffle_random_order();
        game_state_save();
        if (out_fill_completed) *out_fill_completed = true;
        return;
      }
      continue;
    }

    // Normal filling
    s_game.data.current_pixels++;
    if (out_pixel_added) *out_pixel_added = true;

    if (s_game.data.current_pixels >= TOTAL_PIXELS) {
      if (s_game.is_clock_active) {
        // When on clock: continuously calculate without preview confirmation!
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
          if (!s_game.data.is_completed) {
            s_game.data.is_completed = true;
            s_game.data.completion_time_sec = s_game.data.active_play_sec + s_game.data.offline_play_sec;
            s_game.win_just_triggered = true;
          }
        }
        s_game.data.current_color_idx = (s_game.data.current_color_idx + 1) % 64;
        s_game.data.current_pixels = 0;
        prv_shuffle_random_order();
        if (out_fill_completed) *out_fill_completed = true;
      } else {
        // Standard interactive mode: trigger completed preview
        s_game.data.current_pixels = TOTAL_PIXELS;
        s_game.transition_completed_color = s_game.data.current_color_idx;
        s_game.transition_active = true;
        s_game.transition_anim_running = false;
        s_game.transition_anim_ms = 0;
        s_game.preview_background_pixels = 0;
        s_game.view_mode = VIEW_MODE_TIER1;
        if (out_fill_completed) *out_fill_completed = true;
      }
    }
  }
}

bool game_state_can_prestige(void) {
  return (s_game.data.solid_grids_count > 0);
}

uint32_t game_state_prestige(void) {
  uint32_t earned = s_game.data.solid_grids_count;
  s_game.data.currency += earned;
  s_game.data.total_currency_earned += earned;
  s_game.data.current_pixels = 0;
  s_game.data.current_color_idx = 0;
  s_game.data.tier2_pixels = 0;
  s_game.data.solid_grids_count = 0;
  s_game.data.progress_ms = 0;
  s_game.data.prestige_level++;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.transition_anim_ms = 0;
  s_game.gold_pixel_just_earned = false;
  s_game.preview_background_pixels = 0;
  prv_shuffle_random_order();
  game_state_save();
  return earned;
}

// -------------------------------------------------------------
// Store Upgrades
// -------------------------------------------------------------
bool game_state_buy_fill_speed(void) {
  uint32_t cost = game_state_get_fill_speed_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.total_currency_spent += cost;
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
    s_game.data.total_currency_spent += cost;
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
    s_game.data.total_currency_spent += cost;
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
    s_game.data.total_currency_spent += cost;
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
    s_game.data.total_currency_spent += cost;
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
    s_game.data.total_currency_spent += cost;
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
    s_game.data.total_currency_spent += cost;
    s_game.data.upgrade_offline_cap_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_hold_boost(void) {
  uint32_t cost = game_state_get_hold_boost_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.total_currency_spent += cost;
    s_game.data.upgrade_hold_boost_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

bool game_state_buy_offline_speed(void) {
  if (s_game.data.upgrade_offline_speed_lvl >= 9) return false;
  uint32_t cost = game_state_get_offline_speed_cost();
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.total_currency_spent += cost;
    s_game.data.upgrade_offline_speed_lvl++;
    game_state_save();
    return true;
  }
  return false;
}

// -------------------------------------------------------------
// Unlock Currency Purchasing (Capped at exactly 64 Total)
// -------------------------------------------------------------
uint32_t game_state_get_total_unlocks_count(void) {
  uint32_t count = 0;
  if (s_game.data.unlocked_bg_colors > 1) {
    count += (s_game.data.unlocked_bg_colors - 1);
  }
  if (s_game.data.unlocked_fill_orders > 1) {
    count += (s_game.data.unlocked_fill_orders - 1);
  }
  if (s_game.data.unlocked_fonts > 1) {
    count += (s_game.data.unlocked_fonts - 1);
  }
  if (s_game.data.unlocked_clock_styles > 1) {
    count += (s_game.data.unlocked_clock_styles - 1);
  }
  return count;
}

bool game_state_can_unlock_bg_color(void) {
  return (s_game.data.unlock_points >= 1 &&
          s_game.data.unlocked_bg_colors <= UNLOCK_BG_COLORS_COUNT &&
          game_state_get_total_unlocks_count() < TOTAL_MAX_UNLOCKS);
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
  return (s_game.data.unlock_points >= 1 &&
          s_game.data.unlocked_fill_orders <= UNLOCK_FILL_ORDERS_COUNT &&
          game_state_get_total_unlocks_count() < TOTAL_MAX_UNLOCKS);
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
  return (s_game.data.unlock_points >= 1 &&
          s_game.data.unlocked_fonts <= UNLOCK_FONTS_COUNT &&
          game_state_get_total_unlocks_count() < TOTAL_MAX_UNLOCKS);
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
  return (s_game.data.unlock_points >= 1 &&
          s_game.data.unlocked_clock_styles <= UNLOCK_CLOCK_STYLES_COUNT &&
          game_state_get_total_unlocks_count() < TOTAL_MAX_UNLOCKS);
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
    case FILL_ORDER_UP_AND_DOWN: return "Up and Down";
    case FILL_ORDER_CHECKERBOARD: return "Checkerboard";
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
    case 1: return "Square Analogue";
    case 2: return "Bold Minimalist";
    case 3: return "7-Segment Digital";
    case 4: return "Word Clock";
    case 5: return "Now Clock";
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

static const char * const s_pebble_color_names[64] = {
  "Black", "Oxford Blue", "Duke Blue", "Blue",
  "Dark Green", "Midnight Blue", "Cobalt Blue", "Blue Moon",
  "Islamic Green", "Jaeger Green", "Tiffany Blue", "Vivid Cerulean",
  "Green", "Malachite", "Medium Spring Green", "Cyan",
  "Bulgarian Rose", "Imperial Purple", "Dark Violet", "Electric Ultramarine",
  "Army Green", "Dark Gray", "Liberty", "Very Light Blue",
  "Kelly Green", "May Green", "Cadet Blue", "Picton Blue",
  "Bright Green", "Screamin' Green", "Medium Aquamarine", "Electric Blue",
  "Dark Candy Apple", "Jazzberry Jam", "Purple", "Vivid Violet",
  "Windsor Tan", "Rose Vale", "Purpureus", "Lavender Purple",
  "Amber", "Cadillac", "Light Gray", "Baby Blue Eyes",
  "Apple Green", "Inchworm", "Celeste", "Pastel Yellow",
  "Red", "Sunset Orange", "Fashion Fuchsia", "Magenta",
  "Orange", "Rajah", "Shocking Pink", "Rich Brilliant Lavender",
  "Chrome Yellow", "Yellow", "Sunny", "Melon",
  "Lime Green", "Spring Bud", "Mint Green", "White"
};

const char* game_state_get_color_name(uint8_t index) {
  if (index >= 64) return "Unknown";
  return s_pebble_color_names[index];
}

bool game_state_is_color_favourite(uint8_t index) {
  if (index >= 64) return false;
  return ((s_game.data.favourite_colors & (1ULL << index)) != 0);
}

void game_state_toggle_color_favourite(uint8_t index) {
  if (index >= 64) return;
  s_game.data.favourite_colors ^= (1ULL << index);
  bool is_fav = ((s_game.data.favourite_colors & (1ULL << index)) != 0);
  APP_LOG(APP_LOG_LEVEL_INFO, "FAVORITE_TOGGLE: #%02d %s -> %s",
          (int)index, s_pebble_color_names[index], is_fav ? "ON" : "OFF");
  game_state_save();
}

static const uint8_t s_unlockable_bg_colors[UNLOCK_BG_COLORS_COUNT] = {
  1,   // #01 Oxford Blue
  4,   // #04 Dark Green
  13,  // #13 Malachite
  16,  // #16 Bulgarian Rose
  20,  // #20 Army Green
  26,  // #26 Cadet Blue
  32,  // #32 Dark Candy Apple
  40,  // #40 Amber
  45,  // #45 Inchworm
  53,  // #53 Rajah
  59,  // #59 Melon
  61,  // #61 Spring Bud
  62   // #62 Mint Green
};

uint8_t game_state_get_unlockable_bg_color(uint8_t index) {
  if (index >= UNLOCK_BG_COLORS_COUNT) return 0;
  return s_unlockable_bg_colors[index];
}

const char* game_state_get_unlockable_bg_color_name(uint8_t index) {
  if (index >= UNLOCK_BG_COLORS_COUNT) return "Unknown";
  return game_state_get_color_name(s_unlockable_bg_colors[index]);
}

void game_state_dev_add_cents(uint32_t cents) {
  s_game.data.currency += cents;
  s_game.data.total_currency_earned += cents;
  game_state_save();
}

void game_state_dev_add_unlock_points(uint32_t points) {
  s_game.data.unlock_points += points;
  game_state_save();
}

void game_state_weak_vibe(void) {
  if (!s_game.data.option_vibration) return;
  static const uint32_t s_weak_segments[] = { 20 };
  VibePattern pat = {
    .durations = s_weak_segments,
    .num_segments = 1,
  };
  vibes_enqueue_custom_pattern(pat);
}

void game_state_cycle_bg_color(void) {
  uint8_t count = s_game.data.unlocked_bg_colors;
  uint8_t total_options = 1 + count; // 0: Auto Contrast, 1..count: unlocked colors
  if (total_options <= 1) {
    s_game.data.selected_bg_color_index = BG_COLOR_AUTO;
    game_state_save();
    game_state_weak_vibe();
    return;
  }

  int current_slot = 0; // 0 = BG_COLOR_AUTO
  if (s_game.data.selected_bg_color_index != BG_COLOR_AUTO) {
    for (uint8_t i = 0; i < count; i++) {
      if (game_state_get_unlockable_bg_color(i) == s_game.data.selected_bg_color_index) {
        current_slot = i + 1;
        break;
      }
    }
  }

  int next_slot = (current_slot + 1) % total_options;
  if (next_slot == 0) {
    s_game.data.selected_bg_color_index = BG_COLOR_AUTO;
  } else {
    s_game.data.selected_bg_color_index = game_state_get_unlockable_bg_color((uint8_t)(next_slot - 1));
  }

  game_state_save();
  game_state_weak_vibe();
}
