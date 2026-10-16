#include "ui_grid.h"
#include "game_state.h"

static Layer *s_canvas_layer = NULL;

typedef enum {
  ZOOM_STATE_NONE = 0,
  ZOOM_STATE_OUT = 1,
  ZOOM_STATE_IN = 2,
} ZoomState;

static ZoomState s_zoom_state = ZOOM_STATE_NONE;
static uint32_t s_zoom_anim_ms = 0;
#define ZOOM_DURATION_MS 300

void ui_grid_start_zoom_out(void) {
  if (s_zoom_state != ZOOM_STATE_NONE) return;
  s_zoom_state = ZOOM_STATE_OUT;
  s_zoom_anim_ms = 0;
  ui_grid_mark_dirty();
}

void ui_grid_start_zoom_in(void) {
  if (s_zoom_state != ZOOM_STATE_NONE) return;
  s_zoom_state = ZOOM_STATE_IN;
  s_zoom_anim_ms = 0;
  ui_grid_mark_dirty();
}

bool ui_grid_is_zooming(void) {
  return (s_zoom_state != ZOOM_STATE_NONE);
}

void ui_grid_tick_zoom(uint32_t dt_ms) {
  if (s_zoom_state == ZOOM_STATE_NONE) return;
  s_zoom_anim_ms += dt_ms;
  if (s_zoom_anim_ms >= ZOOM_DURATION_MS) {
    if (s_zoom_state == ZOOM_STATE_OUT) {
      game_state_set_view_mode(VIEW_MODE_TIER2);
    } else if (s_zoom_state == ZOOM_STATE_IN) {
      game_state_set_view_mode(VIEW_MODE_TIER1);
    }
    s_zoom_state = ZOOM_STATE_NONE;
    s_zoom_anim_ms = 0;
  }
  ui_grid_mark_dirty();
}

