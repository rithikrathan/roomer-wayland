#include "roomer.h"

typedef struct {
  Vector2* points;
  float    thickness;
  int      points_count;
  int      points_capacity;
  Color    color;
} Line;

static Line* lines          = NULL;
static int   lines_count    = 0;
static int   lines_capacity = 0;

static Line* bb_lines          = NULL;
static int   bb_lines_count    = 0;
static int   bb_lines_capacity = 0;

static Line* hl_lines          = NULL;
static int   hl_lines_count    = 0;
static int   hl_lines_capacity = 0;

static Line* bb_hl_lines          = NULL;
static int   bb_hl_lines_count    = 0;
static int   bb_hl_lines_capacity = 0;

static void    bb_line_begin(void);
static void    bb_line_add_point(Vector2 texture_pos);
static void    hl_line_begin(void);
static void    hl_line_add_point(Vector2 texture_pos);

static void    line_begin(void);
static void    line_add_point(Vector2 texture_pos);
static Vector2 to_texture_coords(Vector2 screen_pos);
static Vector2 to_screen_coords(Vector2 texture_pos);
static float   dist_to_segment(Vector2 p, Vector2 a, Vector2 b);

static Vector2 pen_screen_pos(void) {
  if (g_tablet.abs_x_max == 0 || g_tablet.abs_y_max == 0)
    return GetMousePosition();
  float sx = (float)g_tablet.abs_x / (float)g_tablet.abs_x_max * GetScreenWidth();
  float sy = (float)g_tablet.abs_y / (float)g_tablet.abs_y_max * GetScreenHeight();
  return (Vector2){ sx, sy };
}

static Color parse_yad_color(const char* str, Color fallback) {
  if (str[0] != '#') return fallback;
  unsigned int r = 0, g = 0, b = 0;
  sscanf(str + 1, "%02x%02x%02x", &r, &g, &b);
  return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}

Color open_color_picker(Color current) {
  char init[16];
  snprintf(init, sizeof(init), "#%02x%02x%02x", current.r, current.g, current.b);

  char cmd[256];
  int n = snprintf(cmd, sizeof(cmd), "yad --color --gtk-palette --picker --mode=hex --init-color=%s 2>/dev/null", init);
  if (n < 0 || n >= (int)sizeof(cmd)) return current;

  FILE* fp = popen(cmd, "r");
  if (!fp) return current;

  char buf[64] = { 0 };
  if (fgets(buf, (int)sizeof(buf), fp) == NULL) { pclose(fp); return current; }
  buf[strcspn(buf, "\n")] = 0;
  int rc = pclose(fp);
  if (rc != 0) return current;

  return parse_yad_color(buf, current);
}

void handle_draw(void) {
  bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool right_held = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

  if (g_state->toolbox_open && toolbox_is_mouse_over()) return;
  bool pen_down   = g_tablet.logical_pen_down
                    && !g_tablet.button1 && !g_tablet.button2 && !g_tablet.button3 && !ctrl;
  bool should_draw = right_held || pen_down;

  Vector2 pos = pen_down ? pen_screen_pos() : GetMousePosition();

  if (should_draw && !g_state->is_drawing) {
    if (g_state->current_tool == TOOL_ERASER) {
      hl_lines_erase_at(pos);
      if (g_state->black_board_enabled) bb_lines_erase_at(pos);
      else lines_erase_at(pos);
    } else if (g_state->current_tool == TOOL_HIGHLIGHTER) {
      hl_line_begin();
      g_state->is_drawing = true;
    } else {
      if (g_state->black_board_enabled) bb_line_begin();
      else line_begin();
      g_state->is_drawing = true;
    }
  }

  if (g_state->is_drawing && !right_held && !pen_down) {
    g_state->is_drawing = false;
  }

  if (g_state->is_drawing) {
    Vector2 tp = to_texture_coords(pos);
    if (g_state->current_tool == TOOL_HIGHLIGHTER) hl_line_add_point(tp);
    else if (g_state->black_board_enabled) bb_line_add_point(tp);
    else line_add_point(tp);
  }
}

