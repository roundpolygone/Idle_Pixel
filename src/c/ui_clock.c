#include "ui.h"
#include "game_state.h"

static Window *s_clock_window = NULL;
static Layer *s_clock_layer = NULL;

// -------------------------------------------------------------
// Style 0: Big Digital
// -------------------------------------------------------------
static void prv_draw_big_digital(GContext *ctx, GRect bounds, struct tm *tick_time, bool is_24h, uint8_t font_idx, uint8_t size_idx, GColor fg_color) {
  char time_buf[16];
  if (is_24h) {
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tick_time->tm_hour, tick_time->tm_min);
  } else {
    int hour = tick_time->tm_hour % 12;
    if (hour == 0) hour = 12;
    snprintf(time_buf, sizeof(time_buf), "%d:%02d", hour, tick_time->tm_min);
  }

  int digit_y = (bounds.size.h / 2) - 44;
  if (size_idx == 1) { // Large
    digit_y = (bounds.size.h / 2) - 48;
  } else if (size_idx == 2) { // Small
    digit_y = (bounds.size.h / 2) - 36;
  }
  graphics_context_set_text_color(ctx, fg_color);
  GFont time_font = game_state_get_clock_digit_font(font_idx, size_idx);

  if (game_state_get()->data.option_show_seconds) {
    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), " :%02d", tick_time->tm_sec);
    GFont sec_font = (size_idx == 1) ? fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD) : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    GSize time_size = graphics_text_layout_get_content_size(time_buf, time_font, GRect(0, 0, bounds.size.w, 56), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GSize sec_size = graphics_text_layout_get_content_size(sec_buf, sec_font, GRect(0, 0, bounds.size.w, 56), GTextOverflowModeWordWrap, GTextAlignmentLeft);

    int total_w = time_size.w + sec_size.w;
    if (total_w > bounds.size.w - 4) {
      sec_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
      sec_size = graphics_text_layout_get_content_size(sec_buf, sec_font, GRect(0, 0, bounds.size.w, 56), GTextOverflowModeWordWrap, GTextAlignmentLeft);
      total_w = time_size.w + sec_size.w;
    }

    int start_x = (bounds.size.w - total_w) / 2;
    graphics_draw_text(ctx, time_buf, time_font,
                       GRect(start_x, digit_y, time_size.w, 56),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, sec_buf, sec_font,
                       GRect(start_x + time_size.w, digit_y + (size_idx == 1 ? 18 : 10), sec_size.w, 32),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, time_buf, time_font,
                       GRect(0, digit_y, bounds.size.w, 56),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  char date_buf[32];
  strftime(date_buf, sizeof(date_buf), "%a, %b %e", tick_time);
  GFont date_font = game_state_get_clock_text_font(font_idx, size_idx, 1);
  int date_y = digit_y + (size_idx == 1 ? 54 : (size_idx == 2 ? 38 : 46));
  graphics_draw_text(ctx, date_buf,
                     date_font,
                     GRect(0, date_y, bounds.size.w, 24),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

// -------------------------------------------------------------
// -------------------------------------------------------------
// Style 1: Square Analogue Clock
// -------------------------------------------------------------
static void prv_draw_square_analogue(GContext *ctx, GRect bounds, struct tm *tick_time, uint8_t font_idx, uint8_t size_idx, GColor fg_color, GColor accent_color) {
  GPoint center = grect_center_point(&bounds);
  center.y -= 12;

  int half_size = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 16;
  GRect dial_rect = GRect(center.x - half_size, center.y - half_size, half_size * 2, half_size * 2);

  // Font size alters border & hand thickness:
  // size_idx: 0 = Medium (default), 1 = Large, 2 = Small
  int border_w = (size_idx == 1) ? 3 : ((size_idx == 2) ? 1 : 2);
  int hour_hand_w = (size_idx == 1) ? 5 : ((size_idx == 2) ? 2 : 3);
  int min_hand_w = (size_idx == 1) ? 3 : ((size_idx == 2) ? 1 : 2);
  int sec_hand_w = (size_idx == 1) ? 2 : 1;

  // Outer square bezel
  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_stroke_width(ctx, border_w);
  graphics_draw_round_rect(ctx, dial_rect, 4);

  // Hour numbers font
  GFont num_font = game_state_get_clock_text_font(font_idx, size_idx, 0);

  // 12 hours mapped along the square perimeter with inward ticks
  for (int h = 1; h <= 12; h++) {
    int px = 0, py = 0;
    int tick_dx = 0, tick_dy = 0;
    int tick_len = (h % 3 == 0) ? 5 : 3;

    switch (h) {
      case 12: px = center.x; py = dial_rect.origin.y; tick_dy = 1; break;
      case 1:  px = center.x + half_size / 2; py = dial_rect.origin.y; tick_dy = 1; break;
      case 2:  px = dial_rect.origin.x + dial_rect.size.w; py = center.y - half_size / 2; tick_dx = -1; break;
      case 3:  px = dial_rect.origin.x + dial_rect.size.w; py = center.y; tick_dx = -1; break;
      case 4:  px = dial_rect.origin.x + dial_rect.size.w; py = center.y + half_size / 2; tick_dx = -1; break;
      case 5:  px = center.x + half_size / 2; py = dial_rect.origin.y + dial_rect.size.h; tick_dy = -1; break;
      case 6:  px = center.x; py = dial_rect.origin.y + dial_rect.size.h; tick_dy = -1; break;
      case 7:  px = center.x - half_size / 2; py = dial_rect.origin.y + dial_rect.size.h; tick_dy = -1; break;
      case 8:  px = dial_rect.origin.x; py = center.y + half_size / 2; tick_dx = 1; break;
      case 9:  px = dial_rect.origin.x; py = center.y; tick_dx = 1; break;
      case 10: px = dial_rect.origin.x; py = center.y - half_size / 2; tick_dx = 1; break;
      case 11: px = center.x - half_size / 2; py = dial_rect.origin.y; tick_dy = 1; break;
    }

    // Tick mark pointing inward
    graphics_context_set_stroke_width(ctx, (h % 3 == 0) ? 2 : 1);
    graphics_draw_line(ctx, GPoint(px, py), GPoint(px + tick_dx * tick_len, py + tick_dy * tick_len));

    // Number position
    int num_dist = 11;
    int nx = px + tick_dx * num_dist;
    int ny = py + tick_dy * num_dist;
    if (h == 12) ny += 2;
    if (h == 6)  ny -= 4;
    if (h == 3)  nx -= 3;
    if (h == 9)  nx += 3;

    char num_str[4];
    snprintf(num_str, sizeof(num_str), "%d", h);
    graphics_context_set_text_color(ctx, fg_color);
    graphics_draw_text(ctx, num_str, num_font,
                       GRect(nx - 8, ny - 7, 16, 14),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  }

  // Corner subtle marks
  int corner_inset = 4;
  graphics_draw_pixel(ctx, GPoint(dial_rect.origin.x + corner_inset, dial_rect.origin.y + corner_inset));
  graphics_draw_pixel(ctx, GPoint(dial_rect.origin.x + dial_rect.size.w - corner_inset - 1, dial_rect.origin.y + corner_inset));
  graphics_draw_pixel(ctx, GPoint(dial_rect.origin.x + corner_inset, dial_rect.origin.y + dial_rect.size.h - corner_inset - 1));
  graphics_draw_pixel(ctx, GPoint(dial_rect.origin.x + dial_rect.size.w - corner_inset - 1, dial_rect.origin.y + dial_rect.size.h - corner_inset - 1));

  // Hour Hand
  int32_t hour_angle = (TRIG_MAX_ANGLE * (((tick_time->tm_hour % 12) * 6) + (tick_time->tm_min / 10))) / (12 * 6);
  int16_t hour_len = half_size * 52 / 100;
  GPoint hour_pt = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_stroke_width(ctx, hour_hand_w);
  graphics_draw_line(ctx, center, hour_pt);

  // Minute Hand
  int32_t min_angle = (TRIG_MAX_ANGLE * tick_time->tm_min) / 60;
  int16_t min_len = half_size * 80 / 100;
  GPoint min_pt = (GPoint) {
    .x = (int16_t)(sin_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_width(ctx, min_hand_w);
  graphics_draw_line(ctx, center, min_pt);

  // Second Hand (if enabled in options)
  if (game_state_get()->data.option_show_seconds) {
    int32_t sec_angle = (TRIG_MAX_ANGLE * tick_time->tm_sec) / 60;
    int16_t sec_len = half_size * 88 / 100;
    GPoint sec_pt = (GPoint) {
      .x = (int16_t)(sin_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_color(ctx, accent_color);
    graphics_context_set_stroke_width(ctx, sec_hand_w);
    graphics_draw_line(ctx, center, sec_pt);
  }

  // Center pin
  int pin_r = (size_idx == 1) ? 5 : ((size_idx == 2) ? 3 : 4);
  graphics_context_set_fill_color(ctx, fg_color);
  graphics_fill_circle(ctx, center, pin_r);
  graphics_context_set_fill_color(ctx, accent_color);
  graphics_fill_circle(ctx, center, 1);
}

// -------------------------------------------------------------
// Style 2: Bold Minimalist Analogue Clock (detached floating hands, no center dot)
// -------------------------------------------------------------
static void prv_draw_bold_minimalist(GContext *ctx, GRect bounds, struct tm *tick_time, uint8_t size_idx, GColor fg_color, GColor accent_color) {
  GPoint center = grect_center_point(&bounds);
  center.y -= 12;
  int16_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 16;

  // Font size alters border & hand thickness:
  // size_idx: 0 = Medium (default), 1 = Large, 2 = Small
  int dot_r = (size_idx == 1) ? 4 : ((size_idx == 2) ? 2 : 3);
  int hour_w = (size_idx == 1) ? 8 : ((size_idx == 2) ? 4 : 6);
  int min_w = (size_idx == 1) ? 6 : ((size_idx == 2) ? 2 : 4);
  int sec_w = (size_idx == 1) ? 3 : ((size_idx == 2) ? 1 : 2);

  // Outer border altered by font size
  if (size_idx == 1) {
    graphics_context_set_stroke_color(ctx, fg_color);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_circle(ctx, center, radius + 2);
  } else if (size_idx == 0) {
    graphics_context_set_stroke_color(ctx, fg_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, center, radius + 2);
  }

  // 4 bold dots at 12, 3, 6, 9
  graphics_context_set_fill_color(ctx, fg_color);
  for (int i = 0; i < 4; i++) {
    int32_t angle = (TRIG_MAX_ANGLE * i) / 4;
    int16_t dot_dist = radius - 4;
    int16_t dx = (int16_t)(sin_lookup(angle) * dot_dist / TRIG_MAX_RATIO) + center.x;
    int16_t dy = (int16_t)(-cos_lookup(angle) * dot_dist / TRIG_MAX_RATIO) + center.y;
    graphics_fill_circle(ctx, GPoint(dx, dy), dot_r);
  }

  // Floating Bold Hour Hand (detached from center)
  int32_t hour_angle = (TRIG_MAX_ANGLE * (((tick_time->tm_hour % 12) * 6) + (tick_time->tm_min / 10))) / (12 * 6);
  int16_t hour_start_dist = 14;
  int16_t hour_end_dist = radius * 55 / 100;
  GPoint hour_start = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * hour_start_dist / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * hour_start_dist / TRIG_MAX_RATIO) + center.y,
  };
  GPoint hour_end = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * hour_end_dist / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * hour_end_dist / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_stroke_width(ctx, hour_w);
  graphics_draw_line(ctx, hour_start, hour_end);

  // Floating Bold Minute Hand (detached from center)
  int32_t min_angle = (TRIG_MAX_ANGLE * tick_time->tm_min) / 60;
  int16_t min_start_dist = 16;
  int16_t min_end_dist = radius * 85 / 100;
  GPoint min_start = (GPoint) {
    .x = (int16_t)(sin_lookup(min_angle) * min_start_dist / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(min_angle) * min_start_dist / TRIG_MAX_RATIO) + center.y,
  };
  GPoint min_end = (GPoint) {
    .x = (int16_t)(sin_lookup(min_angle) * min_end_dist / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(min_angle) * min_end_dist / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_width(ctx, min_w);
  graphics_draw_line(ctx, min_start, min_end);

  // Floating accent needle (detached, if enabled in options)
  if (game_state_get()->data.option_show_seconds) {
    int32_t sec_angle = (TRIG_MAX_ANGLE * tick_time->tm_sec) / 60;
    int16_t sec_start_dist = 18;
    int16_t sec_end_dist = radius * 90 / 100;
    GPoint sec_start = (GPoint) {
      .x = (int16_t)(sin_lookup(sec_angle) * sec_start_dist / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(sec_angle) * sec_start_dist / TRIG_MAX_RATIO) + center.y,
    };
    GPoint sec_end = (GPoint) {
      .x = (int16_t)(sin_lookup(sec_angle) * sec_end_dist / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(sec_angle) * sec_end_dist / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_color(ctx, accent_color);
    graphics_context_set_stroke_width(ctx, sec_w);
    graphics_draw_line(ctx, sec_start, sec_end);
  }

  // No center dot: completely open floating center
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

static void prv_draw_7seg_clock(GContext *ctx, GRect bounds, struct tm *tick_time, uint8_t font_idx, uint8_t size_idx, bool is_24h, bool is_dark, GColor fg_color) {
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
      dw = 20;
      dh = (size_idx == 1 ? 86 : (size_idx == 2 ? 48 : 66));
      dt = (size_idx == 1 ? 5 : (size_idx == 2 ? 3 : 4));
      gap = 5; colon_w = 8;
      dw_s = 13; dh_s = dh * 60 / 100; dt_s = 3; gap_s = 3; colon_s = 6;
      total_w = (dw * 4) + (gap * 2) + colon_w + 6 + (dw_s * 2) + gap_s + colon_s;
    } else {
      dw = 24;
      dh = (size_idx == 1 ? 96 : (size_idx == 2 ? 54 : 72));
      dt = (size_idx == 1 ? 6 : (size_idx == 2 ? 4 : 5));
      gap = 6; colon_w = 10;
      total_w = (dw * 4) + (gap * 2) + colon_w;
    }
  } else {
    if (show_sec) {
      dw = 16;
      dh = (size_idx == 1 ? 68 : (size_idx == 2 ? 38 : 52));
      dt = (size_idx == 1 ? 4 : (size_idx == 2 ? 3 : 3));
      gap = 4; colon_w = 6;
      dw_s = 11; dh_s = dh * 60 / 100; dt_s = 2; gap_s = 2; colon_s = 5;
      total_w = (dw * 4) + (gap * 2) + colon_w + 5 + (dw_s * 2) + gap_s + colon_s;
    } else {
      dw = 20;
      dh = (size_idx == 1 ? 76 : (size_idx == 2 ? 42 : 58));
      dt = (size_idx == 1 ? 5 : (size_idx == 2 ? 3 : 4));
      gap = 5; colon_w = 8;
      total_w = (dw * 4) + (gap * 2) + colon_w;
    }
  }

  int start_x = (bounds.size.w - total_w) / 2;
  int start_y = (bounds.size.h / 2) - (dh / 2) - 12;

  // High-contrast off color: black against white, white against black
  GColor off_color = is_dark ? GColorBlack : GColorWhite;

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
  GFont date_font = game_state_get_clock_text_font(font_idx, size_idx, 1);
  graphics_draw_text(ctx, date_buf,
                     date_font,
                     GRect(0, start_y + dh + 6, bounds.size.w, 24),
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

static void prv_draw_word_clock(GContext *ctx, GRect bounds, struct tm *tick_time, uint8_t font_idx, uint8_t size_idx, GColor fg_color) {
  int hour = tick_time->tm_hour % 12;
  int min = tick_time->tm_min;

  char word_buf[64];
  const char *hour_name = s_word_hours[hour];

  if (min == 0) {
    snprintf(word_buf, sizeof(word_buf), "IT IS\n%s\nO'CLOCK", hour_name);
  } else if (min < 10) {
    snprintf(word_buf, sizeof(word_buf), "IT IS\n%s\nO' %s", hour_name, s_word_units[min]);
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

  int y = (bounds.size.h / 2) - 52;
  GFont word_font = game_state_get_clock_text_font(font_idx, size_idx, 2);
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, word_buf,
                     word_font,
                     GRect(2, y, bounds.size.w - 4, 100),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

// -------------------------------------------------------------
// Style 5: "Now" Clock
// -------------------------------------------------------------
static void prv_draw_now_clock(GContext *ctx, GRect bounds, struct tm *tick_time, uint8_t font_idx, uint8_t size_idx, GColor fg_color, GColor accent_color) {
  int center_y = (bounds.size.h / 2) - 28;

  GFont sub_font = game_state_get_clock_text_font(font_idx, size_idx, 1);
  graphics_context_set_text_color(ctx, accent_color);
  graphics_draw_text(ctx, "THE TIME IS",
                     sub_font,
                     GRect(0, center_y - 20, bounds.size.w, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  GFont now_font = game_state_get_clock_digit_font(font_idx, size_idx);
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, "NOW",
                     now_font,
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
  uint8_t font_idx = game->data.selected_font;
  uint8_t size_idx = game->data.selected_font_size;
  switch (style) {
    case 1:
      prv_draw_square_analogue(ctx, bounds, tick_time, font_idx, size_idx, fg_color, accent_color);
      break;
    case 2:
      prv_draw_bold_minimalist(ctx, bounds, tick_time, size_idx, fg_color, accent_color);
      break;
    case 3:
      prv_draw_7seg_clock(ctx, bounds, tick_time, font_idx, size_idx, game->data.option_24h, is_dark, fg_color);
      break;
    case 4:
      prv_draw_word_clock(ctx, bounds, tick_time, font_idx, size_idx, fg_color);
      break;
    case 5:
      prv_draw_now_clock(ctx, bounds, tick_time, font_idx, size_idx, fg_color, accent_color);
      break;
    case 0:
    default:
      prv_draw_big_digital(ctx, bounds, tick_time, game->data.option_24h, font_idx, size_idx, fg_color);
      break;
  }

  // Mini Idle Pixel stats at bottom: show PX, CLR, CRN amounts | boost %
  char stats_buf[48];
  uint32_t boost_whole = 0, boost_tenths = 0;
  game_state_get_current_boost_parts(&boost_whole, &boost_tenths);

  if (game->current_boost_milli > 0) {
    snprintf(stats_buf, sizeof(stats_buf), "PX %lu CLR %u CRN %lu | +%lu%%",
             (unsigned long)game->data.current_pixels,
             (unsigned int)(game->data.current_color_idx + 1),
             (unsigned long)game_state_get_available_crowns(),
             (unsigned long)boost_whole);
  } else {
    snprintf(stats_buf, sizeof(stats_buf), "PX %lu CLR %u CRN %lu | 0%%",
             (unsigned long)game->data.current_pixels,
             (unsigned int)(game->data.current_color_idx + 1),
             (unsigned long)game_state_get_available_crowns());
  }

  int badge_h = 20;
  int badge_y = bounds.size.h - badge_h - 2;
  int badge_w = bounds.size.w - 6;
  GRect badge_rect = GRect((bounds.size.w - badge_w) / 2, badge_y, badge_w, badge_h);

  // Background pill badge for clear readability over any background/boost wave
  graphics_context_set_fill_color(ctx, is_dark ? GColorBlack : GColorWhite);
  graphics_fill_rect(ctx, badge_rect, 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, badge_rect, 4);

  GFont stat_font = game_state_get_clock_text_font(font_idx, size_idx, 0);
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, stats_buf,
                     stat_font,
                     GRect(0, badge_y + 1, bounds.size.w, badge_h),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
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
