#include "game_state.h"
#include <stddef.h>

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

static GlobalAppData s_global_data;
static uint32_t prv_get_slot_key(uint8_t slot_idx);

static void prv_save_global_config(void) {
  persist_write_data(PERSIST_KEY_GLOBAL_CONFIG, &s_global_data, sizeof(GlobalAppData));
  persist_write_int(PERSIST_KEY_GLOBAL_CROWNS, (int32_t)s_global_data.crowns);
}

static void prv_add_start_level_bonus(uint8_t count) {
  if (count == 0) return;
  s_game.data.upgrade_fill_speed_lvl += count;
  s_game.data.upgrade_boost_amount_lvl += count;
  s_game.data.upgrade_boost_accel_lvl += count;
  s_game.data.upgrade_boost_sustain_lvl += count;
  s_game.data.upgrade_boost_diminish_lvl += count;
  s_game.data.upgrade_boost_limit_lvl += count;
  s_game.data.upgrade_offline_cap_lvl += count;
  s_game.data.upgrade_hold_boost_lvl += count;
  s_game.data.upgrade_layer_cents_lvl += count;
  if (s_game.data.upgrade_offline_speed_lvl + count <= 6) {
    s_game.data.upgrade_offline_speed_lvl += count;
  } else {
    s_game.data.upgrade_offline_speed_lvl = 6;
  }
}

static void prv_apply_global_config_to_session(void) {
  s_game.data.option_24h = s_global_data.option_24h;
  s_game.data.option_show_seconds = s_global_data.option_show_seconds;
  s_game.data.option_vibration = s_global_data.option_vibration;
  s_game.data.option_tap_to_wake = s_global_data.option_tap_to_wake;
  s_game.data.option_power_saving = s_global_data.option_power_saving;
  s_game.data.option_auto_close_win = s_global_data.option_auto_close_win;
  s_game.data.upgrade_auto_boost_lvl = s_global_data.upgrade_auto_boost_lvl;
  s_game.data.upgrade_clock_boost_lvl = s_global_data.upgrade_clock_boost_lvl;
  s_game.data.upgrade_headstart_lvl = s_global_data.upgrade_headstart_lvl;
  s_game.data.upgrade_auto_buy_lvl = s_global_data.upgrade_auto_buy_lvl;
  if (s_global_data.upgrade_start_level_lvl > s_game.data.upgrade_start_level_lvl) {
    uint8_t diff = s_global_data.upgrade_start_level_lvl - s_game.data.upgrade_start_level_lvl;
    prv_add_start_level_bonus(diff);
  }
  s_game.data.upgrade_start_level_lvl = s_global_data.upgrade_start_level_lvl;
}

static void prv_award_crowns(uint32_t count) {
  if (count == 0) return;

  uint32_t earned_so_far = s_game.data.crowns;
  uint32_t remaining = (earned_so_far < MAX_CROWNS_PER_SAVE) ? (MAX_CROWNS_PER_SAVE - earned_so_far) : 0;
  uint32_t to_award = (count < remaining) ? count : remaining;

  s_game.data.is_completed = true;
  if (s_game.data.completion_time_sec == 0) {
    s_game.data.completion_time_sec = s_game.data.active_play_sec + s_game.data.offline_play_sec;
  }
  s_game.win_just_triggered = true;

  if (to_award > 0) {
    s_game.crown_just_earned = true;
    s_game.data.crowns += to_award;

    // Award crowns directly to the global crown bank whenever a game wins
    s_global_data.total_crowns_earned += to_award;
    uint32_t spent = game_state_get_total_crowns_spent();
    if (s_global_data.total_crowns_earned >= spent) {
      s_global_data.crowns = s_global_data.total_crowns_earned - spent;
    } else {
      s_global_data.total_crowns_earned = spent;
      s_global_data.crowns = 0;
    }
    prv_save_global_config();
  } else {
    s_game.crown_just_earned = false;
  }

  // Guarantee player has enough unlock points to unlock everything if it wins
  uint32_t total_unlocked = game_state_get_total_unlocks_count();
  if (total_unlocked + s_game.data.unlock_points < TOTAL_MAX_UNLOCKS) {
    s_game.data.unlock_points = TOTAL_MAX_UNLOCKS - total_unlocked;
  }
}

static const uint32_t s_base_interval_table[16] = {
  10000, 9091, 8264, 7513, 6830, 6209, 5645, 5132,
  4665,  4241, 3855, 3505, 3186, 2897, 2633, 2394
};

uint32_t game_state_get_prestige_multiplier_percent(void) {
  return 100 + ((uint32_t)s_game.data.prestige_level * 5);
}

