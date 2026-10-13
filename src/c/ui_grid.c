#include "ui_grid.h"
#include "game_state.h"

static Layer *s_canvas_layer = NULL;

static void ui_grid_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  // 1. Background
  GColor bg_color = game_state_get_bg_color();
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  bool is_dark = game_state_is_color_dark(bg_color);
  GColor fg_color = is_dark ? GColorWhite : GColorBlack;
  GColor panel_bg = is_dark ? GColorBlack : GColorWhite;
  GColor empty_cell_bg = is_dark ? GColorBlack : GColorWhite;
  GColor empty_cell_stroke = is_dark ? GColorDarkGray : GColorLightGray;
  GColor active_cell_stroke = is_dark ? GColorWhite : GColorBlack;

  // 2. Sizing & Layout calculations
  int cell_size = 13;
  int gap = 1;
  int step = cell_size + gap; // 14
  int grid_w = (GRID_SIZE * step) - gap; // 111
  int grid_h = grid_w;
  int bar_w = 6;
  int bar_gap = 5;

  if (bounds.size.w >= 200) {
    cell_size = 16;
    gap = 2;
    step = cell_size + gap;
    grid_w = (GRID_SIZE * step) - gap;
    grid_h = grid_w;
    bar_w = 8;
    bar_gap = 6;
  }

  int total_content_w = bar_w + bar_gap + grid_w;
  int bar_x = (bounds.size.w - total_content_w) / 2;
  int grid_x = bar_x + bar_w + bar_gap;

  int header_y = 1;
  int grid_y = 26;
  int footer_y = grid_y + grid_h + 3;

#if defined(PBL_ROUND)
  header_y = 10;
  grid_y = 33;
  footer_y = grid_y + grid_h + 2;
