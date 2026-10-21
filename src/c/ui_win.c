#include "ui.h"
#include "game_state.h"

#define NUM_FIREWORK_PARTICLES 45

typedef struct {
  int16_t x;
  int16_t y;
  int16_t vx;
  int16_t vy;
  GColor color;
  uint8_t life;
  uint8_t max_life;
} Particle;

static Window *s_win_window = NULL;
static Layer *s_win_layer = NULL;
static AppTimer *s_anim_timer = NULL;
static Particle *s_particles = NULL;
static uint32_t s_anim_frames = 0;

static const GColor s_bright_colors[] = {
  { .argb = 0b11110000 }, // Red
  { .argb = 0b11111100 }, // Yellow
  { .argb = 0b11001100 }, // Green
  { .argb = 0b11001111 }, // Cyan
  { .argb = 0b11110011 }, // Magenta
  { .argb = 0b11110100 }, // Orange
  { .argb = 0b11111011 }, // Light Purple
  { .argb = 0b11011111 }, // Electric Blue
  { .argb = 0b11111111 }, // White
};

static void prv_spawn_burst(int16_t origin_x, int16_t origin_y, int start_idx, int count) {
  if (!s_particles) return;
  for (int i = 0; i < count; i++) {
    int idx = start_idx + i;
    if (idx >= NUM_FIREWORK_PARTICLES) break;
    s_particles[idx].x = origin_x;
    s_particles[idx].y = origin_y;
    int angle = rand() % TRIG_MAX_ANGLE;
    int speed = (rand() % 4) + 2;
    s_particles[idx].vx = (int16_t)((sin_lookup(angle) * speed) / TRIG_MAX_RATIO);
    s_particles[idx].vy = (int16_t)((-cos_lookup(angle) * speed) / TRIG_MAX_RATIO);
    s_particles[idx].color = s_bright_colors[rand() % (sizeof(s_bright_colors) / sizeof(s_bright_colors[0]))];
    s_particles[idx].max_life = (rand() % 15) + 18;
    s_particles[idx].life = s_particles[idx].max_life;
  }
}

static void prv_update_particles(GRect bounds) {
  if (!s_particles) return;
  s_anim_frames++;
  if (s_anim_frames % 22 == 1) {
    int origin_x = (rand() % (bounds.size.w - 40)) + 20;
    int origin_y = (rand() % (bounds.size.h / 2)) + 20;
    int burst_start = ((s_anim_frames / 22) % 3) * 15;
    prv_spawn_burst(origin_x, origin_y, burst_start, 15);
  }

  for (int i = 0; i < NUM_FIREWORK_PARTICLES; i++) {
    if (s_particles[i].life > 0) {
      s_particles[i].x += s_particles[i].vx;
      s_particles[i].y += s_particles[i].vy;
      s_particles[i].vy += 1; // slight gravity
      s_particles[i].life--;
    }
  }
}

// -------------------------------------------------------------
// Win Screen Update & Input
// -------------------------------------------------------------
static void prv_format_duration(uint32_t seconds, char *buf, size_t buf_size) {
  uint32_t mins = seconds / 60;
  uint32_t hours = mins / 60;
  mins = mins % 60;
  if (hours > 0) {
    snprintf(buf, buf_size, "%luh %lum", (unsigned long)hours, (unsigned long)mins);
  } else {
    snprintf(buf, buf_size, "%lum %lus", (unsigned long)mins, (unsigned long)(seconds % 60));
  }
}

