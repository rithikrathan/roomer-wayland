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
  TOOL_PEN = 0,
  TOOL_HIGHLIGHTER = 1,
  TOOL_ERASER = 2,
  TOOL_LINE = 3,
  TOOL_ARROW = 4,
  TOOL_TRIANGLE = 5,
  TOOL_RECTANGLE = 6,
  TOOL_CIRCLE = 7,
  TOOL_STEP_BADGE = 8,
  TOOL_TEXT = 9,
  TOOL_TABLE = 10,
  TOOL_COUNT = 11,
} ToolType;

typedef enum {
  STYLE_SOLID = 0,
  STYLE_DASHED = 1,
  STYLE_DOTTED = 2,
} StrokeStyle;

typedef enum {
  LAYER_IMAGE = 0,
  LAYER_BLACKBOARD = 1,
  LAYER_COUNT = 2,
} DrawLayer;

typedef enum {
  SHAPE_FREEHAND = 0,
  SHAPE_LINE = 1,
  SHAPE_ARROW = 2,
  SHAPE_TRIANGLE = 3,
  SHAPE_RECTANGLE = 4,
  SHAPE_CIRCLE = 5,
  SHAPE_STEP_BADGE = 6,
  SHAPE_TEXT = 7,
  SHAPE_TABLE = 8,
} ShapeType;

typedef struct {
  ShapeType   type;
  ToolType    tool;
  Vector2*    points;
  int         points_count;
  int         points_capacity;
  float       thickness;
  Color       color;
  StrokeStyle style;
  float       dash_len;
  float       dash_gap;
  bool        filled;
  Color       fill_color;
  int         step_number;
  char*       text;
  int         table_rows;
  int         table_cols;
} Stroke;

typedef struct {
  Vector2     pan;
  Vector2     target_pan;
  float       zoom;
  float       target_zoom;
  bool        flashlight_enabled;
  bool        flashlight_rendering;
  bool        flashlight_prev_enabled;
  float       flashlight_radius;
  float       flashlight_display_radius;
  float       flashlight_darkness;
  float       target_flashlight_radius;
  bool        is_drawing;
  bool        toolbox_open;
  bool        keymaps_open;
  ToolType    current_tool;
  float       tool_pen_size;
  float       tool_eraser_size;
  float       tool_highlighter_size;
  Color       color1;
  Color       color2;
  int         active_swatch;
  bool        black_board_enabled;
  int         image_w;
  int         image_h;
  // Shape & tool customizations:
  StrokeStyle shape_stroke_style;
  float       shape_dash_len;
  float       shape_dash_gap;
  bool        shape_filled;
  float       shape_fill_opacity;
  Color       fill_color;
  int         step_badge_counter;
  int         table_rows;
  int         table_cols;
  bool        is_editing_text;
  Vector2     text_edit_world_pos;
  char        text_buffer[1024];
  int         text_cursor;
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
void stroke_toggle_fill_last(void);
void step_badge_pop_last(void);
void text_commit_current(void);
Font get_app_font(void);

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