#endif

  // 3. Sparkles in the background during active boost
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

      if (sx >= bar_x - 3 && sx <= grid_x + grid_w + 3 &&
          sy >= grid_y - 3 && sy <= grid_y + grid_h + 3) {
        continue;
      }

      GColor spark_col = ((i + phase) % 2 == 0) ? (is_dark ? GColorWhite : GColorCobaltBlue) : (is_dark ? GColorYellow : GColorOrange);
      graphics_context_set_stroke_color(ctx, spark_col);

      graphics_draw_pixel(ctx, GPoint(sx, sy));
      graphics_draw_pixel(ctx, GPoint(sx + 1, sy));
      graphics_draw_pixel(ctx, GPoint(sx - 1, sy));
      graphics_draw_pixel(ctx, GPoint(sx, sy + 1));
      graphics_draw_pixel(ctx, GPoint(sx, sy - 1));
    }
  }

  // 4. Header: Active Color & Gold Pixels
  char header_buf[32];
  if (game->data.gold_pixels > 0) {
    snprintf(header_buf, sizeof(header_buf), "COL %u/64 | GP %lu",
             (unsigned int)(game->data.current_color_idx + 1),
             (unsigned long)game->data.gold_pixels);
  } else {
    snprintf(header_buf, sizeof(header_buf), "COL %u/64",
             (unsigned int)(game->data.current_color_idx + 1));
  }

  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, header_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, header_y, bounds.size.w, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // 5. Mini Horizontal Pixel Progress Bar (above 8x8 grid)
  int bar_top_y = grid_y - 4;
  int bar_top_h = 3;
  graphics_context_set_fill_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_fill_rect(ctx, GRect(grid_x, bar_top_y, grid_w, bar_top_h), 1, GCornersAll);

  uint32_t base_int = game_state_get_base_interval_ms();
  int filled_bar_w = (int)((game->data.progress_ms * (uint32_t)grid_w) / (base_int > 0 ? base_int : 1));
  if (filled_bar_w > grid_w) {
    filled_bar_w = grid_w;
  }
  if (filled_bar_w > 0) {
    graphics_context_set_fill_color(ctx, (game->boost_remaining_ms > 0) ? GColorOrange : GColorCobaltBlue);
    graphics_fill_rect(ctx, GRect(grid_x, bar_top_y, filled_bar_w, bar_top_h), 1, GCornersAll);
  }

  // 6. Vertical Progress Bar (to the left of 8x8 grid - 64 Slices)
  GRect bar_panel_rect = GRect(bar_x - 1, grid_y - 2, bar_w + 2, grid_h + 4);
  graphics_context_set_fill_color(ctx, panel_bg);
  graphics_fill_rect(ctx, bar_panel_rect, 1, GCornersAll);
  graphics_context_set_stroke_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_draw_rect(ctx, bar_panel_rect);

  for (int s = 0; s < 64; s++) {
    // Slices build bottom-up (0 at bottom to 63 at top)
    int y_bot = grid_y + grid_h - ((s * grid_h) / 64);
    int y_top = grid_y + grid_h - (((s + 1) * grid_h) / 64);
    int slice_h = y_bot - y_top;
    if (slice_h < 1) {
      slice_h = 1;
    }
    GRect slice_rect = GRect(bar_x, y_top, bar_w, slice_h);

    if (s < game->data.tier2_pixels) {
      GColor slice_col = game_state_get_pixel_color((uint8_t)s);
      graphics_context_set_fill_color(ctx, slice_col);
      graphics_fill_rect(ctx, slice_rect, 0, GCornerNone);
    } else {
      graphics_context_set_fill_color(ctx, empty_cell_bg);
      graphics_fill_rect(ctx, slice_rect, 0, GCornerNone);
    }
  }

  // 7. Grid Background Panel
  graphics_context_set_fill_color(ctx, panel_bg);
  graphics_fill_rect(ctx, GRect(grid_x - 2, grid_y - 2, grid_w + 4, grid_h + 4), 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_draw_rect(ctx, GRect(grid_x - 2, grid_y - 2, grid_w + 4, grid_h + 4));

  // 8. Render 8x8 Tier 1 Grid with Active Solid Color & Wipe/Rise Animation
  uint32_t cur_pix = game->data.current_pixels;
  GColor active_solid_color = game_state_get_active_solid_color();

  for (uint8_t row = 0; row < GRID_SIZE; row++) {
    for (uint8_t col = 0; col < GRID_SIZE; col++) {
      uint8_t seq_idx = game_state_get_order_index(row, col);
      int x = grid_x + (col * step);
      int y = grid_y + (row * step);
      GRect cell_rect = GRect(x, y, cell_size, cell_size);

      if (seq_idx < cur_pix) {
        // Fully filled pixel
        graphics_context_set_fill_color(ctx, active_solid_color);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);
      } else if (seq_idx == cur_pix) {
        // Active filling pixel: Wipe / Rise liquid animation from bottom
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);

        int fill_h = (int)((game->data.progress_ms * (uint32_t)cell_size) / (base_int > 0 ? base_int : 1));
        if (fill_h > cell_size) {
          fill_h = cell_size;
        }

        if (fill_h > 0) {
          GRect liquid_rect = GRect(x, y + cell_size - fill_h, cell_size, fill_h);
          graphics_context_set_fill_color(ctx, active_solid_color);
          graphics_fill_rect(ctx, liquid_rect, 0, GCornerNone);
        }

        // Active border
        graphics_context_set_stroke_color(ctx, active_cell_stroke);
        graphics_draw_rect(ctx, cell_rect);
      } else {
        // Empty pixel
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);
        graphics_context_set_stroke_color(ctx, empty_cell_stroke);
        graphics_draw_rect(ctx, cell_rect);
      }
    }
  }

  // 9. Footer: Stat "Pixels per second" & Navigation hints
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

  graphics_context_set_text_color(ctx, is_dark ? GColorLightGray : GColorDarkGray);
  graphics_draw_text(ctx, "^ Clock | = Menu | v Boost",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, footer_y + 13, bounds.size.w, 14),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // 10. Tier 2 Celebration Modal Popup (active when a solid color finishes)
  if (game_state_is_transition_active()) {
    int modal_w = bounds.size.w - 16;
    int modal_h = bounds.size.h - 26;
    int modal_x = 8;
    int modal_y = 13;

#if defined(PBL_ROUND)
    modal_w = 144;
    modal_h = 144;
    modal_x = (bounds.size.w - modal_w) / 2;
    modal_y = (bounds.size.h - modal_h) / 2;
#endif

    GRect modal_rect = GRect(modal_x, modal_y, modal_w, modal_h);

    // Modal background
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, modal_rect, 4, GCornersAll);
    GColor modal_border_col = game->gold_pixel_just_earned ? GColorYellow : GColorWhite;
    graphics_context_set_stroke_color(ctx, modal_border_col);
    graphics_draw_rect(ctx, modal_rect);

    // Modal title
    if (game->gold_pixel_just_earned) {
      graphics_context_set_text_color(ctx, GColorYellow);
      graphics_draw_text(ctx, "GOLD PIXEL! +1",
                         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(modal_x, modal_y + 4, modal_w, 20),
                         GTextOverflowModeWordWrap,
                         GTextAlignmentCenter,
                         NULL);
    } else {
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, "TIER 2 EXPANSION",
                         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(modal_x, modal_y + 4, modal_w, 20),
                         GTextOverflowModeWordWrap,
                         GTextAlignmentCenter,
                         NULL);
    }

    // Mini 8x8 Tier 2 Grid
    int mini_cell = 8;
    int mini_step = 9;
    int mini_grid_w = (GRID_SIZE * mini_step) - 1; // 71px
    int mini_grid_x = modal_x + (modal_w - mini_grid_w) / 2;
    int mini_grid_y = modal_y + 26;

    // The slot that was just completed:
    // If just earned gold pixel, all 64 were filled. Otherwise tier2_pixels was incremented (or 64th slot).
    uint8_t completed_slot = (game->data.tier2_pixels == 0) ? 63 : (game->data.tier2_pixels - 1);

    for (uint8_t r = 0; r < GRID_SIZE; r++) {
      for (uint8_t c = 0; c < GRID_SIZE; c++) {
        uint8_t slot_idx = (r * GRID_SIZE) + c;
        int sx = mini_grid_x + (c * mini_step);
        int sy = mini_grid_y + (r * mini_step);
        GRect slot_rect = GRect(sx, sy, mini_cell, mini_cell);

        bool is_filled = (slot_idx < game->data.tier2_pixels) ||
                         (game->gold_pixel_just_earned) ||
                         (slot_idx == completed_slot);

        if (is_filled) {
          GColor slot_col = game_state_get_pixel_color(slot_idx);
          graphics_context_set_fill_color(ctx, slot_col);
          graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);

          if (slot_idx == completed_slot) {
            graphics_context_set_stroke_color(ctx, GColorWhite);
            graphics_draw_rect(ctx, slot_rect);
          }
        } else {
          graphics_context_set_fill_color(ctx, GColorBlack);
          graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
          graphics_context_set_stroke_color(ctx, GColorDarkGray);
          graphics_draw_rect(ctx, slot_rect);
        }
      }
    }

    // Modal subtitle
    char sub_buf[40];
    if (game->gold_pixel_just_earned) {
      snprintf(sub_buf, sizeof(sub_buf), "Total Gold: %lu", (unsigned long)game->data.gold_pixels);
      graphics_context_set_text_color(ctx, GColorYellow);
    } else {
      snprintf(sub_buf, sizeof(sub_buf), "Color %u Filled!", (unsigned int)(completed_slot + 1));
      graphics_context_set_text_color(ctx, GColorWhite);
    }

    graphics_draw_text(ctx, sub_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(modal_x, mini_grid_y + mini_grid_w + 3, modal_w, 16),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);

    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "[Click to Continue]",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(modal_x, mini_grid_y + mini_grid_w + 19, modal_w, 16),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  }
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