uint32_t game_state_get_base_interval_ms(void) {
  uint8_t lvl = s_game.data.upgrade_fill_speed_lvl;
  uint32_t interval;
  if (lvl < 16) {
    interval = s_base_interval_table[lvl];
  } else {
    interval = s_base_interval_table[15];
    for (uint8_t i = 15; i < lvl; i++) {
      interval = (interval * 10) / 11;
      if (interval < MIN_BASE_INTERVAL_MS) {
        interval = MIN_BASE_INTERVAL_MS;
        break;
      }
    }
  }

  uint32_t prestige_mult = game_state_get_prestige_multiplier_percent();
  if (prestige_mult > 100) {
    interval = (interval * 100) / prestige_mult;
    if (interval < MIN_BASE_INTERVAL_MS) {
      interval = MIN_BASE_INTERVAL_MS;
    }
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
  return (uint32_t)s_game.data.upgrade_boost_sustain_lvl * 1000;
}

static const uint32_t s_boost_diminish_table[16] = {
  200, 180, 160, 142, 126, 112, 100, 88, 78, 68, 59, 51, 44, 38, 32, 25
};

uint32_t game_state_get_boost_diminish_tenths_per_sec(void) {
  uint8_t lvl = s_game.data.upgrade_boost_diminish_lvl;
  if (lvl < 16) return s_boost_diminish_table[lvl];
  return 25; // minimum floor 2.5%/s
}

static const uint32_t s_boost_max_limit_table[16] = {
  100, 110, 120, 130, 145, 160, 175, 190, 205, 220, 235, 250, 265, 280, 290, 300
};

uint32_t game_state_get_boost_max_limit_percent(void) {
  uint8_t lvl = s_game.data.upgrade_boost_limit_lvl;
  if (lvl < 16) return s_boost_max_limit_table[lvl];
  return 300 + ((uint32_t)(lvl - 15) * 25);
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

bool game_state_is_boost_at_max(void) {
  uint32_t max_milli = game_state_get_boost_max_limit_percent() * 1000;
  if (max_milli == 0) max_milli = 100000;
  return (s_game.current_boost_milli >= max_milli);
}

bool game_state_is_boost_animating(void) {
  if (s_game.current_boost_milli == 0) return false;
  uint32_t max_milli = game_state_get_boost_max_limit_percent() * 1000;
  if (max_milli == 0) max_milli = 100000;
  if (s_game.current_boost_milli >= max_milli && s_game.target_boost_milli >= max_milli) {
    return false;
  }
  return true;
}

bool game_state_is_offline_unlocked(void) {
  return true; // Always unlocked at level 0 (1 minute initial cap)
}

// Formula B: 1.5x compound growth starting at 5 minutes, reaching 24h at Level 14
static const uint32_t s_offline_cap_minutes[15] = {
  5, 8, 12, 18, 27, 40, 60, 90, 135, 200, 300, 450, 675, 1000, 1440
};

uint32_t game_state_get_max_offline_sec(void) {
  uint8_t lvl = s_game.data.upgrade_offline_cap_lvl;
  if (lvl >= 14) return 24 * 3600; // 24 hours ceiling (1440m)
  return s_offline_cap_minutes[lvl] * 60;
}

static const uint32_t s_hold_boost_intervals[16] = {
  2000, 1635, 1335, 1095, 900, 735, 600, 495, 405, 330, 270, 225, 180, 150, 120, 105
};

uint32_t game_state_get_hold_boost_clicks_whole(void) {
  uint32_t tenths_total = 10 + (uint32_t)s_game.data.upgrade_hold_boost_lvl;
  return tenths_total / 10;
}

uint32_t game_state_get_hold_boost_clicks_tenths(void) {
  uint32_t tenths_total = 10 + (uint32_t)s_game.data.upgrade_hold_boost_lvl;
  return tenths_total % 10;
}

uint32_t game_state_get_hold_boost_interval_ms(void) {
  uint8_t lvl = s_game.data.upgrade_hold_boost_lvl;
  if (lvl < 16) return s_hold_boost_intervals[lvl];
  if (lvl >= 25) return 33; // safety floor ~30 clicks/sec
  return 105 - ((uint32_t)(lvl - 15) * 7);
}

// Away idle speed percentage: base 10% + 15% per level, max capped at 100% (Level 6 = 100%)
uint32_t game_state_get_offline_speed_percent(void) {
  uint32_t pct = 10 + ((uint32_t)s_game.data.upgrade_offline_speed_lvl * 15);
  return (pct > 100) ? 100 : pct;
}

static const uint32_t s_layer_bonus_table[16] = {
  1, 2, 4, 7, 12, 20, 35, 60, 100, 170, 290, 490, 830, 1400, 2370, 4000
};

uint32_t game_state_get_layer_bonus_cents(void) {
  uint8_t lvl = s_game.data.upgrade_layer_cents_lvl;
  if (lvl < 16) return s_layer_bonus_table[lvl];
  uint32_t val = s_layer_bonus_table[15];
  for (uint8_t i = 15; i < lvl; i++) {
    if (val > 2800000000UL) return 4000000000UL;
    val = (val * 3) / 2;
  }
  return val;
}

uint32_t game_state_get_clock_boost_percent(void) {
  return (uint32_t)game_state_get_upgrade_level(UPGRADE_CLOCK_BOOST) * 20;
}

uint32_t game_state_get_available_crowns(void) {
  return s_global_data.crowns;
}

uint32_t game_state_get_total_crowns_earned(void) {
  return s_global_data.total_crowns_earned;
}

uint32_t game_state_get_total_crowns_spent(void) {
  uint32_t start_lvl = s_global_data.upgrade_start_level_lvl;
  uint32_t start_lvl_crowns = (start_lvl * (start_lvl + 1)) / 2;
  uint32_t auto_buy_lvl = s_global_data.upgrade_auto_buy_lvl;
  uint32_t auto_buy_crowns = (auto_buy_lvl * (auto_buy_lvl + 1)) / 2;
  return (uint32_t)s_global_data.upgrade_auto_boost_lvl +
         (uint32_t)s_global_data.upgrade_clock_boost_lvl +
         (uint32_t)s_global_data.upgrade_headstart_lvl +
         start_lvl_crowns +
         auto_buy_crowns;
}

static void prv_sanitize_crowns(void) {
  uint32_t spent = game_state_get_total_crowns_spent();

  // Check all save slots and enforce max crowns per save
  uint32_t total_slot_crowns = 0;
  for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
    uint32_t k = prv_get_slot_key(i);
    if (persist_exists(k)) {
      GameSaveData d;
      int bytes = persist_read_data(k, &d, sizeof(GameSaveData));
      if (bytes > 0) {
        bool slot_updated = false;
        if (d.crowns > MAX_CROWNS_PER_SAVE) {
          d.crowns = d.is_completed ? MAX_CROWNS_PER_SAVE : 0;
          slot_updated = true;
        } else if (d.is_completed && d.crowns == 0) {
          d.crowns = 1;
          slot_updated = true;
        }
        if (slot_updated) {
          persist_write_data(k, &d, sizeof(GameSaveData));
        }
        total_slot_crowns += d.crowns;
      }
    }
  }

  if (s_game.data.crowns > MAX_CROWNS_PER_SAVE) {
    s_game.data.crowns = s_game.data.is_completed ? MAX_CROWNS_PER_SAVE : 0;
  } else if (s_game.data.is_completed && s_game.data.crowns == 0) {
    s_game.data.crowns = 1;
  }

  // Detect corrupted crowns (e.g. thousands from struct alignment bug or underflow)
  // or mathematical inconsistency where available crowns exceeds total crowns earned
  bool corrupted = (s_global_data.crowns > 100 ||
                    s_global_data.total_crowns_earned > 100 ||
                    s_global_data.total_crowns_earned < spent ||
                    s_global_data.crowns > s_global_data.total_crowns_earned);

  if (corrupted) {
    uint32_t legitimate_earned = (total_slot_crowns > spent) ? total_slot_crowns : spent;
    s_global_data.total_crowns_earned = legitimate_earned;
    s_global_data.crowns = legitimate_earned - spent;
    prv_save_global_config();
  } else {
    // Enforce invariant: available crowns == total_crowns_earned - spent
    if (s_global_data.total_crowns_earned >= spent) {
      s_global_data.crowns = s_global_data.total_crowns_earned - spent;
    } else {
      s_global_data.total_crowns_earned = spent;
      s_global_data.crowns = 0;
    }
    prv_save_global_config();
  }
}

bool game_state_spend_crowns(uint32_t count) {
  if (count == 0) return true;
  uint32_t spent = game_state_get_total_crowns_spent();
  if (s_global_data.crowns >= count && s_global_data.total_crowns_earned >= spent + count) {
    s_global_data.crowns -= count;
    prv_save_global_config();
    return true;
  }
  return false;
}

bool game_state_spend_crown(void) {
  return game_state_spend_crowns(1);
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

static const uint32_t s_cost_1_5x[16] = {
  1, 2, 2, 3, 5, 8, 11, 17, 26, 38, 58, 86, 130, 195, 292, 438
};

uint32_t game_state_calc_cost_1_5x(uint8_t level) {
  if (level < 16) {
    return s_cost_1_5x[level];
  }
  uint32_t cost = s_cost_1_5x[15];
  for (uint8_t i = 15; i < level; i++) {
    cost = (cost * 3) / 2;
  }
  return cost;
}

uint32_t game_state_calc_cost_2x(uint8_t level) {
  if (level >= 30) return 1000000000;
  return (uint32_t)(1UL << level);
}

// -------------------------------------------------------------
// Data-Driven Store Upgrades Definition & Engine
// -------------------------------------------------------------
static void prv_fmt_stat_fill_speed(char *buf, size_t buf_size) {
  uint32_t interval = game_state_get_base_interval_ms();
  snprintf(buf, buf_size, "%lu.%lus",
           (unsigned long)(interval / 1000), (unsigned long)((interval % 1000) / 100));
}

static void prv_fmt_stat_boost_amount(char *buf, size_t buf_size) {
  uint32_t boost_amt = game_state_get_boost_amount_percent();
  snprintf(buf, buf_size, "+%lu%%", (unsigned long)boost_amt);
}

static void prv_fmt_stat_boost_sustain(char *buf, size_t buf_size) {
  uint32_t sustain_ms = game_state_get_boost_sustain_ms();
  snprintf(buf, buf_size, "%lus", (unsigned long)(sustain_ms / 1000));
}

static void prv_fmt_stat_boost_diminish(char *buf, size_t buf_size) {
  uint32_t decay_tenths = game_state_get_boost_diminish_tenths_per_sec();
  snprintf(buf, buf_size, "-%lu.%lu%%/s",
           (unsigned long)(decay_tenths / 10), (unsigned long)(decay_tenths % 10));
}

static void prv_fmt_stat_boost_limit(char *buf, size_t buf_size) {
  uint32_t limit = game_state_get_boost_max_limit_percent();
  snprintf(buf, buf_size, "%lu%%", (unsigned long)limit);
}

static void prv_fmt_stat_hold_boost(char *buf, size_t buf_size) {
  uint32_t whole = game_state_get_hold_boost_clicks_whole();
  uint32_t tenths = game_state_get_hold_boost_clicks_tenths();
  snprintf(buf, buf_size, "%lu.%lu clk/s", (unsigned long)whole, (unsigned long)tenths);
}

static void prv_fmt_stat_offline_cap(char *buf, size_t buf_size) {
  uint32_t off_min = game_state_get_max_offline_sec() / 60;
  if (off_min < 60) {
    snprintf(buf, buf_size, "%lum", (unsigned long)off_min);
  } else {
    snprintf(buf, buf_size, "%luh %02lum",
             (unsigned long)(off_min / 60), (unsigned long)(off_min % 60));
  }
}

static void prv_fmt_stat_offline_speed(char *buf, size_t buf_size) {
  uint32_t eff_pct = game_state_get_offline_speed_percent();
  snprintf(buf, buf_size, "%lu%%", (unsigned long)eff_pct);
}

static void prv_fmt_stat_layer_bonus(char *buf, size_t buf_size) {
  uint32_t bonus = game_state_get_layer_bonus_cents();
  char cost_buf[24];
  game_state_format_currency(bonus, cost_buf, sizeof(cost_buf));
  snprintf(buf, buf_size, "+%s/row", cost_buf);
}

static void prv_fmt_stat_auto_boost(char *buf, size_t buf_size) {
  uint8_t lvl = game_state_get_upgrade_level(UPGRADE_AUTO_BOOST);
  if (lvl == 0) {
    snprintf(buf, buf_size, "1 boost / 5s");
  } else {
    uint32_t sec = (lvl >= 5) ? 1 : (6 - lvl);
    snprintf(buf, buf_size, "1 boost / %lus", (unsigned long)sec);
  }
}

static void prv_fmt_stat_clock_boost(char *buf, size_t buf_size) {
  uint32_t boost_pct = game_state_get_clock_boost_percent();
  snprintf(buf, buf_size, "+%lu%% on Clock", (unsigned long)boost_pct);
}

static void prv_fmt_stat_headstart(char *buf, size_t buf_size) {
  uint32_t bonus_cents = (uint32_t)game_state_get_upgrade_level(UPGRADE_STARTING_HEADSTART) * 50;
  char cur_str[24];
  game_state_format_currency(bonus_cents, cur_str, sizeof(cur_str));
  snprintf(buf, buf_size, "+%s Start", cur_str);
}

static void prv_fmt_stat_start_level(char *buf, size_t buf_size) {
  uint8_t lvl = s_global_data.upgrade_start_level_lvl;
  snprintf(buf, buf_size, "+%u Extra Lvl", (unsigned int)lvl);
}

static void prv_fmt_stat_auto_buy(char *buf, size_t buf_size) {
  uint32_t interval_ms = game_state_get_auto_buy_interval_ms();
  if (interval_ms == 0) {
    snprintf(buf, buf_size, "1 buy / 20.0s");
  } else {
    snprintf(buf, buf_size, "1 buy / %lu.%lus",
             (unsigned long)(interval_ms / 1000),
             (unsigned long)((interval_ms % 1000) / 100));
  }
}

typedef void (*UpgradeFormatStatFunc)(char *buf, size_t buf_size);

typedef struct {
  const char *title;
  size_t level_offset;
  CostCurveType curve_type;
  CurrencyType currency_type;
  uint8_t max_level;
  UpgradeFormatStatFunc format_stat;
} UpgradeDef;

static const UpgradeDef s_upgrades[UPGRADE_COUNT] = {
  [UPGRADE_FILL_SPEED] = {
    .title = "Auto-Fill Speed (+10%)",
    .level_offset = offsetof(GameSaveData, upgrade_fill_speed_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_fill_speed
  },
  [UPGRADE_BOOST_AMOUNT] = {
    .title = "Boost Amt (+5%)",
    .level_offset = offsetof(GameSaveData, upgrade_boost_amount_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_boost_amount
  },
  [UPGRADE_BOOST_SUSTAIN] = {
    .title = "Boost Sustain (+1s)",
    .level_offset = offsetof(GameSaveData, upgrade_boost_sustain_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_boost_sustain
  },
  [UPGRADE_BOOST_DIMINISH] = {
    .title = "Boost Diminish",
    .level_offset = offsetof(GameSaveData, upgrade_boost_diminish_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_boost_diminish
  },
  [UPGRADE_BOOST_LIMIT] = {
    .title = "Boost Max Limit",
    .level_offset = offsetof(GameSaveData, upgrade_boost_limit_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_boost_limit
  },
  [UPGRADE_HOLD_BOOST] = {
    .title = "Hold Auto-Press (+18%)",
    .level_offset = offsetof(GameSaveData, upgrade_hold_boost_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_hold_boost
  },
  [UPGRADE_OFFLINE_CAP] = {
    .title = "Max Idle Time (+50%)",
    .level_offset = offsetof(GameSaveData, upgrade_offline_cap_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_offline_cap
  },
  [UPGRADE_OFFLINE_SPEED] = {
    .title = "Away Idle Speed (+15%)",
    .level_offset = offsetof(GameSaveData, upgrade_offline_speed_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 6,
    .format_stat = prv_fmt_stat_offline_speed
  },
  [UPGRADE_LAYER_BONUS] = {
    .title = "Layer Bonus (1.5x/row)",
    .level_offset = offsetof(GameSaveData, upgrade_layer_cents_lvl),
    .curve_type = COST_CURVE_2X,
    .currency_type = CURRENCY_TYPE_CENTS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_layer_bonus
  },
  [UPGRADE_AUTO_BOOST] = {
    .title = "👑 Auto Boost",
    .level_offset = offsetof(GameSaveData, upgrade_auto_boost_lvl),
    .curve_type = COST_CURVE_CROWN_FLAT,
    .currency_type = CURRENCY_TYPE_CROWNS,
    .max_level = 5,
    .format_stat = prv_fmt_stat_auto_boost
  },
  [UPGRADE_CLOCK_BOOST] = {
    .title = "👑 Clock Boost (+20%)",
    .level_offset = offsetof(GameSaveData, upgrade_clock_boost_lvl),
    .curve_type = COST_CURVE_CROWN_FLAT,
    .currency_type = CURRENCY_TYPE_CROWNS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_clock_boost
  },
  [UPGRADE_STARTING_HEADSTART] = {
    .title = "👑 Starting Headstart",
    .level_offset = offsetof(GameSaveData, upgrade_headstart_lvl),
    .curve_type = COST_CURVE_CROWN_FLAT,
    .currency_type = CURRENCY_TYPE_CROWNS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_headstart
  },
  [UPGRADE_START_LEVEL] = {
    .title = "👑 Start Level",
    .level_offset = offsetof(GameSaveData, upgrade_start_level_lvl),
    .curve_type = COST_CURVE_CROWN_PROGRESSIVE,
    .currency_type = CURRENCY_TYPE_CROWNS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_start_level
  },
  [UPGRADE_AUTO_BUY] = {
    .title = "👑 Auto Buy (-10% Time)",
    .level_offset = offsetof(GameSaveData, upgrade_auto_buy_lvl),
    .curve_type = COST_CURVE_CROWN_PROGRESSIVE,
    .currency_type = CURRENCY_TYPE_CROWNS,
    .max_level = 0,
    .format_stat = prv_fmt_stat_auto_buy
  }
};

uint8_t game_state_get_upgrade_level(UpgradeId id) {
  if (id >= UPGRADE_COUNT) return 0;
  if (id == UPGRADE_AUTO_BOOST) return s_global_data.upgrade_auto_boost_lvl;
  if (id == UPGRADE_CLOCK_BOOST) return s_global_data.upgrade_clock_boost_lvl;
  if (id == UPGRADE_STARTING_HEADSTART) return s_global_data.upgrade_headstart_lvl;
  if (id == UPGRADE_START_LEVEL) return s_global_data.upgrade_start_level_lvl;
  if (id == UPGRADE_AUTO_BUY) return s_global_data.upgrade_auto_buy_lvl;
  const UpgradeDef *def = &s_upgrades[id];
  const uint8_t *lvl_ptr = (const uint8_t*)(((const uint8_t*)&s_game.data) + def->level_offset);
  return *lvl_ptr;
}

uint32_t game_state_get_upgrade_cost(UpgradeId id) {
  if (id >= UPGRADE_COUNT) return 0;
  const UpgradeDef *def = &s_upgrades[id];
  uint8_t lvl = game_state_get_upgrade_level(id);
  switch (def->curve_type) {
    case COST_CURVE_1_5X:
      return game_state_calc_cost_1_5x(lvl);
    case COST_CURVE_2X:
      return game_state_calc_cost_2x(lvl);
    case COST_CURVE_CROWN_FLAT:
      return 1;
    case COST_CURVE_CROWN_PROGRESSIVE:
      return (uint32_t)lvl + 1;
    default:
      return 0;
  }
}

CurrencyType game_state_get_upgrade_currency_type(UpgradeId id) {
  if (id >= UPGRADE_COUNT) return CURRENCY_TYPE_CENTS;
  return s_upgrades[id].currency_type;
}

bool game_state_is_upgrade_maxed(UpgradeId id) {
  if (id >= UPGRADE_COUNT) return true;
  const UpgradeDef *def = &s_upgrades[id];
  if (def->max_level > 0) {
    return game_state_get_upgrade_level(id) >= def->max_level;
  }
  return false;
}

bool game_state_buy_upgrade(UpgradeId id) {
  if (id >= UPGRADE_COUNT) return false;
  if (game_state_is_upgrade_maxed(id)) return false;

  const UpgradeDef *def = &s_upgrades[id];
  uint8_t *lvl_ptr = (uint8_t*)(((uint8_t*)&s_game.data) + def->level_offset);

  if (def->currency_type == CURRENCY_TYPE_CROWNS) {
    uint32_t cost = game_state_get_upgrade_cost(id);
    if (game_state_spend_crowns(cost)) {
      if (id == UPGRADE_AUTO_BOOST) {
        s_global_data.upgrade_auto_boost_lvl++;
        *lvl_ptr = s_global_data.upgrade_auto_boost_lvl;
      } else if (id == UPGRADE_CLOCK_BOOST) {
        s_global_data.upgrade_clock_boost_lvl++;
        *lvl_ptr = s_global_data.upgrade_clock_boost_lvl;
      } else if (id == UPGRADE_STARTING_HEADSTART) {
        s_global_data.upgrade_headstart_lvl++;
        *lvl_ptr = s_global_data.upgrade_headstart_lvl;
      } else if (id == UPGRADE_START_LEVEL) {
        s_global_data.upgrade_start_level_lvl++;
        prv_apply_global_config_to_session();
      } else if (id == UPGRADE_AUTO_BUY) {
        s_global_data.upgrade_auto_buy_lvl++;
        *lvl_ptr = s_global_data.upgrade_auto_buy_lvl;
      } else {
        (*lvl_ptr)++;
      }
      prv_save_global_config();
      game_state_save();
      return true;
    }
    return false;
  }

  // Cents
  uint32_t cost = game_state_get_upgrade_cost(id);
  if (s_game.data.currency >= cost) {
    s_game.data.currency -= cost;
    s_game.data.total_currency_spent += cost;
    (*lvl_ptr)++;
    game_state_save();
    return true;
  }
  return false;
}

const char* game_state_get_upgrade_title(UpgradeId id) {
  if (id >= UPGRADE_COUNT) return "";
  return s_upgrades[id].title;
}

void game_state_format_upgrade_sub(UpgradeId id, char *buf, size_t buf_size) {
  if (id >= UPGRADE_COUNT || !buf || buf_size == 0) return;
  const UpgradeDef *def = &s_upgrades[id];
  uint8_t lvl = game_state_get_upgrade_level(id);

  char stat_buf[32];
  if (def->format_stat) {
    def->format_stat(stat_buf, sizeof(stat_buf));
  } else {
    stat_buf[0] = '\0';
  }

  if (game_state_is_upgrade_maxed(id)) {
    snprintf(buf, buf_size, "Lvl %d (%s) | MAX", lvl, stat_buf);
  } else if (def->currency_type == CURRENCY_TYPE_CROWNS) {
    uint32_t cost = game_state_get_upgrade_cost(id);
    if (cost == 1) {
      snprintf(buf, buf_size, "Lvl %d (%s) | 1 Crown", lvl, stat_buf);
    } else {
      snprintf(buf, buf_size, "Lvl %d (%s) | %lu Crowns", lvl, stat_buf, (unsigned long)cost);
    }
  } else {
    char cost_str[24];
    uint32_t cost = game_state_get_upgrade_cost(id);
    game_state_format_currency(cost, cost_str, sizeof(cost_str));
    snprintf(buf, buf_size, "Lvl %d (%s) | %s", lvl, stat_buf, cost_str);
  }
}

// Legacy cost helpers forwarding to unified engine
uint32_t game_state_get_fill_speed_cost(void) { return game_state_get_upgrade_cost(UPGRADE_FILL_SPEED); }
uint32_t game_state_get_boost_amount_cost(void) { return game_state_get_upgrade_cost(UPGRADE_BOOST_AMOUNT); }
uint32_t game_state_get_boost_accel_cost(void) { return 0; }
uint32_t game_state_get_boost_sustain_cost(void) { return game_state_get_upgrade_cost(UPGRADE_BOOST_SUSTAIN); }
uint32_t game_state_get_boost_diminish_cost(void) { return game_state_get_upgrade_cost(UPGRADE_BOOST_DIMINISH); }
uint32_t game_state_get_boost_limit_cost(void) { return game_state_get_upgrade_cost(UPGRADE_BOOST_LIMIT); }
uint32_t game_state_get_offline_cap_cost(void) { return game_state_get_upgrade_cost(UPGRADE_OFFLINE_CAP); }
uint32_t game_state_get_hold_boost_cost(void) { return game_state_get_upgrade_cost(UPGRADE_HOLD_BOOST); }
uint32_t game_state_get_offline_speed_cost(void) { return game_state_get_upgrade_cost(UPGRADE_OFFLINE_SPEED); }
uint32_t game_state_get_layer_bonus_cost(void) { return game_state_get_upgrade_cost(UPGRADE_LAYER_BONUS); }
uint32_t game_state_get_auto_boost_cost_crowns(void) { return game_state_get_upgrade_cost(UPGRADE_AUTO_BOOST); }
uint32_t game_state_get_clock_boost_cost_crowns(void) { return game_state_get_upgrade_cost(UPGRADE_CLOCK_BOOST); }
uint32_t game_state_get_starting_headstart_cents(void) { return (uint32_t)game_state_get_upgrade_level(UPGRADE_STARTING_HEADSTART) * 50; }
bool game_state_buy_starting_headstart(void) { return game_state_buy_upgrade(UPGRADE_STARTING_HEADSTART); }

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
    out_info->crown_spent = false;
    out_info->date_started = s_game.data.date_started;
    out_info->total_time_sec = s_game.data.active_play_sec + s_game.data.offline_play_sec;
    out_info->completion_time_sec = s_game.data.completion_time_sec;
    out_info->tier2_pixels = s_game.data.tier2_pixels;
    out_info->crowns = s_game.data.crowns;
    return;
  }

  if (persist_exists(key)) {
    GameSaveData slot_data;
    if (persist_read_data(key, &slot_data, sizeof(GameSaveData)) == sizeof(GameSaveData)) {
      out_info->is_used = slot_data.is_used;
      out_info->is_completed = slot_data.is_completed;
      out_info->crown_spent = false;
      out_info->date_started = slot_data.date_started;
      out_info->total_time_sec = slot_data.active_play_sec + slot_data.offline_play_sec;
      out_info->completion_time_sec = slot_data.completion_time_sec;
      out_info->tier2_pixels = slot_data.tier2_pixels;
      out_info->crowns = slot_data.crowns;
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
  s_game.data.crowns = 0;
  s_game.data.max_unlock_milestone = 0;
  s_game.data.progress_ms = 0;
  s_game.data.currency = game_state_get_starting_headstart_cents();
  s_game.data.prestige_level = 0;
  s_game.data.unlock_points = 0;
  uint8_t start_lvl = s_global_data.upgrade_start_level_lvl;
  s_game.data.upgrade_fill_speed_lvl = start_lvl;
  s_game.data.upgrade_boost_amount_lvl = start_lvl;
  s_game.data.upgrade_boost_accel_lvl = start_lvl;
  s_game.data.upgrade_boost_sustain_lvl = start_lvl;
  s_game.data.upgrade_boost_diminish_lvl = start_lvl;
  s_game.data.upgrade_boost_limit_lvl = start_lvl;
  s_game.data.upgrade_offline_cap_lvl = start_lvl;
  s_game.data.upgrade_hold_boost_lvl = start_lvl;
  s_game.data.upgrade_offline_speed_lvl = (start_lvl > 6) ? 6 : start_lvl;
  s_game.data.upgrade_layer_cents_lvl = start_lvl;
  s_game.data.upgrade_auto_boost_lvl = s_global_data.upgrade_auto_boost_lvl;
  s_game.data.upgrade_clock_boost_lvl = s_global_data.upgrade_clock_boost_lvl;
  s_game.data.upgrade_headstart_lvl = s_global_data.upgrade_headstart_lvl;
  s_game.data.upgrade_start_level_lvl = s_global_data.upgrade_start_level_lvl;
  s_game.data.upgrade_auto_buy_lvl = s_global_data.upgrade_auto_buy_lvl;
  s_game.data.crown_spent = false;
  s_game.data.unlocked_bg_colors = 1;
  s_game.data.selected_bg_color_index = BG_COLOR_AUTO;
  s_game.data.unlocked_fill_orders = 1;
  s_game.data.selected_fill_order = FILL_ORDER_NORMAL;
  s_game.data.unlocked_fonts = 1;
  s_game.data.selected_font = 0;
  s_game.data.unlocked_font_sizes = 1;
  s_game.data.selected_font_size = 0;
  s_game.data.unlocked_clock_styles = 1;
  s_game.data.selected_clock_style = 0;
  s_game.data.option_vibration = s_global_data.option_vibration;
  s_game.data.option_analog_clock = false;
  s_game.data.option_24h = s_global_data.option_24h;
  s_game.data.option_show_seconds = s_global_data.option_show_seconds;
  s_game.data.option_tap_to_wake = s_global_data.option_tap_to_wake;
  s_game.data.option_power_saving = s_global_data.option_power_saving;
  s_game.data.option_auto_close_win = s_global_data.option_auto_close_win;
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
  prv_apply_global_config_to_session();

  s_game.current_boost_milli = 0;
  s_game.target_boost_milli = 0;
  s_game.boost_sustain_remaining_ms = 0;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.meld_anim_running = false;
  s_game.meld_anim_ms = 0;
  s_game.meld_anim_color = 0;
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
    int other_slot = -1;
    for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
      if (i != slot_idx && persist_exists(prv_get_slot_key(i))) {
        other_slot = i;
        break;
      }
    }
    if (other_slot >= 0) {
      game_state_switch_to_slot((uint8_t)other_slot);
    } else {
      s_game.active_slot = 0;
      persist_write_int(PERSIST_KEY_ACTIVE_SLOT, 0);
      prv_init_default_save_data();
      s_game.data.is_used = true;
      prv_apply_global_config_to_session();
      game_state_save();
    }
  }
  return true;
}

bool game_state_start_new_run_in_slot(uint8_t slot_idx) {
  if (slot_idx >= NUM_SAVE_SLOTS) return false;
  game_state_save();

  s_game.active_slot = slot_idx;
  persist_write_int(PERSIST_KEY_ACTIVE_SLOT, slot_idx);

  prv_init_default_save_data();
  s_game.data.is_used = true;
  prv_apply_global_config_to_session();
  s_game.data.currency = game_state_get_starting_headstart_cents();
  game_state_save_to_slot(slot_idx);

  s_game.current_boost_milli = 0;
  s_game.target_boost_milli = 0;
  s_game.boost_sustain_remaining_ms = 0;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.meld_anim_running = false;
  s_game.meld_anim_ms = 0;
  s_game.meld_anim_color = 0;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.preview_background_pixels = 0;
  prv_shuffle_random_order();
  return true;
}

void game_state_init(void) {
  memset(&s_game, 0, sizeof(GameState));
  prv_shuffle_random_order();

  if (persist_exists(PERSIST_KEY_GLOBAL_CONFIG)) {
    memset(&s_global_data, 0, sizeof(GlobalAppData));
    persist_read_data(PERSIST_KEY_GLOBAL_CONFIG, &s_global_data, sizeof(GlobalAppData));
  } else {
    memset(&s_global_data, 0, sizeof(GlobalAppData));
    s_global_data.version = 1;
    s_global_data.option_24h = false;
    s_global_data.option_show_seconds = true;
    s_global_data.option_vibration = true;
    s_global_data.option_tap_to_wake = true;
    s_global_data.option_power_saving = false;
    s_global_data.option_auto_close_win = false;
    if (persist_exists(PERSIST_KEY_GLOBAL_CROWNS)) {
      s_global_data.crowns = (uint32_t)persist_read_int(PERSIST_KEY_GLOBAL_CROWNS);
      s_global_data.total_crowns_earned = s_global_data.crowns;
    }
    prv_save_global_config();
  }

  // Migrate any crown purchases found across existing slots into global config
  bool global_upgrades_migrated = false;
  for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
    uint32_t k = prv_get_slot_key(i);
    if (persist_exists(k)) {
      GameSaveData d;
      if (persist_read_data(k, &d, sizeof(GameSaveData)) == sizeof(GameSaveData)) {
        if (d.upgrade_auto_boost_lvl > s_global_data.upgrade_auto_boost_lvl) {
          s_global_data.upgrade_auto_boost_lvl = d.upgrade_auto_boost_lvl;
          global_upgrades_migrated = true;
        }
        if (d.upgrade_clock_boost_lvl > s_global_data.upgrade_clock_boost_lvl) {
          s_global_data.upgrade_clock_boost_lvl = d.upgrade_clock_boost_lvl;
          global_upgrades_migrated = true;
        }
        if (d.upgrade_headstart_lvl > s_global_data.upgrade_headstart_lvl) {
          s_global_data.upgrade_headstart_lvl = d.upgrade_headstart_lvl;
          global_upgrades_migrated = true;
        }
        if (d.upgrade_start_level_lvl > s_global_data.upgrade_start_level_lvl) {
          s_global_data.upgrade_start_level_lvl = d.upgrade_start_level_lvl;
          global_upgrades_migrated = true;
        }
        if (d.upgrade_auto_buy_lvl > s_global_data.upgrade_auto_buy_lvl) {
          s_global_data.upgrade_auto_buy_lvl = d.upgrade_auto_buy_lvl;
          global_upgrades_migrated = true;
        }
      }
    }
  }
  if (global_upgrades_migrated) {
    prv_save_global_config();
  }

  // Migrate older saves to current version and sanitize save times
  for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
    uint32_t k = prv_get_slot_key(i);
    if (persist_exists(k)) {
      GameSaveData d;
      int bytes = persist_read_data(k, &d, sizeof(GameSaveData));
      if (bytes > 0 && d.version < SAVE_VERSION && d.version >= 5) {
        d.version = SAVE_VERSION;
        d.option_power_saving = s_global_data.option_power_saving;
        time_t now = time(NULL);
        if (d.last_save_time <= 0 || d.last_save_time > now) {
          d.last_save_time = now;
        }
        persist_write_data(k, &d, sizeof(GameSaveData));
      }
    }
  }

  // Sanitize crowns to ensure no negative numbers and exact match with earned and spent
  prv_sanitize_crowns();

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
    if (bytes_read > 0 && s_game.data.version == SAVE_VERSION) {
      loaded = true;
    } else if (bytes_read > 0 && s_game.data.version < SAVE_VERSION && s_game.data.version >= 5) {
      s_game.data.version = SAVE_VERSION;
      s_game.data.option_tap_to_wake = true;
      s_game.data.option_power_saving = s_global_data.option_power_saving;
      if (s_game.data.unlocked_font_sizes == 0) s_game.data.unlocked_font_sizes = 1;
      time_t now = time(NULL);
      if (s_game.data.last_save_time <= 0 || s_game.data.last_save_time > now) {
        s_game.data.last_save_time = now;
      }
      loaded = true;
      persist_write_data(active_key, &s_game.data, sizeof(GameSaveData));
    }
  } else if (active_slot == 0 && persist_exists(PERSIST_KEY_GAME_DATA)) {
    // Legacy migration from single-save key
    uint32_t old_version = 0;
    persist_read_data(PERSIST_KEY_GAME_DATA, &old_version, sizeof(uint32_t));
    if (old_version >= 5 && old_version <= 8) {
      persist_read_data(PERSIST_KEY_GAME_DATA, &s_game.data, sizeof(GameSaveData));
      s_game.data.version = SAVE_VERSION;
      s_game.data.option_tap_to_wake = true;
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
    s_game.active_slot = 0;
    persist_write_int(PERSIST_KEY_ACTIVE_SLOT, 0);
    prv_init_default_save_data();
    s_game.data.is_used = true;
    persist_write_data(prv_get_slot_key(0), &s_game.data, sizeof(GameSaveData));
  } else {
    prv_apply_global_config_to_session();
    // Process offline idle catch-up with closed-game idle speed
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

      // Earn layer cents for 8-pixel groups gained offline
      uint32_t layers_gained = (s_game.data.current_pixels + pixels_gained) / 8 - (s_game.data.current_pixels / 8);
      if (layers_gained > 0) {
        uint32_t layer_cents = (1 + s_game.data.upgrade_layer_cents_lvl) * layers_gained;
        s_game.data.currency += layer_cents;
        s_game.data.total_currency_earned += layer_cents;
      }

      uint32_t total_pixels = s_game.data.current_pixels + pixels_gained;
      uint32_t solid_grids_gained = total_pixels / TOTAL_PIXELS;
      s_game.data.current_pixels = total_pixels % TOTAL_PIXELS;
      s_game.data.solid_grids_count += solid_grids_gained;

      for (uint32_t f = 0; f < solid_grids_gained; f++) {
        uint32_t current_solid_num = s_game.data.solid_grids_count - solid_grids_gained + 1 + f;
        // Only get unlocks from before winning
        if (!s_game.data.is_completed && current_solid_num <= 64) {
          if (current_solid_num > s_game.data.max_unlock_milestone) {
            s_game.data.max_unlock_milestone = current_solid_num;
            s_game.data.unlock_points++;
          }
        }
      }

      uint32_t total_tier2 = s_game.data.tier2_pixels + solid_grids_gained;
      uint32_t completions = total_tier2 / 64;
      s_game.data.tier2_pixels = (uint8_t)(total_tier2 % 64);
      s_game.data.current_color_idx = (uint8_t)((s_game.data.current_color_idx + solid_grids_gained) % 64);

      s_game.data.offline_play_sec += credited_sec;

      // Check win condition and award crowns
      if (completions > 10) completions = 10;
      if (completions > 0) {
        prv_award_crowns(completions);
      }

      if (elapsed_sec > 60) {
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

  // Ensure global options and crown upgrades are applied to active session
  prv_apply_global_config_to_session();

  s_game.current_boost_milli = 0;
  s_game.target_boost_milli = 0;
  s_game.boost_sustain_remaining_ms = 0;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.meld_anim_running = false;
  s_game.meld_anim_ms = 0;
  s_game.meld_anim_color = 0;
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
  // Sync global options and crown purchases
  s_global_data.option_24h = s_game.data.option_24h;
  s_global_data.option_show_seconds = s_game.data.option_show_seconds;
  s_global_data.option_vibration = s_game.data.option_vibration;
  s_global_data.option_tap_to_wake = s_game.data.option_tap_to_wake;
  s_global_data.option_power_saving = s_game.data.option_power_saving;
  s_global_data.option_auto_close_win = s_game.data.option_auto_close_win;
  if (s_game.data.upgrade_auto_boost_lvl > s_global_data.upgrade_auto_boost_lvl) {
    s_global_data.upgrade_auto_boost_lvl = s_game.data.upgrade_auto_boost_lvl;
  }
  if (s_game.data.upgrade_clock_boost_lvl > s_global_data.upgrade_clock_boost_lvl) {
    s_global_data.upgrade_clock_boost_lvl = s_game.data.upgrade_clock_boost_lvl;
  }
  if (s_game.data.upgrade_headstart_lvl > s_global_data.upgrade_headstart_lvl) {
    s_global_data.upgrade_headstart_lvl = s_game.data.upgrade_headstart_lvl;
  }
  if (s_game.data.upgrade_start_level_lvl > s_global_data.upgrade_start_level_lvl) {
    s_global_data.upgrade_start_level_lvl = s_game.data.upgrade_start_level_lvl;
  }
  if (s_game.data.upgrade_auto_buy_lvl > s_global_data.upgrade_auto_buy_lvl) {
    s_global_data.upgrade_auto_buy_lvl = s_game.data.upgrade_auto_buy_lvl;
  }
  prv_save_global_config();

  s_game.data.last_save_time = time(NULL);
  s_game.data.is_used = true;
  uint32_t key = prv_get_slot_key(s_game.active_slot);
  persist_write_data(key, &s_game.data, sizeof(GameSaveData));
  persist_write_int(PERSIST_KEY_ACTIVE_SLOT, s_game.active_slot);
}

void game_state_hard_reset(void) {
  // Erase all saved data across all slots
  for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
    persist_delete(PERSIST_KEY_SLOT_BASE + i);
  }
  if (persist_exists(PERSIST_KEY_GAME_DATA)) {
    persist_delete(PERSIST_KEY_GAME_DATA);
  }
  if (persist_exists(PERSIST_KEY_GLOBAL_CROWNS)) {
    persist_delete(PERSIST_KEY_GLOBAL_CROWNS);
  }
  if (persist_exists(PERSIST_KEY_GLOBAL_CONFIG)) {
    persist_delete(PERSIST_KEY_GLOBAL_CONFIG);
  }
  memset(&s_global_data, 0, sizeof(GlobalAppData));
  s_global_data.version = 1;
  s_global_data.option_24h = false;
  s_global_data.option_show_seconds = true;
  s_global_data.option_vibration = true;
  s_global_data.option_tap_to_wake = true;
  s_global_data.option_power_saving = false;
  s_global_data.option_auto_close_win = false;
  prv_save_global_config();

  // Automatically start a save in the first slot
  s_game.active_slot = 0;
  persist_write_int(PERSIST_KEY_ACTIVE_SLOT, 0);
  prv_init_default_save_data();
  prv_apply_global_config_to_session();
  s_game.data.is_used = true;
  persist_write_data(prv_get_slot_key(0), &s_game.data, sizeof(GameSaveData));

  // Reset runtime transient state
  s_game.current_boost_milli = 0;
  s_game.target_boost_milli = 0;
  s_game.boost_sustain_remaining_ms = 0;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.preview_background_pixels = 0;
  prv_shuffle_random_order();
}

GridViewMode game_state_get_view_mode(void) {
  return s_game.view_mode;
}

void game_state_set_view_mode(GridViewMode mode) {
  s_game.view_mode = mode;
}

bool game_state_is_meld_anim_running(void) {
  return s_game.meld_anim_running;
}

uint32_t game_state_get_meld_anim_ms(void) {
  return s_game.meld_anim_ms;
}

uint8_t game_state_get_meld_anim_color(void) {
  return s_game.meld_anim_color;
}

bool game_state_is_transition_active(void) {
  return s_game.meld_anim_running;
}

bool game_state_is_transition_anim_running(void) {
  return s_game.meld_anim_running;
}

void game_state_start_transition_anim(void) {}

void game_state_finish_transition(void) {}

uint32_t game_state_get_transition_anim_ms(void) {
  return s_game.meld_anim_ms;
}

uint8_t game_state_get_transition_completed_color(void) {
  return s_game.meld_anim_color;
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

  // Advance meld & grow-to-fill screen animation if active
  if (s_game.meld_anim_running) {
    if (s_game.data.option_power_saving) {
      s_game.meld_anim_running = false;
      s_game.meld_anim_ms = 0;
      s_game.crown_just_earned = false;
    } else {
      s_game.meld_anim_ms += dt_ms;
      if (s_game.meld_anim_ms >= 480) {
        s_game.meld_anim_running = false;
        s_game.meld_anim_ms = 0;
        s_game.crown_just_earned = false;
      }
    }
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

  // Progress advancement (includes extra boost if on clock face)
  uint32_t extra_clock_boost_milli = 0;
  uint8_t clock_boost_lvl = game_state_get_upgrade_level(UPGRADE_CLOCK_BOOST);
  if (s_game.is_clock_active && clock_boost_lvl > 0) {
    extra_clock_boost_milli = (uint32_t)clock_boost_lvl * 20000;
  }
  uint64_t mult = 100000ULL + s_game.current_boost_milli + extra_clock_boost_milli;
  uint32_t progress_delta = (uint32_t)(((uint64_t)dt_ms * mult) / 100000ULL);
  s_game.data.progress_ms += progress_delta;
  uint32_t interval = game_state_get_base_interval_ms();

  while (s_game.data.progress_ms >= interval) {
    s_game.data.progress_ms -= interval;

    // Normal filling
    s_game.data.current_pixels++;
    if (out_pixel_added) *out_pixel_added = true;

    // Earn currency per 8 pixels (1 layer): base 1¢ + upgrade
    if ((s_game.data.current_pixels % 8) == 0) {
      uint32_t layer_cents = 1 + s_game.data.upgrade_layer_cents_lvl;
      s_game.data.currency += layer_cents;
      s_game.data.total_currency_earned += layer_cents;
    }

    if (s_game.data.current_pixels >= TOTAL_PIXELS) {
      // Completed solid grid: advance solid_grids_count and tier2_pixels
      s_game.data.solid_grids_count++;
      s_game.data.tier2_pixels++;
      // Only get unlocks from before winning
      if (!s_game.data.is_completed && s_game.data.solid_grids_count <= 64) {
        if (s_game.data.solid_grids_count > s_game.data.max_unlock_milestone) {
          s_game.data.max_unlock_milestone = s_game.data.solid_grids_count;
          s_game.data.unlock_points++;
        }
      }
      if (s_game.data.tier2_pixels >= 64) {
        s_game.data.tier2_pixels = 0;
        s_game.data.is_completed = true;
        prv_award_crowns(1);
      }

      // Start meld and grow animation on the pixel grid with completed color (skipped in power saving)
      s_game.meld_anim_color = s_game.data.current_color_idx;
      s_game.meld_anim_ms = 0;
      s_game.meld_anim_running = !s_game.data.option_power_saving;

      // Start next colour immediately with ZERO delay
      s_game.data.current_color_idx = (s_game.data.current_color_idx + 1) % 64;
      s_game.data.current_pixels = 0;
      prv_shuffle_random_order();
      game_state_save();
      if (out_fill_completed) *out_fill_completed = true;
    }
  }
}

uint32_t game_state_get_prestige_payout(void) {
  uint32_t layer_cents = 1 + s_game.data.upgrade_layer_cents_lvl;
  uint32_t total_layers = (s_game.data.solid_grids_count * 8) + (s_game.data.current_pixels / 8);
  return total_layers * layer_cents;
}

bool game_state_can_prestige(void) {
  return (s_game.data.solid_grids_count > 0);
}

uint32_t game_state_prestige(void) {
  uint32_t earned = game_state_get_prestige_payout();
  s_game.data.currency += earned;
  s_game.data.total_currency_earned += earned;
  s_game.data.current_pixels = 0;
  s_game.data.current_color_idx = 0;
  s_game.data.tier2_pixels = 0;
  s_game.data.solid_grids_count = 0;
  s_game.data.max_unlock_milestone = 0;
  s_game.data.progress_ms = 0;
  s_game.data.prestige_level++;
  s_game.view_mode = VIEW_MODE_TIER1;
  s_game.meld_anim_running = false;
  s_game.meld_anim_ms = 0;
  s_game.meld_anim_color = 0;
  s_game.transition_active = false;
  s_game.transition_anim_running = false;
  s_game.transition_anim_ms = 0;
  s_game.gold_pixel_just_earned = false;
  s_game.preview_background_pixels = 0;

  prv_apply_global_config_to_session();
  prv_shuffle_random_order();
  game_state_save();
  return earned;
}

uint32_t game_state_get_auto_boost_interval_ms(void) {
  uint8_t lvl = game_state_get_upgrade_level(UPGRADE_AUTO_BOOST);
  if (lvl == 0) return 0;
  if (lvl >= 5) return 1000;
  return (6 - lvl) * 1000;
}

uint8_t game_state_get_start_level(void) {
  return s_global_data.upgrade_start_level_lvl;
}

bool game_state_buy_start_level(void) {
  return game_state_buy_upgrade(UPGRADE_START_LEVEL);
}

uint32_t game_state_get_auto_buy_interval_ms(void) {
  uint8_t lvl = game_state_get_upgrade_level(UPGRADE_AUTO_BUY);
  if (lvl == 0) return 0;
  uint32_t interval_ms = 20000;
  for (uint8_t i = 1; i < lvl; i++) {
    interval_ms = (interval_ms * 9) / 10;
    if (interval_ms < 100) {
      interval_ms = 100;
      break;
    }
  }
  return interval_ms;
}

bool game_state_try_auto_buy_cheapest(void) {
  if (game_state_get_upgrade_level(UPGRADE_AUTO_BUY) == 0) return false;

  UpgradeId cheapest_id = UPGRADE_COUNT;
  uint32_t min_cost = 0xFFFFFFFFUL;

  for (uint8_t i = 0; i < UPGRADE_COUNT; i++) {
    UpgradeId id = (UpgradeId)i;
    if (game_state_get_upgrade_currency_type(id) != CURRENCY_TYPE_CENTS) continue;
    if (game_state_is_upgrade_maxed(id)) continue;

    uint32_t cost = game_state_get_upgrade_cost(id);
    if (cheapest_id == UPGRADE_COUNT || cost < min_cost) {
      cheapest_id = id;
      min_cost = cost;
    }
  }

  if (cheapest_id != UPGRADE_COUNT && s_game.data.currency >= min_cost) {
    return game_state_buy_upgrade(cheapest_id);
  }
  return false;
}

// -------------------------------------------------------------
// Store Upgrades (Legacy Wrappers)
// -------------------------------------------------------------
bool game_state_buy_fill_speed(void) { return game_state_buy_upgrade(UPGRADE_FILL_SPEED); }
bool game_state_buy_boost_amount(void) { return game_state_buy_upgrade(UPGRADE_BOOST_AMOUNT); }
bool game_state_buy_boost_accel(void) { return false; }
bool game_state_buy_boost_sustain(void) { return game_state_buy_upgrade(UPGRADE_BOOST_SUSTAIN); }
bool game_state_buy_boost_diminish(void) { return game_state_buy_upgrade(UPGRADE_BOOST_DIMINISH); }
bool game_state_buy_boost_limit(void) { return game_state_buy_upgrade(UPGRADE_BOOST_LIMIT); }
bool game_state_buy_offline_cap(void) { return game_state_buy_upgrade(UPGRADE_OFFLINE_CAP); }
bool game_state_buy_hold_boost(void) { return game_state_buy_upgrade(UPGRADE_HOLD_BOOST); }
bool game_state_buy_offline_speed(void) { return game_state_buy_upgrade(UPGRADE_OFFLINE_SPEED); }
bool game_state_buy_layer_bonus(void) { return game_state_buy_upgrade(UPGRADE_LAYER_BONUS); }
bool game_state_buy_auto_boost(void) { return game_state_buy_upgrade(UPGRADE_AUTO_BOOST); }
bool game_state_buy_clock_boost(void) { return game_state_buy_upgrade(UPGRADE_CLOCK_BOOST); }

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
  if (s_game.data.unlocked_font_sizes > 1) {
    count += (s_game.data.unlocked_font_sizes - 1);
  }
  if (s_game.data.unlocked_clock_styles > 1) {
    count += (s_game.data.unlocked_clock_styles - 1);
  }
  return count;
}

bool game_state_can_unlock_bg_color(void) {
  return (s_game.data.unlock_points >= 1 &&
          s_game.data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT &&
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

bool game_state_can_unlock_font_size(void) {
  return (s_game.data.unlock_points >= 1 &&
          s_game.data.unlocked_font_sizes < UNLOCK_FONT_SIZES_COUNT &&
          game_state_get_total_unlocks_count() < TOTAL_MAX_UNLOCKS);
}

bool game_state_buy_next_font_size(void) {
  if (game_state_can_unlock_font_size()) {
    s_game.data.unlock_points -= 1;
    s_game.data.unlocked_font_sizes++;
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

const char* game_state_get_font_size_name(uint8_t size_idx) {
  switch (size_idx) {
    case 1: return "Large";
    case 2: return "Small";
    case 0:
    default: return "Medium";
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

GFont game_state_get_clock_digit_font(uint8_t font_idx, uint8_t size_idx) {
  // size_idx: 0 = Medium (Standard), 1 = Large, 2 = Small
  if (size_idx == 1) { // Large
    switch (font_idx) {
      case 1: return fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
      case 2: return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
      case 3: return fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
      case 0:
      default: return fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
    }
  } else if (size_idx == 2) { // Small
    switch (font_idx) {
      case 1: return fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
      case 2: return fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
      case 3: return fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);
      case 0:
      default: return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    }
  } else { // Medium (Standard default)
    switch (font_idx) {
      case 1: return fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS);
      case 2: return fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
      case 3: return fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);
      case 0:
      default: return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    }
  }
}

GFont game_state_get_clock_text_font(uint8_t font_idx, uint8_t size_idx, uint8_t size_type) {
  // size_type: 0 = small/stats, 1 = medium/date/headers, 2 = large/words
  // size_idx:  0 = Medium, 1 = Large, 2 = Small
  if (size_type == 2) { // Words / Big Headers
    if (size_idx == 1) {
      return (font_idx == 1) ? fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK) : fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    } else if (size_idx == 2) {
      return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    } else {
      return (font_idx == 1) ? fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK) : fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    }
  } else if (size_type == 1) { // Date / Subheaders
    if (size_idx == 1) {
      return (font_idx == 3) ? fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21) : fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    } else if (size_idx == 2) {
      return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    } else {
      return (font_idx == 3) ? fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21) : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    }
  } else { // 0 = Small / Stats badge
    if (size_idx == 1) {
      return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    } else if (size_idx == 2) {
      return fonts_get_system_font(FONT_KEY_GOTHIC_14);
    } else {
      return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    }
  }
}

GFont game_state_get_font(uint8_t font_idx, bool large) {
  if (large) {
    return game_state_get_clock_digit_font(font_idx, s_game.data.selected_font_size);
  }
  return game_state_get_clock_text_font(font_idx, s_game.data.selected_font_size, 1);
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
  1,   // #01 Oxford Blue (Dark)
  11,  // #11 Vivid Cerulean (Light)
  52,  // #52 Orange (Light)
  16,  // #16 Bulgarian Rose (Dark)
  62,  // #62 Mint Green (Light)
  6,   // #06 Cobalt Blue (Dark)
  40,  // #40 Amber (Light)
  34,  // #34 Purple (Dark)
  39,  // #39 Lavender Purple (Light)
  45,  // #45 Inchworm (Light)
  4,   // #04 Dark Green (Dark)
  53,  // #53 Rajah (Light)
  32,  // #32 Dark Candy Apple (Dark)
  26,  // #26 Cadet Blue (Light)
  59,  // #59 Melon (Light)
  49,  // #49 Sunset Orange (Dark)
  13,  // #13 Malachite (Light)
  20,  // #20 Army Green (Dark)
  61   // #61 Spring Bud (Light)
};

uint8_t game_state_get_unlockable_bg_color(uint8_t index) {
  if (index >= UNLOCK_BG_COLORS_COUNT) return 0;
  return s_unlockable_bg_colors[index];
}

const char* game_state_get_unlockable_bg_color_name(uint8_t index) {
  if (index >= UNLOCK_BG_COLORS_COUNT) return "Unknown";
  return game_state_get_color_name(s_unlockable_bg_colors[index]);
}

#if ENABLE_DEV_MENU
void game_state_dev_add_cents(uint32_t cents) {
  s_game.data.currency += cents;
  s_game.data.total_currency_earned += cents;
  game_state_save();
}

void game_state_dev_add_unlock_points(uint32_t points) {
  s_game.data.unlock_points += points;
  game_state_save();
}
#endif

bool game_state_is_vibration_enabled(void) {
  return s_game.data.option_vibration;
}

bool game_state_get_power_saving(void) {
  return s_game.data.option_power_saving;
}

void game_state_set_power_saving(bool enabled) {
  s_game.data.option_power_saving = enabled;
  s_global_data.option_power_saving = enabled;
  game_state_save();
}

bool game_state_get_auto_close_win(void) {
  return s_game.data.option_auto_close_win;
}

void game_state_set_auto_close_win(bool enabled) {
  s_game.data.option_auto_close_win = enabled;
  s_global_data.option_auto_close_win = enabled;
  game_state_save();
}

void game_state_weak_vibe(void) {
  if (!s_game.data.option_vibration) return;
  static const uint32_t s_weak_segments[] = { 40 };
  VibePattern pat = {
    .durations = s_weak_segments,
    .num_segments = 1,
  };
  vibes_enqueue_custom_pattern(pat);
}

void game_state_vibe_short(void) {
  if (s_game.data.option_vibration) {
    vibes_short_pulse();
  }
}

void game_state_vibe_double(void) {
  if (s_game.data.option_vibration) {
    vibes_double_pulse();
  }
}

void game_state_vibe_long(void) {
  if (s_game.data.option_vibration) {
    vibes_long_pulse();
  }
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

uint32_t game_state_get_global_crowns(void) {
  return s_global_data.crowns;
}

uint32_t game_state_get_active_save_crowns(void) {
  return s_game.data.crowns;
}

#if ENABLE_DEV_MENU
void game_state_dev_add_crown(void) {
  s_global_data.crowns += 1;
  s_global_data.total_crowns_earned += 1;
  prv_save_global_config();
  if (s_game.data.crowns < MAX_CROWNS_PER_SAVE) {
    s_game.data.crowns += 1;
  }
  game_state_save();
}
#endif
