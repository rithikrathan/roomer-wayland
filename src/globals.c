#include "roomer.h"

Configuration g_default_configuration = {
  .window_title_roomermode = "roomer",
  .window_title_imagemode  = "roomer - image viewer",
  .window_width            = 1080,
  .window_height           = 720,
  .monitor_scaling         = 1.0F,
  .background_color        = (Color){ 0, 0, 0, 0 },
  .zoom_min                = 0.69F,
  .zoom_max                = 69.0F,
  .zoom_step               = 0.16F,
  .flashlight_radius_min   = 20.0F,
  .flashlight_radius_max   = 600.0F,
  .flashlight_radius_step  = 20.0F,
  .draw_color              = (Color){ 224, 40, 64, 255 },
  .draw_thickness          = 3.5F,
  .transparent_background  = false,
  .daemon_mode             = false,
  .quit_daemon             = false,
  .no_daemon               = false,
  .toggle_daemon           = false,
};

Args g_default_args = {
  .program_name      = NULL,
  .screenshot_folder = NULL,
};

State g_initial_state = {
  .pan                       = { 0, 0 },
  .target_pan                = { 0, 0 },
  .zoom                      = 1.0F,
  .target_zoom               = 1.0F,
  .flashlight_enabled        = false,
  .flashlight_rendering      = false,
  .flashlight_prev_enabled   = false,
  .flashlight_radius         = 100.0F,
  .flashlight_display_radius = 100.0F,
  .flashlight_darkness       = 0.069F,
  .target_flashlight_radius  = 200.0F,
  .is_drawing                = false,
  .toolbox_open              = false,
  .keymaps_open              = false,
  .should_quit               = false,
  .current_tool              = TOOL_PEN,
  .tool_pen_size             = 3.0F,
  .tool_eraser_size          = 18.0F,
  .tool_highlighter_size     = 20.0F,
  .color1                    = (Color){ 224, 40, 64, 255 },
  .color2                    = (Color){ 255, 255, 255, 255 },
  .active_swatch             = 0,
  .black_board_enabled       = false,
  .shape_stroke_style        = STYLE_SOLID,
  .shape_dash_len            = 12.0F,
  .shape_dash_gap            = 8.0F,
  .shape_filled              = false,
  .shape_fill_opacity        = 0.35F,
  .shape_border_color        = (Color){ 224, 40, 64, 255 },
  .fill_color                = (Color){ 224, 40, 64, 90 },
  .shape_thickness           = 3.0F,
  .badge_border_thickness    = 2.0F,
  .badge_size                = 22.0F,
  .badge_mode                = BADGE_MODE_NUMERIC,
  .badge_custom_text         = { 0 },
  .is_editing_badge_text     = false,
  .ngon_sides                = 6,
  .poly_pts                  = NULL,
  .poly_pts_count            = 0,
  .poly_pts_capacity         = 0,
  .poly_active               = false,
  .hide_overlay              = false,
  .text_bold                 = false,
  .text_italic               = false,
  .text_font_size            = 24.0F,
  .step_badge_counter        = 1,
  .table_rows                = 3,
  .table_cols                = 3,
  .is_editing_text           = false,
  .text_edit_world_pos       = (Vector2){ 0, 0 },
  .text_buffer               = { 0 },
  .text_cursor               = 0,
};

Configuration* g_configuration = NULL;
Args*          g_args          = NULL;
State*         g_state         = NULL;

__attribute__((__constructor__)) void initialize_globals(void) {
  g_configuration = malloc(sizeof(Configuration));
  assert(g_configuration);
  *g_configuration = g_default_configuration;

  g_args = malloc(sizeof(Args));
  assert(g_args);
  *g_args = g_default_args;

  g_state = malloc(sizeof(State));
  assert(g_state);
  *g_state = g_initial_state;
}

__attribute__((__destructor__)) void deinitialize_globals(void) {
  free(g_configuration);
  free(g_args);
  free(g_state);
  draw_free_all_memory();
}