void lines_draw(void) {
  for (int i = 0; i < lines_count; i++) {
    Line* line = &lines[i];

    int n = line->points_count;
    if (n == 0) continue;

    float screen_radius    = line->thickness * g_state->zoom;
    float screen_thickness = screen_radius * 2.0F;

    if (n == 1) {
      Vector2 p = to_screen_coords(line->points[0]);
      DrawCircleV(p, screen_radius, line->color);
      continue;
    }

    if (n == 2) {
      Vector2 p0 = to_screen_coords(line->points[0]);
      Vector2 p1 = to_screen_coords(line->points[1]);
      DrawLineEx(p0, p1, screen_thickness, line->color);
      DrawCircleV(p1, screen_radius, line->color);
      continue;
    }

    // start
    Vector2 start = to_screen_coords(line->points[0]);
    DrawCircleV(start, screen_radius, line->color);

    // segments
    for (int j = 0; j < n - 2; j++) {
      Vector2 P0 = to_screen_coords(line->points[j + 0]);
      Vector2 P1 = to_screen_coords(line->points[j + 1]);
      Vector2 P2 = to_screen_coords(line->points[j + 2]);

      Vector2 average_p0_p1 = Vector2Scale(Vector2Add(P0, P2), 0.5F);
      Vector2 ctrl          = Vector2Subtract(Vector2Scale(P1, 2.0F), average_p0_p1);

      DrawSplineSegmentBezierQuadratic(P0, ctrl, P2, screen_thickness, line->color);
    }

    // end
    Vector2 end = to_screen_coords(line->points[n - 1]);
    DrawCircleV(end, screen_radius, line->color);
  }
}

void lines_clear(void) {
  for (int i = 0; i < lines_count; i++) free(lines[i].points);
  free(lines);
  lines          = NULL;
  lines_count    = 0;
  lines_capacity = 0;
}

void lines_erase_at(Vector2 screen_pos) {
  for (int i = lines_count - 1; i >= 0; i--) {
    Line* line = &lines[i];
    float threshold = line->thickness * g_state->zoom + 8.0F;

    if (line->points_count == 1) {
      Vector2 p = to_screen_coords(line->points[0]);
      if (Vector2Distance(screen_pos, p) < threshold) goto remove;
      continue;
    }

    for (int j = 0; j < line->points_count - 1; j++) {
      Vector2 a = to_screen_coords(line->points[j]);
      Vector2 b = to_screen_coords(line->points[j + 1]);
      if (dist_to_segment(screen_pos, a, b) < threshold) goto remove;
    }
    continue;

  remove:
    free(line->points);
    memmove(&lines[i], &lines[i + 1], (lines_count - i - 1) * sizeof(Line));
    lines_count--;
    return;
  }
}

static void line_begin(void) {
  if (lines_count >= lines_capacity) {
    int   new_lines_capacity = (lines_capacity == 0) ? 4 : lines_capacity * 2;
    Line* new_lines          = realloc(lines, sizeof(Line) * new_lines_capacity);
    assert(new_lines);

    lines          = new_lines;
    lines_capacity = new_lines_capacity;
  }

  float size = (g_state->current_tool == TOOL_ERASER) ? g_state->tool_eraser_size : g_state->tool_pen_size;

  Color color = g_configuration->draw_color;

  Line* new_line = &lines[lines_count++];
  *new_line      = (Line){
    .thickness = size / g_state->zoom,
    .color     = color,
  };
}

static void line_add_point(Vector2 texture_pos) {
  Line* current_line = &lines[lines_count - 1];

  if (current_line->points_count >= current_line->points_capacity) {
    int      new_points_capacity = (current_line->points_capacity == 0) ? 64 : current_line->points_capacity * 2;
    Vector2* new_points          = realloc(current_line->points, sizeof(Vector2) * new_points_capacity);
    assert(new_points);

    current_line->points          = new_points;
    current_line->points_capacity = new_points_capacity;
  }

  current_line->points[current_line->points_count++] = texture_pos;
}

