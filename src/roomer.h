#pragma once

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <memory.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "tablet.h"

typedef enum {
  TOOL_PEN,
  TOOL_ERASER,
  TOOL_HIGHLIGHTER,
  TOOL_LINE,
  TOOL_RECTANGLE,
  TOOL_CIRCLE,
  TOOL_ARROW,
} ToolType;

typedef enum {
  LAYER_IMAGE = 0,
  LAYER_BLACKBOARD = 1,
  LAYER_COUNT = 2,
} DrawLayer;

typedef enum {
  SHAPE_FREEHAND,
  SHAPE_LINE,
  SHAPE_RECTANGLE,
  SHAPE_CIRCLE,
  SHAPE_ARROW,
} ShapeType;

typedef struct {
  ShapeType type;
  ToolType  tool;
  Vector2*  points;
  int       points_count;
  int       points_capacity;
  float     thickness;
  Color     color;
} Stroke;

typedef struct {
  Vector2 pan;
  Vector2 target_pan;
  float   zoom;
  float   target_zoom;
  bool    flashlight_enabled;
  bool    flashlight_rendering;
  bool    flashlight_prev_enabled;
  float   flashlight_radius;
  float   flashlight_display_radius;
  float   flashlight_darkness;
  float   target_flashlight_radius;
  bool    is_drawing;
  bool    toolbox_open;
  bool    keymaps_open;
  ToolType current_tool;
  float   tool_pen_size;
  float   tool_eraser_size;
  float   tool_highlighter_size;
  Color   color1;
  Color   color2;
  int     active_swatch;
  bool    black_board_enabled;
  int     image_w;
  int     image_h;
} State;

typedef struct {
  char* program_name;
  char* screenshot_folder;
} Args;

typedef struct {
  char* window_title_roomermode;
  char* window_title_imagemode;
  int   window_width;
  int   window_height;
  float monitor_scaling;
  Color background_color;
  float zoom_min;
  float zoom_max;
  float zoom_step;
  float flashlight_radius_min;
  float flashlight_radius_max;
  float flashlight_radius_step;
  Color draw_color;
  float draw_thickness;
  bool  transparent_background;
  bool  daemon_mode;
  bool  quit_daemon;
  bool  no_daemon;
} Configuration;

extern Configuration g_default_configuration;
extern State         g_initial_state;

extern Configuration* g_configuration;
extern Args*          g_args;
extern State*         g_state;

void process_commandline_arguments(int argc, char** argv);

Image load_image_from_stdin(bool* out_was_file);
const char* detect_image_extension(const unsigned char* data, size_t length);

void handle_inputs(void);
void handle_draw(void);
Vector2 get_cursor_screen_pos(void);
extern const char* g_flashlight_frag_shader_source;
void app_render_frame(Texture2D img_texture, Shader flashlight_shader, int loc_center, int loc_radius, int loc_darkness, int loc_texture);

// Unified Drawing System
void draw_layer_normal(DrawLayer layer);
void draw_render_highlighter(DrawLayer layer, bool view_changed);
void draw_composite_highlighter(void);
void draw_dot_grid(int sw, int sh, Vector2 pan, float zoom);
bool draw_is_dirty(DrawLayer layer);
void draw_clear_dirty(DrawLayer layer);
void draw_clear_layer(DrawLayer layer);
void draw_clear_current(void);
void draw_clear_all(void);
void draw_cleanup(void);
void draw_free_all_memory(void);

// Compatibility wrappers
void lines_draw(void);
bool is_lines_dirty(void);
bool is_bb_lines_dirty(void);
void clear_lines_dirty(void);
void clear_bb_lines_dirty(void);
void lines_clear(void);
void lines_erase_at(Vector2 screen_pos);
void bb_lines_clear(void);
void bb_lines_erase_at(Vector2 screen_pos);
void bb_lines_draw(void);
void hl_clear(void);
void bb_hl_clear(void);
void hl_lines_clear(void);
void hl_lines_erase_at(Vector2 screen_pos);
bool hl_render_rt(void);
void hl_composite(void);
void hl_lines_draw(void);

Color open_color_picker(Color current);

void draw_size_indicator(void);
void keymaps_render(void);

#define HIGHLIGHTER_ALPHA 0.3f

void toolbox_toggle(void);
bool toolbox_is_open(void);
bool toolbox_is_mouse_over(void);
void toolbox_handle_input(void);
void toolbox_render(void);
void toolbox_sync_size(void);