static void prv_win_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  // 1. Black space background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // 2. Stats card overlay
  int card_w = bounds.size.w - 12;
  int card_h = bounds.size.h - 14;
  int card_x = 6;
  int card_y = 7;
  GRect card_rect = GRect(card_x, card_y, card_w, card_h);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, card_rect, 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorYellow);
  graphics_draw_rect(ctx, card_rect);

  // Title with crown
  graphics_context_set_text_color(ctx, GColorYellow);
  const char *title_str = game->crown_just_earned ? "\xF0\x9F\x91\x91 CROWN WON! \xF0\x9F\x91\x91"
                                                  : "\xF0\x9F\x91\x91 64 COLOURS! \xF0\x9F\x91\x91";
  graphics_draw_text(ctx, title_str,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(card_x, card_y + 2, card_w, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "All 64 Colours Complete!",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(card_x, card_y + 20, card_w, 15),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // Stats formatting
  char total_time_str[24];
  char in_game_str[24];
  char away_str[24];
  char earned_str[24];
  char spent_str[24];

  uint32_t total_sec = game->data.active_play_sec + game->data.offline_play_sec;
  prv_format_duration(total_sec, total_time_str, sizeof(total_time_str));
  prv_format_duration(game->data.active_play_sec, in_game_str, sizeof(in_game_str));
  prv_format_duration(game->data.offline_play_sec, away_str, sizeof(away_str));
  game_state_format_currency(game->data.total_currency_earned, earned_str, sizeof(earned_str));
  game_state_format_currency(game->data.total_currency_spent, spent_str, sizeof(spent_str));

  char stats_buf[220];
  snprintf(stats_buf, sizeof(stats_buf),
           "Time: %s (Away %s)\nCrowns: %lu | Unlocks: %lu\nEarned: %s | Spent: %s",
           total_time_str, away_str,
           (unsigned long)game_state_get_global_crowns(),
           (unsigned long)game_state_get_total_unlocks_count(),
           earned_str, spent_str);

  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, stats_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(card_x + 8, card_y + 36, card_w - 16, card_h - 66),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentLeft,
                     NULL);

  // Dismiss prompts: Choice to keep going or start a new save
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "> Keep Going",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(card_x, card_y + card_h - 26, card_w, 13),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
  graphics_context_set_text_color(ctx, GColorYellow);
  graphics_draw_text(ctx, "v Start New Save",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(card_x, card_y + card_h - 14, card_w, 13),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // 3. Draw firework particles ON TOP of stats card overlay
  if (s_particles) {
    for (int i = 0; i < NUM_FIREWORK_PARTICLES; i++) {
      if (s_particles[i].life > 0) {
        graphics_context_set_fill_color(ctx, s_particles[i].color);
        int radius = (s_particles[i].life > 6) ? 2 : 1;
        graphics_fill_circle(ctx, GPoint(s_particles[i].x, s_particles[i].y), radius);
      }
    }
  }
}

static void prv_anim_timer_callback(void *context) {
  if (s_win_layer) {
    GRect bounds = layer_get_bounds(s_win_layer);
    prv_update_particles(bounds);
    layer_mark_dirty(s_win_layer);
  }
  uint32_t interval = game_state_get_power_saving() ? 1000 : 33;
  s_anim_timer = app_timer_register(interval, prv_anim_timer_callback, NULL);
}

static void prv_continue_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_win_window) {
    window_stack_pop(true);
  }
}

static void prv_new_save_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_win_window) {
    window_stack_pop(false);
  }
  // Find first unused slot
  uint8_t empty_slot = 0xFF;
  for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
    SaveSlotInfo info;
    game_state_get_slot_info(i, &info);
    if (!info.is_used) {
      empty_slot = i;
      break;
    }
  }

  if (empty_slot != 0xFF) {
    game_state_start_new_run_in_slot(empty_slot);
    game_state_vibe_short();
  } else {
    // If all slots are full, show save slots manager so user can choose
    ui_save_slots_show();
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_continue_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_continue_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_continue_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_new_save_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_win_layer = layer_create(bounds);
  layer_set_update_proc(s_win_layer, prv_win_update_proc);
  layer_add_child(window_layer, s_win_layer);

  s_particles = malloc(sizeof(Particle) * NUM_FIREWORK_PARTICLES);
  if (s_particles) {
    memset(s_particles, 0, sizeof(Particle) * NUM_FIREWORK_PARTICLES);
    s_anim_frames = 0;
    prv_spawn_burst(bounds.size.w / 2, bounds.size.h / 3, 0, 20);
  }

  uint32_t interval = game_state_get_power_saving() ? 1000 : 33;
  s_anim_timer = app_timer_register(interval, prv_anim_timer_callback, NULL);
  game_state_vibe_double();
}

static void prv_window_unload(Window *window) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  if (s_particles) {
    free(s_particles);
    s_particles = NULL;
  }
  layer_destroy(s_win_layer);
  s_win_layer = NULL;
  window_destroy(s_win_window);
  s_win_window = NULL;
}

void ui_win_show(void) {
  if (s_win_window) return;
  s_win_window = window_create();
  window_set_click_config_provider(s_win_window, prv_click_config_provider);
  window_set_window_handlers(s_win_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_win_window, true);
}