static void bb_line_begin(void) {
  if (bb_lines_count >= bb_lines_capacity) {
    int   new_cap = (bb_lines_capacity == 0) ? 4 : bb_lines_capacity * 2;
    Line* new_buf = realloc(bb_lines, sizeof(Line) * new_cap);
    assert(new_buf);
    bb_lines          = new_buf;
    bb_lines_capacity = new_cap;
  }

  float size = (g_state->current_tool == TOOL_ERASER) ? g_state->tool_eraser_size : g_state->tool_pen_size;
  Color color = g_configuration->draw_color;

  Line* line = &bb_lines[bb_lines_count++];
  *line = (Line){
    .thickness = size / g_state->zoom,
    .color     = color,
  };
}

static void bb_line_add_point(Vector2 texture_pos) {
  Line* line = &bb_lines[bb_lines_count - 1];
  if (line->points_count >= line->points_capacity) {
    int      new_cap = (line->points_capacity == 0) ? 64 : line->points_capacity * 2;
    Vector2* new_pts = realloc(line->points, sizeof(Vector2) * new_cap);
    assert(new_pts);
    line->points          = new_pts;
    line->points_capacity = new_cap;
  }
  line->points[line->points_count++] = texture_pos;
}

static void hl_line_begin(void) {
  Line** arr     = g_state->black_board_enabled ? &bb_hl_lines : &hl_lines;
  int*   cnt     = g_state->black_board_enabled ? &bb_hl_lines_count : &hl_lines_count;
  int*   cap     = g_state->black_board_enabled ? &bb_hl_lines_capacity : &hl_lines_capacity;
  int    cur_cnt = *cnt;
  int    cur_cap = *cap;
  if (cur_cnt >= cur_cap) {
    int   new_cap = (cur_cap == 0) ? 4 : cur_cap * 2;
    Line* new_buf = realloc(*arr, sizeof(Line) * new_cap);
    assert(new_buf);
    *arr          = new_buf;
    *cap          = new_cap;
  }
  float size = g_state->tool_pen_size;
  Color color = g_configuration->draw_color;
  float ha = HIGHLIGHTER_ALPHA * (color.a / 255.0f);
  color.a = (unsigned char)(ha * 255.0f);
  Line* line = &(*arr)[*cnt];
  *line = (Line){
    .thickness = size / g_state->zoom,
    .color     = color,
  };
  (*cnt)++;
}

static void hl_line_add_point(Vector2 texture_pos) {
  Line* arr = g_state->black_board_enabled ? bb_hl_lines : hl_lines;
  int   idx = (g_state->black_board_enabled ? bb_hl_lines_count : hl_lines_count) - 1;
  Line* line = &arr[idx];
  if (line->points_count >= line->points_capacity) {
    int      new_cap = (line->points_capacity == 0) ? 64 : line->points_capacity * 2;
    Vector2* new_pts = realloc(line->points, sizeof(Vector2) * new_cap);
    assert(new_pts);
    line->points          = new_pts;
    line->points_capacity = new_cap;
  }
  line->points[line->points_count++] = texture_pos;
}

static Vector2 to_texture_coords(Vector2 screen_pos) {
  return Vector2Scale(Vector2Subtract(screen_pos, g_state->pan), 1.0F / g_state->zoom);
}

static Vector2 to_screen_coords(Vector2 texture_pos) {
  return Vector2Add(Vector2Scale(texture_pos, g_state->zoom), g_state->pan);
}

