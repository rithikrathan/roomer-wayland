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

typedef struct {
  Vector2 pan;
  float   zoom;
  bool    flashlight_enabled;
  float   flashlight_radius;
  bool    is_drawing;
} State;

typedef struct {
  char* program_name;
  char* screenshot_folder;
} Args;

typedef struct {
  char* window_title_boomermode;
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
