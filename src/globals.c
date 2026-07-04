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
  .current_tool              = TOOL_PEN,
  .tool_pen_size             = 3.5F,
  .tool_eraser_size          = 20.0F,
  .color1                    = (Color){ 224, 40, 64, 255 },
  .color2                    = (Color){ 255, 255, 255, 255 },
  .active_swatch             = 0,
  .black_board_enabled       = false,
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
  lines_clear();
  bb_lines_clear();
  hl_lines_clear();
}