void bb_lines_draw(void) {
  for (int i = 0; i < bb_lines_count; i++) {
    Line* line = &bb_lines[i];
    int n = line->points_count;
    if (n == 0) continue;
    float screen_radius    = line->thickness * g_state->zoom;
    float screen_thickness = screen_radius * 2.0F;
    if (n == 1) {
      DrawCircleV(to_screen_coords(line->points[0]), screen_radius, line->color);
      continue;
    }
    if (n == 2) {
      Vector2 p0 = to_screen_coords(line->points[0]);
      Vector2 p1 = to_screen_coords(line->points[1]);
      DrawLineEx(p0, p1, screen_thickness, line->color);
      DrawCircleV(p1, screen_radius, line->color);
      continue;
    }
    DrawCircleV(to_screen_coords(line->points[0]), screen_radius, line->color);
    for (int j = 0; j < n - 2; j++) {
      Vector2 P0 = to_screen_coords(line->points[j + 0]);
      Vector2 P1 = to_screen_coords(line->points[j + 1]);
      Vector2 P2 = to_screen_coords(line->points[j + 2]);
      Vector2 avg = Vector2Scale(Vector2Add(P0, P2), 0.5F);
      Vector2 ctrl = Vector2Subtract(Vector2Scale(P1, 2.0F), avg);
      DrawSplineSegmentBezierQuadratic(P0, ctrl, P2, screen_thickness, line->color);
    }
    DrawCircleV(to_screen_coords(line->points[n - 1]), screen_radius, line->color);
  }
}

void bb_lines_clear(void) {
  for (int i = 0; i < bb_lines_count; i++) free(bb_lines[i].points);
  free(bb_lines);
  bb_lines          = NULL;
  bb_lines_count    = 0;
  bb_lines_capacity = 0;
}

void bb_lines_erase_at(Vector2 screen_pos) {
  for (int i = bb_lines_count - 1; i >= 0; i--) {
    Line* line = &bb_lines[i];
    float threshold = line->thickness * g_state->zoom + 8.0F;
    if (line->points_count == 1) {
      if (Vector2Distance(screen_pos, to_screen_coords(line->points[0])) < threshold) goto remove;
      continue;
    }
    for (int j = 0; j < line->points_count - 1; j++) {
      Vector2 a = to_screen_coords(line->points[j]);
      Vector2 b = to_screen_coords(line->points[j + 1]);
      if (dist_to_segment(screen_pos, a, b) < threshold) goto remove;
    }
    continue;
  remove:
    free(line->points);
    memmove(&bb_lines[i], &bb_lines[i + 1], (bb_lines_count - i - 1) * sizeof(Line));
    bb_lines_count--;
    return;
  }
}

static RenderTexture2D s_hl_rt   = { 0 };
static int            s_hl_rt_w = 0;
static int            s_hl_rt_h = 0;

static void hl_draw_into_rt(Line* arr, int count) {
  for (int i = 0; i < count; i++) {
    Line* line = &arr[i];
    int n = line->points_count;
    if (n == 0) continue;
    Color c = line->color;
    c.a = 255;
    float screen_radius    = line->thickness * g_state->zoom;
    float screen_thickness = screen_radius * 2.0F;
    if (n == 1) {
      DrawCircleV(to_screen_coords(line->points[0]), screen_radius, c);
      continue;
    }
    if (n == 2) {
      Vector2 p0 = to_screen_coords(line->points[0]);
      Vector2 p1 = to_screen_coords(line->points[1]);
      DrawLineEx(p0, p1, screen_thickness, c);
      DrawCircleV(p1, screen_radius, c);
      continue;
    }
    DrawCircleV(to_screen_coords(line->points[0]), screen_radius, c);
    for (int j = 0; j < n - 2; j++) {
      Vector2 P0 = to_screen_coords(line->points[j + 0]);
      Vector2 P1 = to_screen_coords(line->points[j + 1]);
      Vector2 P2 = to_screen_coords(line->points[j + 2]);
      Vector2 avg = Vector2Scale(Vector2Add(P0, P2), 0.5F);
      Vector2 ctrl = Vector2Subtract(Vector2Scale(P1, 2.0F), avg);
      DrawSplineSegmentBezierQuadratic(P0, ctrl, P2, screen_thickness, c);
    }
    DrawCircleV(to_screen_coords(line->points[n - 1]), screen_radius, c);
  }
}

