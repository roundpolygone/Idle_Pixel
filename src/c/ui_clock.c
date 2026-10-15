#include "ui_clock.h"
#include "game_state.h"

static Window *s_clock_window = NULL;
static Layer *s_clock_layer = NULL;

// -------------------------------------------------------------
// Style 0: Big Digital
// -------------------------------------------------------------
static void prv_draw_big_digital(GContext *ctx, GRect bounds, struct tm *tick_time, bool is_24h, uint8_t font_idx, GColor fg_color) {
  char time_buf[16];
  if (is_24h) {
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tick_time->tm_hour, tick_time->tm_min);
  } else {
    int hour = tick_time->tm_hour % 12;
    if (hour == 0) hour = 12;
    snprintf(time_buf, sizeof(time_buf), "%d:%02d", hour, tick_time->tm_min);
  }

  int digit_y = (bounds.size.h / 2) - 40;
  graphics_context_set_text_color(ctx, fg_color);
  GFont time_font = game_state_get_font(font_idx, true);

  if (game_state_get()->data.option_show_seconds) {
    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), " :%02d", tick_time->tm_sec);
    GFont sec_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    GSize time_size = graphics_text_layout_get_content_size(time_buf, time_font, GRect(0, 0, bounds.size.w, 48), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GSize sec_size = graphics_text_layout_get_content_size(sec_buf, sec_font, GRect(0, 0, bounds.size.w, 48), GTextOverflowModeWordWrap, GTextAlignmentLeft);

    int total_w = time_size.w + sec_size.w;
    if (total_w > bounds.size.w - 4) {
      sec_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
      sec_size = graphics_text_layout_get_content_size(sec_buf, sec_font, GRect(0, 0, bounds.size.w, 48), GTextOverflowModeWordWrap, GTextAlignmentLeft);
      total_w = time_size.w + sec_size.w;
    }

    int start_x = (bounds.size.w - total_w) / 2;
    graphics_draw_text(ctx, time_buf, time_font,
                       GRect(start_x, digit_y, time_size.w, 48),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, sec_buf, sec_font,
                       GRect(start_x + time_size.w, digit_y + 14, sec_size.w, 28),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, time_buf, time_font,
                       GRect(0, digit_y, bounds.size.w, 48),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  char date_buf[32];
  strftime(date_buf, sizeof(date_buf), "%a, %b %e", tick_time);
  graphics_draw_text(ctx, date_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, digit_y + 46, bounds.size.w, 24),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

// -------------------------------------------------------------
// Style 1: Square Analogue Clock
// -------------------------------------------------------------
static void prv_draw_square_analogue(GContext *ctx, GRect bounds, struct tm *tick_time, GColor fg_color, GColor accent_color) {
  GPoint center = grect_center_point(&bounds);
  center.y -= 8;
  int half_size = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 18;
  GRect square_dial = GRect(center.x - half_size, center.y - half_size, half_size * 2, half_size * 2);

  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_rect(ctx, square_dial);

  // 12 hour ticks projected to square frame
  for (int i = 0; i < 12; i++) {
    int32_t angle = (TRIG_MAX_ANGLE * i) / 12;
    int16_t dx = (sin_lookup(angle) * half_size) / TRIG_MAX_RATIO;
    int16_t dy = (-cos_lookup(angle) * half_size) / TRIG_MAX_RATIO;
    int16_t abs_dx = (dx < 0) ? -dx : dx;
    int16_t abs_dy = (dy < 0) ? -dy : dy;
    int16_t max_comp = (abs_dx > abs_dy) ? abs_dx : abs_dy;
    if (max_comp > 0) {
      int16_t px2 = center.x + (int16_t)(((int32_t)dx * half_size) / max_comp);
      int16_t py2 = center.y + (int16_t)(((int32_t)dy * half_size) / max_comp);
      int16_t tick_len = (i % 3 == 0) ? 6 : 4;
      int16_t px1 = center.x + (int16_t)(((int32_t)dx * (half_size - tick_len)) / max_comp);
      int16_t py1 = center.y + (int16_t)(((int32_t)dy * (half_size - tick_len)) / max_comp);
      graphics_context_set_stroke_width(ctx, (i % 3 == 0) ? 2 : 1);
      graphics_draw_line(ctx, GPoint(px1, py1), GPoint(px2, py2));
    }
  }

  // Hour Hand
  int32_t hour_angle = (TRIG_MAX_ANGLE * (((tick_time->tm_hour % 12) * 6) + (tick_time->tm_min / 10))) / (12 * 6);
  int16_t hour_len = half_size * 5 / 10;
  GPoint hour_pt = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, center, hour_pt);

  // Minute Hand
  int32_t min_angle = (TRIG_MAX_ANGLE * tick_time->tm_min) / 60;
  int16_t min_len = half_size * 8 / 10;
  GPoint min_pt = (GPoint) {
    .x = (int16_t)(sin_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, center, min_pt);

  // Second Hand (if enabled in options)
  if (game_state_get()->data.option_show_seconds) {
    int32_t sec_angle = (TRIG_MAX_ANGLE * tick_time->tm_sec) / 60;
    int16_t sec_len = half_size * 9 / 10;
    GPoint sec_pt = (GPoint) {
      .x = (int16_t)(sin_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_color(ctx, accent_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, center, sec_pt);
  }

  // Center pin
  graphics_context_set_fill_color(ctx, fg_color);
  graphics_fill_rect(ctx, GRect(center.x - 3, center.y - 3, 6, 6), 0, GCornerNone);
}

// -------------------------------------------------------------
// Style 2: Bold Minimalist Analogue Clock
// -------------------------------------------------------------
static void prv_draw_bold_minimalist(GContext *ctx, GRect bounds, struct tm *tick_time, GColor fg_color, GColor accent_color) {
  GPoint center = grect_center_point(&bounds);
  center.y -= 8;
  int16_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 18;

  // 4 bold dots at 12, 3, 6, 9
  graphics_context_set_fill_color(ctx, fg_color);
  for (int i = 0; i < 4; i++) {
    int32_t angle = (TRIG_MAX_ANGLE * i) / 4;
    int16_t dot_dist = radius - 4;
    int16_t dx = (int16_t)(sin_lookup(angle) * dot_dist / TRIG_MAX_RATIO) + center.x;
    int16_t dy = (int16_t)(-cos_lookup(angle) * dot_dist / TRIG_MAX_RATIO) + center.y;
    graphics_fill_circle(ctx, GPoint(dx, dy), 3);
  }

  // Bold Hour Hand (width 6)
  int32_t hour_angle = (TRIG_MAX_ANGLE * (((tick_time->tm_hour % 12) * 6) + (tick_time->tm_min / 10))) / (12 * 6);
  int16_t hour_len = radius * 5 / 10;
  GPoint hour_pt = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_stroke_width(ctx, 6);
  graphics_draw_line(ctx, center, hour_pt);

  // Bold Minute Hand (width 4)
  int32_t min_angle = (TRIG_MAX_ANGLE * tick_time->tm_min) / 60;
  int16_t min_len = radius * 8 / 10;
  GPoint min_pt = (GPoint) {
    .x = (int16_t)(sin_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, center, min_pt);

  // Sleek accent needle with counter-balance tail (if enabled in options)
  if (game_state_get()->data.option_show_seconds) {
    int32_t sec_angle = (TRIG_MAX_ANGLE * tick_time->tm_sec) / 60;
    int16_t sec_len = radius * 9 / 10;
    int16_t tail_len = radius * 2 / 10;
    GPoint sec_tip = (GPoint) {
      .x = (int16_t)(sin_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.y,
    };
    GPoint sec_tail = (GPoint) {
      .x = (int16_t)(-sin_lookup(sec_angle) * tail_len / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(cos_lookup(sec_angle) * tail_len / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_color(ctx, accent_color);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, sec_tail, sec_tip);
  }

  // Bold center pin
  graphics_context_set_fill_color(ctx, fg_color);
  graphics_fill_circle(ctx, center, 6);
  graphics_context_set_fill_color(ctx, accent_color);
  graphics_fill_circle(ctx, center, 2);
}

// -------------------------------------------------------------
// Style 3: 7-Segment Digital Clock
// -------------------------------------------------------------
static const uint8_t s_7seg_masks[10] = {
  0x3F, // 0: A B C D E F
  0x06, // 1: B C
  0x5B, // 2: A B G E D
  0x4F, // 3: A B G C D
  0x66, // 4: F G B C
  0x6D, // 5: A F G C D
  0x7D, // 6: A F E D C G
  0x07, // 7: A B C
  0x7F, // 8: A B C D E F G
  0x6F  // 9: A B C D F G
};

static void prv_draw_7seg_digit(GContext *ctx, int x, int y, int w, int h, int t, uint8_t digit, GColor on_col, GColor off_col) {
  uint8_t mask = (digit <= 9) ? s_7seg_masks[digit] : 0;
  int half_h = (h - 3 * t) / 2;

  // Seg A (top)
  graphics_context_set_fill_color(ctx, (mask & 0x01) ? on_col : off_col);
  graphics_fill_rect(ctx, GRect(x + t, y, w - 2 * t, t), 0, GCornerNone);

  // Seg B (top-right)
  graphics_context_set_fill_color(ctx, (mask & 0x02) ? on_col : off_col);
  graphics_fill_rect(ctx, GRect(x + w - t, y + t, t, half_h), 0, GCornerNone);

  // Seg C (bottom-right)
  graphics_context_set_fill_color(ctx, (mask & 0x04) ? on_col : off_col);
  graphics_fill_rect(ctx, GRect(x + w - t, y + t + half_h + t, t, half_h), 0, GCornerNone);

  // Seg D (bottom)
  graphics_context_set_fill_color(ctx, (mask & 0x08) ? on_col : off_col);
  graphics_fill_rect(ctx, GRect(x + t, y + h - t, w - 2 * t, t), 0, GCornerNone);

  // Seg E (bottom-left)
  graphics_context_set_fill_color(ctx, (mask & 0x10) ? on_col : off_col);
  graphics_fill_rect(ctx, GRect(x, y + t + half_h + t, t, half_h), 0, GCornerNone);

  // Seg F (top-left)
  graphics_context_set_fill_color(ctx, (mask & 0x20) ? on_col : off_col);
  graphics_fill_rect(ctx, GRect(x, y + t, t, half_h), 0, GCornerNone);

  // Seg G (middle)
  graphics_context_set_fill_color(ctx, (mask & 0x40) ? on_col : off_col);
  graphics_fill_rect(ctx, GRect(x + t, y + t + half_h, w - 2 * t, t), 0, GCornerNone);
}

static void prv_draw_7seg_clock(GContext *ctx, GRect bounds, struct tm *tick_time, bool is_24h, bool is_dark, GColor fg_color) {
  int hour = tick_time->tm_hour;
  if (!is_24h) {
    hour = hour % 12;
    if (hour == 0) hour = 12;
  }
  int h1 = hour / 10;
  int h2 = hour % 10;
  int m1 = tick_time->tm_min / 10;
  int m2 = tick_time->tm_min % 10;
  int s1 = tick_time->tm_sec / 10;
  int s2 = tick_time->tm_sec % 10;

  bool show_sec = game_state_get()->data.option_show_seconds;

  int dw, dh, dt, gap, colon_w;
  int dw_s = 0, dh_s = 0, dt_s = 0, gap_s = 0, colon_s = 0;
  int total_w = 0;

  if (bounds.size.w >= 200) {
    if (show_sec) {
      dw = 20; dh = 44; dt = 4; gap = 5; colon_w = 8;
      dw_s = 13; dh_s = 26; dt_s = 3; gap_s = 3; colon_s = 6;
      total_w = (dw * 4) + (gap * 2) + colon_w + 6 + (dw_s * 2) + gap_s + colon_s;
    } else {
      dw = 22; dh = 48; dt = 5; gap = 6; colon_w = 10;
      total_w = (dw * 4) + (gap * 2) + colon_w;
    }
  } else {
    if (show_sec) {
      dw = 16; dh = 38; dt = 3; gap = 4; colon_w = 6;
      dw_s = 10; dh_s = 22; dt_s = 2; gap_s = 2; colon_s = 4;
      total_w = (dw * 4) + (gap * 2) + colon_w + 5 + (dw_s * 2) + gap_s + colon_s;
    } else {
      dw = 20; dh = 44; dt = 4; gap = 5; colon_w = 8;
      total_w = (dw * 4) + (gap * 2) + colon_w;
    }
  }

  int start_x = (bounds.size.w - total_w) / 2;
  int start_y = (bounds.size.h / 2) - (dh / 2) - 8;

  GColor off_color = is_dark ? GColorDarkGray : GColorLightGray;

  // Hour digit 1
  if (!is_24h && h1 == 0) {
    prv_draw_7seg_digit(ctx, start_x, start_y, dw, dh, dt, 10, fg_color, off_color);
  } else {
    prv_draw_7seg_digit(ctx, start_x, start_y, dw, dh, dt, (uint8_t)h1, fg_color, off_color);
  }

  // Hour digit 2
  int cur_x = start_x + dw + gap;
  prv_draw_7seg_digit(ctx, cur_x, start_y, dw, dh, dt, (uint8_t)h2, fg_color, off_color);

  // Main Colon
  cur_x += dw;
  int dot_size = dt;
  int col_x = cur_x + (colon_w - dot_size) / 2;
  graphics_context_set_fill_color(ctx, fg_color);
  graphics_fill_rect(ctx, GRect(col_x, start_y + (dh / 3) - dot_size / 2, dot_size, dot_size), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(col_x, start_y + ((dh * 2) / 3) - dot_size / 2, dot_size, dot_size), 0, GCornerNone);

  // Minute digit 1
  cur_x += colon_w;
  prv_draw_7seg_digit(ctx, cur_x, start_y, dw, dh, dt, (uint8_t)m1, fg_color, off_color);

  // Minute digit 2
  cur_x += dw + gap;
  prv_draw_7seg_digit(ctx, cur_x, start_y, dw, dh, dt, (uint8_t)m2, fg_color, off_color);

  // In-line seconds digits if enabled
  if (show_sec) {
    cur_x += dw + (bounds.size.w >= 200 ? 6 : 5);
    int sec_y = start_y + (dh - dh_s); // aligned to bottom baseline

    // Small seconds colon
    int s_dot = dt_s;
    int s_col_x = cur_x + (colon_s - s_dot) / 2;
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_fill_rect(ctx, GRect(s_col_x, sec_y + (dh_s / 3) - s_dot / 2, s_dot, s_dot), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(s_col_x, sec_y + ((dh_s * 2) / 3) - s_dot / 2, s_dot, s_dot), 0, GCornerNone);

    cur_x += colon_s;
    prv_draw_7seg_digit(ctx, cur_x, sec_y, dw_s, dh_s, dt_s, (uint8_t)s1, fg_color, off_color);
    cur_x += dw_s + gap_s;
    prv_draw_7seg_digit(ctx, cur_x, sec_y, dw_s, dh_s, dt_s, (uint8_t)s2, fg_color, off_color);
  }

  char date_buf[32];
  strftime(date_buf, sizeof(date_buf), "%a, %b %e", tick_time);
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, date_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, start_y + dh + 10, bounds.size.w, 22),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

// -------------------------------------------------------------
// Style 4: Word Clock
// -------------------------------------------------------------
static const char * const s_word_hours[12] = {
  "TWELVE", "ONE", "TWO", "THREE", "FOUR", "FIVE",
  "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN"
};

static const char * const s_word_teens[10] = {
  "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN",
  "FIFTEEN", "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN"
};

static const char * const s_word_tens[4] = {
  "TWENTY", "THIRTY", "FORTY", "FIFTY"
};

static const char * const s_word_units[10] = {
  "", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"
};

static void prv_draw_word_clock(GContext *ctx, GRect bounds, struct tm *tick_time, GColor fg_color) {
  int hour = tick_time->tm_hour % 12;
  int min = tick_time->tm_min;

  char word_buf[64];
  const char *hour_name = s_word_hours[hour];

  if (min == 0) {
    snprintf(word_buf, sizeof(word_buf), "IT IS\n%s\nO'CLOCK", hour_name);
  } else if (min < 10) {
    snprintf(word_buf, sizeof(word_buf), "IT IS\n%s\nOH %s", hour_name, s_word_units[min]);
  } else if (min < 20) {
    snprintf(word_buf, sizeof(word_buf), "IT IS\n%s\n%s", hour_name, s_word_teens[min - 10]);
  } else {
    int t_idx = (min / 10) - 2;
    int u_idx = min % 10;
    if (u_idx == 0) {
      snprintf(word_buf, sizeof(word_buf), "IT IS\n%s\n%s", hour_name, s_word_tens[t_idx]);
    } else {
      snprintf(word_buf, sizeof(word_buf), "IT IS\n%s\n%s %s", hour_name, s_word_tens[t_idx], s_word_units[u_idx]);
    }
  }

  int y = (bounds.size.h / 2) - 48;
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, word_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(4, y, bounds.size.w - 8, 90),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

// -------------------------------------------------------------
// Style 5: "Now" Clock
// -------------------------------------------------------------
static void prv_draw_now_clock(GContext *ctx, GRect bounds, struct tm *tick_time, GColor fg_color, GColor accent_color) {
  int center_y = (bounds.size.h / 2) - 28;

  graphics_context_set_text_color(ctx, accent_color);
  graphics_draw_text(ctx, "THE TIME IS",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, center_y - 20, bounds.size.w, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, "NOW",
                     fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(0, center_y - 2, bounds.size.w, 50),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  if (game_state_get()->data.option_show_seconds) {
    int sec = tick_time->tm_sec;
    int bar_w = bounds.size.w - 40;
    int bar_x = 20;
    int bar_y = center_y + 50;
    int fill_w = (sec * bar_w) / 60;
    graphics_context_set_fill_color(ctx, accent_color);
    graphics_fill_rect(ctx, GRect(bar_x, bar_y, fill_w, 2), 0, GCornerNone);
  }
}

// -------------------------------------------------------------
// Clock Layer Update Proc
// -------------------------------------------------------------
static void prv_clock_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  GColor bg_color = game_state_get_bg_color();
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Boost liquid ink wave animation behind the clock
  if (game->current_boost_milli > 0) {
    GColor ink_color = game_state_get_ink_color();
    static uint16_t s_clock_wave_phase = 0;
    s_clock_wave_phase += 1400;

    uint32_t max_limit_milli = game_state_get_boost_max_limit_percent() * 1000;
    if (max_limit_milli == 0) max_limit_milli = 100000;

    int liquid_h = (int)(((int32_t)bounds.size.h * (int32_t)game->current_boost_milli) / (int32_t)max_limit_milli);
    if (liquid_h > bounds.size.h) liquid_h = bounds.size.h;
    if (liquid_h < 4) liquid_h = 4;

    int surface_y = bounds.size.h - liquid_h;
    int solid_fill_y = surface_y + 4;

    graphics_context_set_stroke_color(ctx, ink_color);
    for (int x = 0; x < bounds.size.w; x++) {
      int32_t angle = (x * 1100) + s_clock_wave_phase;
      int wave_offset = (sin_lookup(angle) * 3) / TRIG_MAX_RATIO;
      int y_top = surface_y + wave_offset;
      if (y_top < 0) y_top = 0;
      if (y_top < bounds.size.h) {
        int y_bottom = (solid_fill_y < bounds.size.h) ? solid_fill_y : bounds.size.h;
        graphics_draw_line(ctx, GPoint(x, y_top), GPoint(x, y_bottom));
      }
    }

    if (solid_fill_y < bounds.size.h) {
      graphics_context_set_fill_color(ctx, ink_color);
      graphics_fill_rect(ctx, GRect(0, solid_fill_y, bounds.size.w, bounds.size.h - solid_fill_y), 0, GCornerNone);
    }
  }

  bool is_dark = game_state_is_color_dark(bg_color);
  GColor fg_color = is_dark ? GColorWhite : GColorBlack;
  GColor accent_color = is_dark ? GColorCyan : GColorCobaltBlue;

  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  uint8_t style = game->data.selected_clock_style;
  switch (style) {
    case 1:
      prv_draw_square_analogue(ctx, bounds, tick_time, fg_color, accent_color);
      break;
    case 2:
      prv_draw_bold_minimalist(ctx, bounds, tick_time, fg_color, accent_color);
      break;
    case 3:
      prv_draw_7seg_clock(ctx, bounds, tick_time, game->data.option_24h, is_dark, fg_color);
      break;
    case 4:
      prv_draw_word_clock(ctx, bounds, tick_time, fg_color);
      break;
    case 5:
      prv_draw_now_clock(ctx, bounds, tick_time, fg_color, accent_color);
      break;
    case 0:
    default:
      prv_draw_big_digital(ctx, bounds, tick_time, game->data.option_24h, game->data.selected_font, fg_color);
      break;
  }

  // Mini Idle Pixel stats at bottom
  char idle_buf[48];
  if (game->current_boost_milli > 0) {
    uint32_t boost_whole = 0, boost_tenths = 0;
    game_state_get_current_boost_parts(&boost_whole, &boost_tenths);
    snprintf(idle_buf, sizeof(idle_buf), "PX %lu/64 [+%lu.%1lu%%] GP %lu",
             (unsigned long)game->data.current_pixels,
             (unsigned long)boost_whole, (unsigned long)boost_tenths,
             (unsigned long)game->data.gold_pixels);

    int pill_w = bounds.size.w >= 200 ? 186 : 138;
    GRect pill_rect = GRect((bounds.size.w - pill_w) / 2, bounds.size.h - 18, pill_w, 16);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, pill_rect, 2, GCornersAll);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, idle_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(0, bounds.size.h - 18, bounds.size.w, 16),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  } else {
    snprintf(idle_buf, sizeof(idle_buf), "PX %lu/64 | COL %u/64 | GP %lu",
             (unsigned long)game->data.current_pixels,
             (unsigned int)(game->data.current_color_idx + 1),
             (unsigned long)game->data.gold_pixels);
    graphics_context_set_text_color(ctx, fg_color);
    graphics_draw_text(ctx, idle_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, bounds.size.h - 18, bounds.size.w, 16),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_clock_layer) {
    layer_mark_dirty(s_clock_layer);
  }
}

static bool s_clock_down_held = false;

bool ui_clock_is_down_held(void) {
  return s_clock_down_held;
}

void ui_clock_mark_dirty(void) {
  if (s_clock_layer) {
    layer_mark_dirty(s_clock_layer);
  }
}

static void prv_clock_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  game_state_cycle_bg_color();
  if (s_clock_layer) layer_mark_dirty(s_clock_layer);
}

static void prv_clock_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  GameState *game = game_state_get();
  if (game->data.unlocked_clock_styles > 1) {
    game->data.selected_clock_style = (game->data.selected_clock_style + 1) % game->data.unlocked_clock_styles;
    game_state_save();
    game_state_weak_vibe();
    if (s_clock_layer) layer_mark_dirty(s_clock_layer);
  }
}

static void prv_clock_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  game_state_activate_boost();
  game_state_weak_vibe();
  if (s_clock_layer) layer_mark_dirty(s_clock_layer);
}

static void prv_clock_down_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
  s_clock_down_held = true;
  game_state_activate_boost();
  game_state_weak_vibe();
  if (s_clock_layer) layer_mark_dirty(s_clock_layer);
}

static void prv_clock_down_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_clock_down_held = false;
}

static void prv_clock_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_clock_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_clock_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_clock_down_click_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 400, prv_clock_down_long_click_down_handler, prv_clock_down_long_click_up_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_clock_layer = layer_create(bounds);
  layer_set_update_proc(s_clock_layer, prv_clock_update_proc);
  layer_add_child(window_layer, s_clock_layer);

  window_set_click_config_provider(window, prv_clock_click_config_provider);

  game_state_set_clock_active(true);
  TimeUnits units = game_state_get()->data.option_show_seconds ? SECOND_UNIT : MINUTE_UNIT;
  tick_timer_service_subscribe(units, prv_tick_handler);
}

static void prv_window_unload(Window *window) {
  s_clock_down_held = false;
  game_state_set_clock_active(false);
  tick_timer_service_unsubscribe();
  layer_destroy(s_clock_layer);
  s_clock_layer = NULL;
  window_destroy(s_clock_window);
  s_clock_window = NULL;
}

void ui_clock_show(void) {
  if (s_clock_window) {
    return;
  }
  s_clock_window = window_create();
  window_set_window_handlers(s_clock_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_clock_window, true);
}
