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
  TOOL_COLOURPICKER,
} ToolType;

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
  ToolType current_tool;
  float   tool_pen_size;
  float   tool_eraser_size;
  Color   color2;
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
} Configuration;

extern Configuration g_default_configuration;
extern State         g_initial_state;

extern Configuration* g_configuration;
extern Args*          g_args;
extern State*         g_state;

void process_commandline_arguments(int argc, char** argv);

Image load_image_from_stdin(bool* out_was_file);

void handle_inputs(void);
void handle_draw(void);
void lines_draw(void);
void lines_clear(void);
void lines_erase_at(Vector2 screen_pos);
void bb_lines_clear(void);
void bb_lines_erase_at(Vector2 screen_pos);
void bb_lines_draw(void);
void hl_lines_clear(void);
void hl_lines_erase_at(Vector2 screen_pos);
void hl_lines_draw(void);

Color open_color_picker(Color current);

#define HIGHLIGHTER_ALPHA 0.3f

void toolbox_toggle(void);
bool toolbox_is_open(void);
bool toolbox_is_mouse_over(void);
void toolbox_handle_input(void);
void toolbox_render(void);