static void prv_draw_tier2_view(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  // 1. Background
  GColor bg_color = game_state_get_bg_color();
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  bool is_dark = game_state_is_color_dark(bg_color);
  GColor fg_color = is_dark ? GColorWhite : GColorBlack;
  GColor empty_cell_bg = is_dark ? GColorBlack : GColorWhite;
  GColor empty_cell_stroke = is_dark ? GColorDarkGray : GColorLightGray;

  // 2. Sizing & Layout for 8x8 Tier 2 Grid
  int cell_size = 13;
  int gap = 1;
  if (bounds.size.w >= 200) {
    cell_size = 17;
    gap = 2;
  }
  int step = cell_size + gap;
  int grid_w = (GRID_SIZE * step) - gap;
  int grid_h = grid_w;
  int grid_x = (bounds.size.w - grid_w) / 2;
  int header_y = 1;
  int grid_y = 20;
  int footer_y = grid_y + grid_h + 1;

  if (bounds.size.w >= 200) {
    header_y = 4;
    grid_y = 26;
    footer_y = 171;
  }

  // 3. Header
  char header_buf[32];
  uint32_t crw = game_state_get_global_crowns();
  if (crw > 0) {
    snprintf(header_buf, sizeof(header_buf), "COLOURS | CRW %lu", (unsigned long)crw);
  } else {
    snprintf(header_buf, sizeof(header_buf), "COLOURS (64)");
  }
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, header_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, header_y, bounds.size.w, 18),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // 4. Background panel for grid
  GRect panel_rect = GRect(grid_x - 2, grid_y - 2, grid_w + 4, grid_h + 4);
  graphics_context_set_fill_color(ctx, is_dark ? GColorBlack : GColorWhite);
  graphics_fill_rect(ctx, panel_rect, 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, empty_cell_stroke);
  graphics_draw_rect(ctx, panel_rect);

  // 5. Draw all 64 slots of Tier 2 in standard row-major order
  for (uint8_t r = 0; r < GRID_SIZE; r++) {
    for (uint8_t c = 0; c < GRID_SIZE; c++) {
      uint8_t slot_idx = (r * GRID_SIZE) + c;
      int x = grid_x + (c * step);
      int y = grid_y + (r * step);
      GRect slot_rect = GRect(x, y, cell_size, cell_size);

      if (slot_idx < game->data.tier2_pixels) {
        // Fully completed color slot
        GColor slot_col = game_state_get_pixel_color(slot_idx);
        graphics_context_set_fill_color(ctx, slot_col);
        graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
      } else if (slot_idx == game->data.tier2_pixels) {
        // Active color currently building in Tier 1:
        // Animate each pixel filling up inside this cell according to selected fill pattern!
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);

        GColor active_col = game_state_get_active_solid_color();
        graphics_context_set_fill_color(ctx, active_col);

        uint32_t cur_pix = game->data.current_pixels;
        for (uint8_t mr = 0; mr < GRID_SIZE; mr++) {
          for (uint8_t mc = 0; mc < GRID_SIZE; mc++) {
            uint8_t p_order = game_state_get_order_index(mr, mc);
            if (p_order < cur_pix) {
              int px0 = x + (mc * cell_size) / GRID_SIZE;
              int px1 = x + ((mc + 1) * cell_size) / GRID_SIZE;
              int py0 = y + (mr * cell_size) / GRID_SIZE;
              int py1 = y + ((mr + 1) * cell_size) / GRID_SIZE;
              graphics_fill_rect(ctx, GRect(px0, py0, px1 - px0, py1 - py0), 0, GCornerNone);
            }
          }
        }

        // Notifyer: Colored border highlighting the next unlock in the Colours grid
        graphics_context_set_stroke_color(ctx, active_col);
        graphics_draw_rect(ctx, slot_rect);
        GRect outer_notify_rect = GRect(slot_rect.origin.x - 1, slot_rect.origin.y - 1, slot_rect.size.w + 2, slot_rect.size.h + 2);
        graphics_context_set_stroke_color(ctx, GColorYellow);
        graphics_draw_rect(ctx, outer_notify_rect);
      } else {
        // Unfilled socket
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
        graphics_context_set_stroke_color(ctx, empty_cell_stroke);
        graphics_draw_rect(ctx, slot_rect);
      }
    }
  }

  // 6. Footer: Completed counter
  char stat_buf[32];
  snprintf(stat_buf, sizeof(stat_buf), "%u/64 Colours Completed", (unsigned int)game->data.tier2_pixels);
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, stat_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, footer_y, bounds.size.w, 14),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // High-contrast 2-line button directions card
  int card_w = bounds.size.w - 12;
  int card_h = 24;
  int card_x = (bounds.size.w - card_w) / 2;
  int card_y = 143;
  int line1_y = card_y + 1;
  int line2_y = card_y + 12;

  if (bounds.size.w >= 200) {
    card_h = 36;
    card_y = 187;
    line1_y = card_y + 3;
    line2_y = card_y + 17;
  }

  GRect card_rect = GRect(card_x, card_y, card_w, card_h);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, card_rect, 3, GCornersAll);
  graphics_context_set_stroke_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_draw_rect(ctx, card_rect);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "v Pixels",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(card_x, line1_y, card_w, 13),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
  graphics_draw_text(ctx, "> Menu | ^ Hold for Clock",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(card_x, line2_y, card_w, 13),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void prv_draw_zoom_transition(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  // Background
  GColor bg_color = game_state_get_bg_color();
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  bool is_dark = game_state_is_color_dark(bg_color);
  GColor empty_cell_bg = is_dark ? GColorBlack : GColorWhite;
  GColor empty_cell_stroke = is_dark ? GColorDarkGray : GColorLightGray;

  // Tier 2 layout metrics
  int t2_cell_size = (bounds.size.w >= 200) ? 17 : 13;
  int t2_gap = (bounds.size.w >= 200) ? 2 : 1;
  int t2_step = t2_cell_size + t2_gap;
  int t2_grid_w = (GRID_SIZE * t2_step) - t2_gap;
  int t2_grid_x = (bounds.size.w - t2_grid_w) / 2;
  int t2_grid_y = (bounds.size.w >= 200) ? 26 : 20;

  // Tier 1 layout metrics
  int t1_cell_size = (bounds.size.w >= 200) ? 16 : 13;
  int t1_gap = (bounds.size.w >= 200) ? 2 : 1;
  int t1_step = t1_cell_size + t1_gap;
  int t1_grid_w = (GRID_SIZE * t1_step) - t1_gap;
  int t1_grid_h = t1_grid_w;
  int bar_w = (bounds.size.w >= 200) ? 8 : 6;
  int bar_gap = (bounds.size.w >= 200) ? 6 : 5;
  int t1_content_w = bar_w + bar_gap + t1_grid_w;
  int t1_bar_x = (bounds.size.w - t1_content_w) / 2;
  int t1_grid_x = t1_bar_x + bar_w + bar_gap;
  int t1_grid_y = (bounds.size.w >= 200) ? 26 : 20;

  // Target slot for active color in Tier 2
  uint8_t active_slot = game->data.tier2_pixels;
  int target_r = active_slot / GRID_SIZE;
  int target_c = active_slot % GRID_SIZE;
  int target_slot_x = t2_grid_x + (target_c * t2_step);
  int target_slot_y = t2_grid_y + (target_r * t2_step);

  // Interpolation fraction (0 to 1000)
  int32_t t_ms = (int32_t)s_zoom_anim_ms;
  if (t_ms > ZOOM_DURATION_MS) t_ms = ZOOM_DURATION_MS;
  int32_t frac = (t_ms * 1000) / ZOOM_DURATION_MS;

  int start_x, start_y, start_w, start_h;
  int end_x, end_y, end_w, end_h;

  if (s_zoom_state == ZOOM_STATE_OUT) {
    // From Tier 1 (full) to Tier 2 active slot
    start_x = t1_grid_x; start_y = t1_grid_y; start_w = t1_grid_w; start_h = t1_grid_h;
    end_x = target_slot_x; end_y = target_slot_y; end_w = t2_cell_size; end_h = t2_cell_size;
  } else {
    // From Tier 2 active slot to Tier 1 full
    start_x = target_slot_x; start_y = target_slot_y; start_w = t2_cell_size; start_h = t2_cell_size;
    end_x = t1_grid_x; end_y = t1_grid_y; end_w = t1_grid_w; end_h = t1_grid_h;
  }

  int cur_x = start_x + (int)(((int32_t)(end_x - start_x) * frac) / 1000);
  int cur_y = start_y + (int)(((int32_t)(end_y - start_y) * frac) / 1000);
  int cur_w = start_w + (int)(((int32_t)(end_w - start_w) * frac) / 1000);
  int cur_h = start_h + (int)(((int32_t)(end_h - start_h) * frac) / 1000);

  // Draw other Tier 2 cells in background
  for (uint8_t r = 0; r < GRID_SIZE; r++) {
    for (uint8_t c = 0; c < GRID_SIZE; c++) {
      uint8_t slot_idx = (r * GRID_SIZE) + c;
      if (slot_idx == active_slot) continue;
      int x = t2_grid_x + (c * t2_step);
      int y = t2_grid_y + (r * t2_step);
      GRect slot_rect = GRect(x, y, t2_cell_size, t2_cell_size);

      if (slot_idx < game->data.tier2_pixels) {
        GColor slot_col = game_state_get_pixel_color(slot_idx);
        graphics_context_set_fill_color(ctx, slot_col);
        graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
      } else {
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
        graphics_context_set_stroke_color(ctx, empty_cell_stroke);
        graphics_draw_rect(ctx, slot_rect);
      }
    }
  }

  // Draw the active zooming grid
  GRect zoom_rect = GRect(cur_x, cur_y, cur_w, cur_h);
  graphics_context_set_fill_color(ctx, game_state_get_active_solid_color());
  graphics_fill_rect(ctx, zoom_rect, (cur_w > 20 ? 2 : 0), GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, zoom_rect);
}

static void ui_grid_update_proc(Layer *layer, GContext *ctx) {
  // If actively zooming, render the smooth zoom transition!
  if (s_zoom_state != ZOOM_STATE_NONE) {
    prv_draw_zoom_transition(layer, ctx);
    return;
  }

  // If in Tier 2 zoom view, render Tier 2 grid
  if (game_state_get_view_mode() == VIEW_MODE_TIER2) {
    prv_draw_tier2_view(layer, ctx);
    return;
  }

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
  int step = cell_size + gap;
  int grid_w = (GRID_SIZE * step) - gap;
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

  int header_y = 0;
  int grid_y = 20;
  int footer_y = grid_y + grid_h + 1;

  if (bounds.size.w >= 200) {
    header_y = 4;
    grid_y = 26;
    footer_y = 171;
  }

  // 3. Liquid ink filling up the background like liquid during active boost
  if (game->current_boost_milli > 0) {
    GColor ink_color = game_state_get_ink_color();
    static uint16_t s_ink_wave_phase = 0;
    s_ink_wave_phase += 1400;

    uint32_t max_limit_milli = game_state_get_boost_max_limit_percent() * 1000;
    if (max_limit_milli == 0) max_limit_milli = 100000;

    int liquid_h = (int)(((int32_t)bounds.size.h * (int32_t)game->current_boost_milli) / (int32_t)max_limit_milli);
    if (liquid_h > bounds.size.h) liquid_h = bounds.size.h;
    if (liquid_h < 4) liquid_h = 4;

    int surface_y = bounds.size.h - liquid_h;
    int solid_fill_y = surface_y + 4;

    graphics_context_set_stroke_color(ctx, ink_color);
    for (int x = 0; x < bounds.size.w; x++) {
      int32_t angle = (x * 1100) + s_ink_wave_phase;
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

  // 4. Header: Active Color & Crowns
  char header_buf[32];
  uint32_t crw = game_state_get_global_crowns();
  if (crw > 0) {
    snprintf(header_buf, sizeof(header_buf), "COL %u/64 | CRW %lu",
             (unsigned int)(game->data.current_color_idx + 1),
             (unsigned long)crw);
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

  // 5. Vertical Progress Bar (to the left of 8x8 grid - 64 Slices)
  GRect bar_panel_rect = GRect(bar_x - 1, grid_y - 2, bar_w + 2, grid_h + 4);
  graphics_context_set_fill_color(ctx, panel_bg);
  graphics_fill_rect(ctx, bar_panel_rect, 1, GCornersAll);
  graphics_context_set_stroke_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_draw_rect(ctx, bar_panel_rect);

  for (int s = 0; s < 64; s++) {
    int y_bot = grid_y + grid_h - ((s * grid_h) / 64);
    int y_top = grid_y + grid_h - (((s + 1) * grid_h) / 64);
    int slice_h = y_bot - y_top;
    if (slice_h < 1) slice_h = 1;
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

  // 8. Render 8x8 Tier 1 Grid
  uint32_t cur_pix = game->data.current_pixels;
  GColor active_solid_color = game_state_get_active_solid_color();
  uint32_t base_int = game_state_get_base_interval_ms();

  for (uint8_t row = 0; row < GRID_SIZE; row++) {
    for (uint8_t col = 0; col < GRID_SIZE; col++) {
      uint8_t seq_idx = game_state_get_order_index(row, col);
      int x = grid_x + (col * step);
      int y = grid_y + (row * step);
      GRect cell_rect = GRect(x, y, cell_size, cell_size);

      if (seq_idx < cur_pix) {
        graphics_context_set_fill_color(ctx, active_solid_color);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);
      } else if (seq_idx == cur_pix) {
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);

        int fill_h = (int)((game->data.progress_ms * (uint32_t)cell_size) / (base_int > 0 ? base_int : 1));
        if (fill_h > cell_size) fill_h = cell_size;

        if (fill_h > 0) {
          GRect liquid_rect = GRect(x, y + cell_size - fill_h, cell_size, fill_h);
          graphics_context_set_fill_color(ctx, active_solid_color);
          graphics_fill_rect(ctx, liquid_rect, 0, GCornerNone);
        }

        graphics_context_set_stroke_color(ctx, active_cell_stroke);
        graphics_draw_rect(ctx, cell_rect);
      } else {
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);
        graphics_context_set_stroke_color(ctx, empty_cell_stroke);
        graphics_draw_rect(ctx, cell_rect);
      }
    }
  }

  // 9. Footer: PPS & High-contrast 2-line button card
  uint32_t pps_whole = 0, pps_frac = 0;
  game_state_get_pps(&pps_whole, &pps_frac);

  char pps_buf[48];
  if (game->current_boost_milli > 0) {
    uint32_t boost_whole = 0, boost_tenths = 0;
    game_state_get_current_boost_parts(&boost_whole, &boost_tenths);
    snprintf(pps_buf, sizeof(pps_buf), "%lu.%02lu px/s [+%lu.%1lu%%]",
             (unsigned long)pps_whole, (unsigned long)pps_frac,
             (unsigned long)boost_whole, (unsigned long)boost_tenths);
    GRect badge_rect = GRect((bounds.size.w - 140) / 2, footer_y, 140, 13);
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
                       GRect(0, footer_y, bounds.size.w, 13),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  }

  // High-contrast 2-line button directions card (contrasts with background & boost ink)
  int card_w = bounds.size.w - 12;
  int card_h = 24;
  int card_x = (bounds.size.w - card_w) / 2;
  int card_y = 143;
  int line1_y = card_y + 1;
  int line2_y = card_y + 12;

  if (bounds.size.w >= 200) {
    card_h = 36;
    card_y = 187;
    line1_y = card_y + 3;
    line2_y = card_y + 17;
  }

  GRect card_rect = GRect(card_x, card_y, card_w, card_h);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, card_rect, 3, GCornersAll);
  graphics_context_set_stroke_color(ctx, is_dark ? GColorDarkGray : GColorLightGray);
  graphics_draw_rect(ctx, card_rect);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "^ Colours (Hold: Clock)",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(card_x, line1_y, card_w, 13),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
  graphics_draw_text(ctx, "> Menu | v Boost (Hold)",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(card_x, line2_y, card_w, 13),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // 10. Meld & Grow-to-fill-screen Animation
  if (game_state_is_meld_anim_running()) {
    uint32_t anim_ms = game_state_get_meld_anim_ms();
    uint8_t completed_color = game_state_get_meld_anim_color();
    GColor completed_col = game_state_get_pixel_color(completed_color);

    // Expand smoothly from the 8x8 grid rect to fill the entire screen
    int expand_duration = 280;
    int cur_ms = (anim_ms > (uint32_t)expand_duration) ? expand_duration : (int)anim_ms;
    int progress = (cur_ms * 1000) / expand_duration; // 0 to 1000
    int inv = 1000 - progress;
    int ease = 1000 - (inv * inv) / 1000; // ease-out quad 0 to 1000

    int target_x = -4;
    int target_y = -4;
    int target_w = bounds.size.w + 8;
    int target_h = bounds.size.h + 8;

    int cur_x = grid_x + ((target_x - grid_x) * ease) / 1000;
    int cur_y = grid_y + ((target_y - grid_y) * ease) / 1000;
    int cur_w = grid_w + ((target_w - grid_w) * ease) / 1000;
    int cur_h = grid_h + ((target_h - grid_h) * ease) / 1000;
    GRect meld_rect = GRect(cur_x, cur_y, cur_w, cur_h);

    GColor border_col = game->crown_just_earned ? GColorYellow : GColorWhite;

    if (anim_ms < 240) {
      // Solid melded block growing outward across the screen
      graphics_context_set_fill_color(ctx, completed_col);
      graphics_fill_rect(ctx, meld_rect, (cur_w < bounds.size.w ? 2 : 0), GCornersAll);

      graphics_context_set_stroke_color(ctx, border_col);
      graphics_draw_rect(ctx, meld_rect);
      if (cur_w < bounds.size.w) {
        graphics_draw_rect(ctx, GRect(cur_x - 1, cur_y - 1, cur_w + 2, cur_h + 2));
      }
    } else if (anim_ms < 360) {
      // 50% scanline dither fade as it fills the screen
      graphics_context_set_stroke_color(ctx, completed_col);
      int y_start = (meld_rect.origin.y < 0) ? 0 : meld_rect.origin.y;
      int y_end = (meld_rect.origin.y + meld_rect.size.h > bounds.size.h) ? bounds.size.h : (meld_rect.origin.y + meld_rect.size.h);
      int x_start = (meld_rect.origin.x < 0) ? 0 : meld_rect.origin.x;
      int x_end = (meld_rect.origin.x + meld_rect.size.w > bounds.size.w) ? bounds.size.w : (meld_rect.origin.x + meld_rect.size.w);
      for (int y = y_start; y < y_end; y += 2) {
        graphics_draw_line(ctx, GPoint(x_start, y), GPoint(x_end, y));
      }

      graphics_context_set_stroke_color(ctx, border_col);
      graphics_draw_rect(ctx, meld_rect);
    } else {
      // 25% scanline fade quickly dissolving away
      graphics_context_set_stroke_color(ctx, completed_col);
      int y_start = (meld_rect.origin.y < 0) ? 0 : meld_rect.origin.y;
      int y_end = (meld_rect.origin.y + meld_rect.size.h > bounds.size.h) ? bounds.size.h : (meld_rect.origin.y + meld_rect.size.h);
      int x_start = (meld_rect.origin.x < 0) ? 0 : meld_rect.origin.x;
      int x_end = (meld_rect.origin.x + meld_rect.size.w > bounds.size.w) ? bounds.size.w : (meld_rect.origin.x + meld_rect.size.w);
      for (int y = y_start; y < y_end; y += 4) {
        graphics_draw_line(ctx, GPoint(x_start, y), GPoint(x_end, y));
      }

      graphics_context_set_stroke_color(ctx, border_col);
      graphics_draw_rect(ctx, meld_rect);
    }
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