bool hl_render_rt(void) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  bool render_hl = hl_lines_count > 0 && !g_state->black_board_enabled;
  bool render_bb = bb_hl_lines_count > 0 && g_state->black_board_enabled;
  if (!render_hl && !render_bb) return false;

  if (s_hl_rt.id == 0 || s_hl_rt_w != sw || s_hl_rt_h != sh) {
    if (s_hl_rt.id != 0) UnloadRenderTexture(s_hl_rt);
    s_hl_rt   = LoadRenderTexture(sw, sh);
    s_hl_rt_w = sw;
    s_hl_rt_h = sh;
  }

  BeginTextureMode(s_hl_rt);
  ClearBackground((Color){ 0, 0, 0, 0 });
  if (render_hl) hl_draw_into_rt(hl_lines, hl_lines_count);
  if (render_bb) hl_draw_into_rt(bb_hl_lines, bb_hl_lines_count);
  EndTextureMode();
  return true;
}

void hl_composite(void) {
  if (s_hl_rt.id == 0) return;
  bool active = (hl_lines_count > 0 && !g_state->black_board_enabled) ||
                (bb_hl_lines_count > 0 && g_state->black_board_enabled);
  if (!active) return;
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  unsigned char ha_byte = (unsigned char)(255.0f * HIGHLIGHTER_ALPHA);
  Color tint = { ha_byte, ha_byte, ha_byte, ha_byte };
  BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
  DrawTextureRec(
    s_hl_rt.texture,
    (Rectangle){ 0, 0, (float)sw, (float)-sh },
    (Vector2){ 0, 0 },
    tint
  );
  EndBlendMode();
}

void hl_lines_draw(void) {
  if (hl_render_rt()) hl_composite();
}

void hl_lines_clear(void) {
  for (int i = 0; i < hl_lines_count; i++) free(hl_lines[i].points);
  free(hl_lines);
  hl_lines          = NULL;
  hl_lines_count    = 0;
  hl_lines_capacity = 0;
  for (int i = 0; i < bb_hl_lines_count; i++) free(bb_hl_lines[i].points);
  free(bb_hl_lines);
  bb_hl_lines          = NULL;
  bb_hl_lines_count    = 0;
  bb_hl_lines_capacity = 0;
  if (s_hl_rt.id != 0) {
    UnloadRenderTexture(s_hl_rt);
    s_hl_rt = (RenderTexture2D){ 0 };
    s_hl_rt_w = 0;
    s_hl_rt_h = 0;
  }
}

static int erase_one_array(Line** arr, int* cnt, Vector2 screen_pos) {
  for (int i = *cnt - 1; i >= 0; i--) {
    Line* line = &(*arr)[i];
    float threshold = line->thickness * g_state->zoom + 8.0F;
    if (line->points_count == 1) {
      if (Vector2Distance(screen_pos, to_screen_coords(line->points[0])) < threshold) goto remove;
      continue;
    }
    for (int j = 0; j < line->points_count - 1; j++) {
      Vector2 a = to_screen_coords(line->points[j]);
      Vector2 b = to_screen_coords(line->points[j + 1]);
      if (dist_to_segment(screen_pos, a, b) < threshold) goto remove;
    }
    continue;
  remove:
    free(line->points);
    memmove(&(*arr)[i], &(*arr)[i + 1], (*cnt - i - 1) * sizeof(Line));
    (*cnt)--;
    return 1;
  }
  return 0;
}

void hl_lines_erase_at(Vector2 screen_pos) {
  if (erase_one_array(&hl_lines, &hl_lines_count, screen_pos)) return;
  if (erase_one_array(&bb_hl_lines, &bb_hl_lines_count, screen_pos)) return;
}

static float dist_to_segment(Vector2 p, Vector2 a, Vector2 b) {
  Vector2 ab = Vector2Subtract(b, a);
  Vector2 ap = Vector2Subtract(p, a);
  float   t  = Clamp(Vector2DotProduct(ap, ab) / Vector2DotProduct(ab, ab), 0.0F, 1.0F);
  Vector2 closest = Vector2Add(a, Vector2Scale(ab, t));
  return Vector2Distance(p, closest);
}
