#include "ui_grid.h"
#include "game_state.h"

static Layer *s_canvas_layer = NULL;

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
  int header_y = 2;
  int grid_y = 21;
  int footer_y = grid_y + grid_h + 3;

#if defined(PBL_ROUND)
  header_y = 8;
  grid_y = 26;
  footer_y = grid_y + grid_h + 2;
#endif

  // 3. Header
  char header_buf[32];
  if (game->data.gold_pixels > 0) {
    snprintf(header_buf, sizeof(header_buf), "TIER 2 | GP %lu", (unsigned long)game->data.gold_pixels);
  } else {
    snprintf(header_buf, sizeof(header_buf), "TIER 2 (64 COLORS)");
  }
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, header_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, header_y, bounds.size.w, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // 4. Background panel for grid
  GRect panel_rect = GRect(grid_x - 2, grid_y - 2, grid_w + 4, grid_h + 4);
  graphics_context_set_fill_color(ctx, is_dark ? GColorBlack : GColorWhite);
  graphics_fill_rect(ctx, panel_rect, 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, empty_cell_stroke);
  graphics_draw_rect(ctx, panel_rect);

  // 5. Draw all 64 slots of Tier 2
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
        // Active color currently building in Tier 1
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);

        int fill_h = (int)((game->data.current_pixels * (uint32_t)cell_size) / TOTAL_PIXELS);
        if (fill_h > cell_size) fill_h = cell_size;
        if (fill_h > 0) {
          GRect cur_fill_rect = GRect(x, y + cell_size - fill_h, cell_size, fill_h);
          graphics_context_set_fill_color(ctx, game_state_get_active_solid_color());
          graphics_fill_rect(ctx, cur_fill_rect, 0, GCornerNone);
        }

        graphics_context_set_stroke_color(ctx, is_dark ? GColorWhite : GColorBlack);
        graphics_draw_rect(ctx, slot_rect);
      } else {
        // Unfilled socket
        graphics_context_set_fill_color(ctx, empty_cell_bg);
        graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
        graphics_context_set_stroke_color(ctx, empty_cell_stroke);
        graphics_draw_rect(ctx, slot_rect);
      }
    }
  }

  // 6. Footer
  char stat_buf[32];
  snprintf(stat_buf, sizeof(stat_buf), "%u/64 Colors Completed", (unsigned int)game->data.tier2_pixels);
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, stat_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, footer_y, bounds.size.w, 14),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  graphics_context_set_text_color(ctx, is_dark ? GColorLightGray : GColorDarkGray);
  graphics_draw_text(ctx, "v Zoom In (Tier 1) | = Menu",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, footer_y + 13, bounds.size.w, 14),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void ui_grid_update_proc(Layer *layer, GContext *ctx) {
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

  // 3. Liquid ink filling up the background like liquid during active boost
  if (game->current_boost_milli > 0) {
    GColor ink_color = game_state_get_ink_color();
    static uint16_t s_ink_wave_phase = 0;
    s_ink_wave_phase += 1400; // gentle undulating wave motion

    uint32_t max_limit_milli = game_state_get_boost_max_limit_percent() * 1000;
    if (max_limit_milli == 0) max_limit_milli = 100000;

    int liquid_h = (int)(((int32_t)bounds.size.h * (int32_t)game->current_boost_milli) / (int32_t)max_limit_milli);
    if (liquid_h > bounds.size.h) {
      liquid_h = bounds.size.h;
    }
    if (liquid_h < 4) {
      liquid_h = 4;
    }

    int surface_y = bounds.size.h - liquid_h;
    int solid_fill_y = surface_y + 4;

    graphics_context_set_stroke_color(ctx, ink_color);
    for (int x = 0; x < bounds.size.w; x++) {
      int32_t angle = (x * 1100) + s_ink_wave_phase;
      int wave_offset = (sin_lookup(angle) * 3) / TRIG_MAX_RATIO;
      int y_top = surface_y + wave_offset;
      if (y_top < 0) {
        y_top = 0;
      }
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

  // 4. Header: Active Color & Gold Pixels (or Completion notification)
  bool in_preview_phase = (game_state_is_transition_active() && !game_state_is_transition_anim_running());
  char header_buf[32];
  if (in_preview_phase) {
    snprintf(header_buf, sizeof(header_buf), "COLOR #%u COMPLETE!",
             (unsigned int)(game_state_get_transition_completed_color() + 1));
  } else if (game->data.gold_pixels > 0) {
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
    graphics_context_set_fill_color(ctx, (game->current_boost_milli > 0) ? GColorOrange : GColorCobaltBlue);
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
  uint32_t anim_ms = game_state_get_transition_anim_ms();
  bool in_fuse_phase = (game_state_is_transition_active() && game_state_is_transition_anim_running() && anim_ms < 400);

  if (in_fuse_phase) {
    // Stage 1: The entire grid fuses into one continuous solid block!
    uint8_t fuse_color_idx = game_state_get_transition_completed_color();
    GColor fuse_col = game_state_get_pixel_color(fuse_color_idx);

    graphics_context_set_fill_color(ctx, fuse_col);
    graphics_fill_rect(ctx, GRect(grid_x, grid_y, grid_w, grid_h), 2, GCornersAll);

    // Glowing white border around fused block
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_rect(ctx, GRect(grid_x - 1, grid_y - 1, grid_w + 2, grid_h + 2));
    graphics_draw_rect(ctx, GRect(grid_x, grid_y, grid_w, grid_h));
  } else if (in_preview_phase) {
    // Stage 0: Full color grid completion preview waiting for button press
    uint8_t fuse_color_idx = game_state_get_transition_completed_color();
    GColor fuse_col = game_state_get_pixel_color(fuse_color_idx);

    for (uint8_t row = 0; row < GRID_SIZE; row++) {
      for (uint8_t col = 0; col < GRID_SIZE; col++) {
        int x = grid_x + (col * step);
        int y = grid_y + (row * step);
        GRect cell_rect = GRect(x, y, cell_size, cell_size);
        graphics_context_set_fill_color(ctx, fuse_col);
        graphics_fill_rect(ctx, cell_rect, 0, GCornerNone);
      }
    }
    // Highlighting border around the full grid
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_rect(ctx, GRect(grid_x - 1, grid_y - 1, grid_w + 2, grid_h + 2));
  } else {
    // Normal Tier 1 cell-by-cell liquid wipe rendering
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
  }

  // 9. Footer: Stat "Pixels per second" & Navigation hints (or Preview prompt)
  if (in_preview_phase) {
    graphics_context_set_text_color(ctx, fg_color);
    graphics_draw_text(ctx, "[Press Any Button]",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(0, footer_y, bounds.size.w, 14),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);

    graphics_context_set_text_color(ctx, is_dark ? GColorLightGray : GColorDarkGray);
    graphics_draw_text(ctx, "To Shrink into Tier 2",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, footer_y + 13, bounds.size.w, 14),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  } else {
    uint32_t pps_whole = 0, pps_frac = 0;
    game_state_get_pps(&pps_whole, &pps_frac);

    char pps_buf[48];
    if (game->current_boost_milli > 0) {
      uint32_t boost_whole = 0, boost_tenths = 0;
      game_state_get_current_boost_parts(&boost_whole, &boost_tenths);
      snprintf(pps_buf, sizeof(pps_buf), "%lu.%02lu px/s [+%lu.%1lu%%]",
               (unsigned long)pps_whole, (unsigned long)pps_frac,
               (unsigned long)boost_whole, (unsigned long)boost_tenths);
      GRect badge_rect = GRect((bounds.size.w - 140) / 2, footer_y, 140, 14);
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
    graphics_draw_text(ctx, "^ Tier 2 | = Menu | v Boost",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, footer_y + 13, bounds.size.w, 14),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  }

  // 10. Tier 2 Celebration Modal Popup (active when anim_ms >= 400ms during transition anim)
  if (game_state_is_transition_active() && game_state_is_transition_anim_running() && anim_ms >= 400) {
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
    int mini_cell = (bounds.size.w >= 200) ? 10 : 8;
    int mini_step = mini_cell + 1;
    int mini_grid_w = (GRID_SIZE * mini_step) - 1;
    int mini_grid_x = modal_x + (modal_w - mini_grid_w) / 2;
    int mini_grid_y = modal_y + ((bounds.size.w >= 200) ? 36 : 26);

    uint8_t completed_color = game_state_get_transition_completed_color();
    GColor completed_col = game_state_get_pixel_color(completed_color);

    int target_r = completed_color / GRID_SIZE;
    int target_c = completed_color % GRID_SIZE;
    int target_x = mini_grid_x + (target_c * mini_step);
    int target_y = mini_grid_y + (target_r * mini_step);
    int target_w = mini_cell;
    int target_h = mini_cell;

    for (uint8_t r = 0; r < GRID_SIZE; r++) {
      for (uint8_t c = 0; c < GRID_SIZE; c++) {
        uint8_t slot_idx = (r * GRID_SIZE) + c;
        int sx = mini_grid_x + (c * mini_step);
        int sy = mini_grid_y + (r * mini_step);
        GRect slot_rect = GRect(sx, sy, mini_cell, mini_cell);

        if (slot_idx < completed_color) {
          // Already completed earlier
          GColor slot_col = game_state_get_pixel_color(slot_idx);
          graphics_context_set_fill_color(ctx, slot_col);
          graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
        } else if (slot_idx == completed_color) {
          // The newly completed slot: only show as filled once block lands (anim_ms >= 1200)
          if (anim_ms >= 1200) {
            graphics_context_set_fill_color(ctx, completed_col);
            graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);

            if (anim_ms < 1500 && ((anim_ms / 60) % 2 == 0)) {
              // Impact flash pulse
              graphics_context_set_stroke_color(ctx, GColorWhite);
              graphics_draw_rect(ctx, GRect(sx - 1, sy - 1, mini_cell + 2, mini_cell + 2));
            } else {
              graphics_context_set_stroke_color(ctx, GColorWhite);
              graphics_draw_rect(ctx, slot_rect);
            }
          } else {
            // Unfilled socket waiting for shrinking block to arrive
            graphics_context_set_fill_color(ctx, GColorBlack);
            graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
            graphics_context_set_stroke_color(ctx, GColorWhite);
            graphics_draw_rect(ctx, slot_rect);
          }
        } else {
          // Future slots
          graphics_context_set_fill_color(ctx, GColorBlack);
          graphics_fill_rect(ctx, slot_rect, 0, GCornerNone);
          graphics_context_set_stroke_color(ctx, GColorDarkGray);
          graphics_draw_rect(ctx, slot_rect);
        }
      }
    }

    // Shrinking and flying block animation (400ms to 1200ms)
    if (anim_ms < 1200) {
      int t_ms = (int)(anim_ms - 400);
      int duration = 800;
      // Quadratic ease out: (t/d) * (2 - t/d) * 1000
      int ease_num = (t_ms * 2000 - ((t_ms * t_ms) / duration) * 1000) / duration;
      if (ease_num < 0) ease_num = 0;
      if (ease_num > 1000) ease_num = 1000;

      int cur_x = grid_x + ((target_x - grid_x) * ease_num) / 1000;
      int cur_y = grid_y + ((target_y - grid_y) * ease_num) / 1000;
      int cur_w = grid_w + ((target_w - grid_w) * ease_num) / 1000;
      int cur_h = grid_h + ((target_h - grid_h) * ease_num) / 1000;

      GRect fly_rect = GRect(cur_x, cur_y, cur_w, cur_h);
      graphics_context_set_fill_color(ctx, completed_col);
      graphics_fill_rect(ctx, fly_rect, (cur_w > 16 ? 2 : 0), GCornersAll);
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_rect(ctx, fly_rect);
    }

    // Modal subtitle
    char sub_buf[40];
    if (game->gold_pixel_just_earned) {
      snprintf(sub_buf, sizeof(sub_buf), "Total Gold: %lu", (unsigned long)game->data.gold_pixels);
      graphics_context_set_text_color(ctx, GColorYellow);
    } else {
      snprintf(sub_buf, sizeof(sub_buf), "Color #%u Complete!", (unsigned int)(completed_color + 1));
      graphics_context_set_text_color(ctx, GColorWhite);
    }

    graphics_draw_text(ctx, sub_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(modal_x, mini_grid_y + mini_grid_w + 3, modal_w, 16),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);

    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, game->gold_pixel_just_earned ? "Master Grid Completed!" : "Fusing into Tier 2...",
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
