#include "ui_grid.h"
#include "game_state.h"

static Layer *s_canvas_layer = NULL;

static void ui_grid_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  // Background
  GColor bg_color = game_state_get_bg_color();
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  bool is_dark = game_state_is_color_dark(bg_color);
  GColor fg_color = is_dark ? GColorWhite : GColorBlack;
  GColor panel_bg = GColorBlack;

  // Grid dimensions
  int cell_size = 13;
  int gap = 1;
  int step = cell_size + gap; // 14
  int grid_w = (GRID_SIZE * step) - gap; // 111
  int grid_h = grid_w;

  if (bounds.size.w >= 200) {
    cell_size = 16;
    gap = 2;
    step = cell_size + gap;
    grid_w = (GRID_SIZE * step) - gap;
    grid_h = grid_w;
  }

  int grid_x = (bounds.size.w - grid_w) / 2;
  int header_y = 1;
  int grid_y = 26;
  int footer_y = grid_y + grid_h + 3;

#if defined(PBL_ROUND)
  header_y = 10;
  grid_y = 33;
  footer_y = grid_y + grid_h + 2;
#endif

  // Sparkles in the background when active boost
  if (game->boost_remaining_ms > 0) {
    static const int16_t s_sparkle_offsets[16][2] = {
      { 6, 8 }, { 24, 12 }, { 120, 10 }, { 136, 14 },
      { 8, 50 }, { 136, 52 }, { 6, 90 }, { 138, 92 },
      { 8, 130 }, { 136, 132 }, { 18, 158 }, { 40, 162 },
      { 72, 164 }, { 104, 160 }, { 128, 158 }, { 70, 8 }
    };

    uint32_t phase = (game->boost_remaining_ms / 100);

    for (int i = 0; i < 16; i++) {
      if (((i + phase) % 3) == 0) {
        continue;
      }

      int16_t sx = s_sparkle_offsets[i][0];
      int16_t sy = s_sparkle_offsets[i][1];

      if (bounds.size.w > 144) {
        sx = (sx * bounds.size.w) / 144;
        sy = (sy * bounds.size.h) / 168;
      }

      if (sx >= grid_x - 3 && sx <= grid_x + grid_w + 3 &&
          sy >= grid_y - 3 && sy <= grid_y + grid_h + 3) {
        continue;
      }

      GColor spark_col = ((i + phase) % 2 == 0) ? GColorWhite : GColorYellow;
      graphics_context_set_stroke_color(ctx, spark_col);

      graphics_draw_pixel(ctx, GPoint(sx, sy));
      graphics_draw_pixel(ctx, GPoint(sx + 1, sy));
      graphics_draw_pixel(ctx, GPoint(sx - 1, sy));
      graphics_draw_pixel(ctx, GPoint(sx, sy + 1));
      graphics_draw_pixel(ctx, GPoint(sx, sy - 1));
    }
  }

  // 1. Header: Fill Counter
  char header_buf[32];
  snprintf(header_buf, sizeof(header_buf), "FILL %lu / %d",
           (unsigned long)game->data.fills_count, MAX_FILLS);

  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, header_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, header_y, bounds.size.w, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // 2. Mini Progress Bar
  int bar_y = grid_y - 4;
  int bar_h = 3;
  graphics_context_set_fill_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_fill_rect(ctx, GRect(grid_x, bar_y, grid_w, bar_h), 1, GCornersAll);

  uint32_t base_int = game_state_get_base_interval_ms();
  int filled_bar_w = (int)((game->data.progress_ms * (uint32_t)grid_w) / base_int);
  if (filled_bar_w > grid_w) {
    filled_bar_w = grid_w;
  }
  if (filled_bar_w > 0) {
    graphics_context_set_fill_color(ctx, (game->boost_remaining_ms > 0) ? GColorOrange : GColorCobaltBlue);
    graphics_fill_rect(ctx, GRect(grid_x, bar_y, filled_bar_w, bar_h), 1, GCornersAll);
  }

  // 3. Grid Background Panel
  graphics_context_set_fill_color(ctx, panel_bg);
  graphics_fill_rect(ctx, GRect(grid_x - 2, grid_y - 2, grid_w + 4, grid_h + 4), 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_draw_rect(ctx, GRect(grid_x - 2, grid_y - 2, grid_w + 4, grid_h + 4));

  // 4. Render 8x8 Grid with active Fill Order
  uint32_t cur_pix = game->data.current_pixels;

  for (uint8_t row = 0; row < GRID_SIZE; row++) {
    for (uint8_t col = 0; col < GRID_SIZE; col++) {
      uint8_t seq_idx = game_state_get_order_index(row, col);
      int x = grid_x + (col * step);
      int y = grid_y + (row * step);
      GRect cell_rect = GRect(x, y, cell_size, cell_size);

      if (seq_idx < cur_pix) {
        uint8_t color_idx = (uint8_t)(row * GRID_SIZE + col);
        GColor pixel_color = game_state_get_pixel_color(color_idx);
        graphics_context_set_fill_color(ctx, pixel_color);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);
      } else {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);
        graphics_context_set_stroke_color(ctx, GColorDarkGray);
        graphics_draw_rect(ctx, cell_rect);

        if (seq_idx == cur_pix) {
          graphics_context_set_stroke_color(ctx, GColorWhite);
          graphics_draw_rect(ctx, cell_rect);
        }
      }
    }
  }

  // 5. Footer: Stat "Pixels per second" & status
  uint32_t pps_whole = 0, pps_frac = 0;
  game_state_get_pps(&pps_whole, &pps_frac);

  char pps_buf[48];
  if (game->boost_remaining_ms > 0) {
    snprintf(pps_buf, sizeof(pps_buf), "%lu.%02lu px/s [+%lu%%]",
             (unsigned long)pps_whole, (unsigned long)pps_frac,
             (unsigned long)game_state_get_boost_percent());
    GRect badge_rect = GRect((bounds.size.w - 130) / 2, footer_y, 130, 14);
    graphics_context_set_fill_color(ctx, GColorOrange);
    graphics_fill_rect(ctx, badge_rect, 2, GCornersAll);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, pps_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       badge_rect,
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  } else {
    snprintf(pps_buf, sizeof(pps_buf), "%lu.%02lu px/s",
             (unsigned long)pps_whole, (unsigned long)pps_frac);
    graphics_context_set_text_color(ctx, fg_color);
    graphics_draw_text(ctx, pps_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(0, footer_y, bounds.size.w, 14),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  }

  // Navigation hint line showing UP: Clock, MID: Menu, DOWN: Boost
  graphics_context_set_text_color(ctx, is_dark ? GColorLightGray : GColorDarkGray);
  graphics_draw_text(ctx, "^ Clock | = Menu | v Boost",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, footer_y + 13, bounds.size.w, 14),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

void ui_grid_init(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, ui_grid_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

void ui_grid_deinit(void) {
  if (s_canvas_layer) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
  }
}

void ui_grid_mark_dirty(void) {
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}
