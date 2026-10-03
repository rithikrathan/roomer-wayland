#include "roomer.h"
#include <cairo/cairo.h>

// ── Types and State ─────────────────────────────────────────

typedef struct {
  Stroke* strokes;
  int     count;
  int     capacity;
  bool    dirty;
  bool    hl_dirty;
} StrokeLayer;

static StrokeLayer s_layers[LAYER_COUNT] = { 0 };

static Stroke* s_active_stroke = NULL;
static Vector2 s_last_pos      = { 0 };
static bool    s_has_last_pos  = false;

static RenderTexture2D s_hl_rt   = { 0 };
static int             s_hl_rt_w = 0;
static int             s_hl_rt_h = 0;

static inline StrokeLayer* get_layer(DrawLayer layer) {
  int idx = (int)layer;
  if (idx < 0 || idx >= LAYER_COUNT) idx = 0;
  return &s_layers[idx];
}

static inline StrokeLayer* get_active_layer(void) {
  return get_layer(g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE);
}

static inline Vector2 to_texture_coords(Vector2 screen_pos) {
  return Vector2Scale(Vector2Subtract(screen_pos, g_state->pan), 1.0F / g_state->zoom);
}

static inline Vector2 to_screen_coords(Vector2 texture_pos) {
  return Vector2Add(Vector2Scale(texture_pos, g_state->zoom), g_state->pan);
}

// ── Cursor / Pen position helper ────────────────────────────

Vector2 get_cursor_screen_pos(void) {
  if (g_tablet.present && (g_tablet.logical_pen_down || g_tablet.active) && g_tablet.abs_x_max > 0 && g_tablet.abs_y_max > 0) {
    float sx = (float)g_tablet.abs_x / (float)g_tablet.abs_x_max * (float)GetScreenWidth();
    float sy = (float)g_tablet.abs_y / (float)g_tablet.abs_y_max * (float)GetScreenHeight();
    return (Vector2){ sx, sy };
  }
  return GetMousePosition();
}

// ── Color Picker (yad) ──────────────────────────────────────

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

// ── Decimation ──────────────────────────────────────────────

static bool decimate(const Vector2* points, int count, Vector2 new_pos_world, float zoom) {
  if (count == 0) return false;
  float screen_dist = Vector2Distance(new_pos_world, points[count - 1]) * zoom;
  return screen_dist < 2.0f;
}

// ── Stroke Memory Management ────────────────────────────────

static void stroke_free_data(Stroke* stroke) {
  if (stroke->points) {
    free(stroke->points);
    stroke->points = NULL;
  }
  if (stroke->text) {
    free(stroke->text);
    stroke->text = NULL;
  }
  if (stroke->cached_tex.id != 0) {
    UnloadTexture(stroke->cached_tex);
    stroke->cached_tex = (Texture2D){ 0 };
  }
  stroke->points_count = 0;
  stroke->points_capacity = 0;
}

static void stroke_begin(DrawLayer layer, ToolType tool, ShapeType shape, float screen_size, Color color) {
  StrokeLayer* l = get_layer(layer);
  if (l->count >= l->capacity) {
    int new_cap = (l->capacity == 0) ? 8 : l->capacity * 2;
    Stroke* new_strokes = realloc(l->strokes, sizeof(Stroke) * new_cap);
    assert(new_strokes);
    l->strokes = new_strokes;
    l->capacity = new_cap;
  }

  Stroke* s = &l->strokes[l->count++];
  memset(s, 0, sizeof(Stroke));
  s->type = shape;
  s->tool = tool;
  s->thickness = screen_size / g_state->zoom;
  s->color = color;
  s->style = g_state->shape_stroke_style;
  s->dash_len = g_state->shape_dash_len;
  s->dash_gap = g_state->shape_dash_gap;
  s->filled = g_state->shape_filled;
  Color fc = g_state->fill_color;
  fc.a = (unsigned char)(g_state->shape_fill_opacity * 255.0f);
  s->fill_color = fc;
  s->step_number = g_state->step_badge_counter;
  s->badge_thickness = 2.0f;
  s->badge_size = g_state->badge_size;
  s->badge_mode = g_state->badge_mode;
  if (tool == TOOL_STEP_BADGE && g_state->badge_mode == BADGE_MODE_CUSTOM && g_state->badge_custom_text[0] != '\0') {
    s->text = strdup(g_state->badge_custom_text);
  }
  s->ngon_sides = g_state->ngon_sides;
  s->text_bold = g_state->text_bold;
  s->text_italic = g_state->text_italic;
  s->table_rows = g_state->table_rows;
  s->table_cols = g_state->table_cols;
  s->cached_tex = (Texture2D){ 0 };
  s->cached_zoom = 0.0f;
  s->cache_dirty = true;

  if (tool == TOOL_HIGHLIGHTER) {
    l->hl_dirty = true;
  } else {
    l->dirty = true;
  }
  s_active_stroke = s;
}

static void stroke_add_point(Vector2 texture_pos) {
  if (!s_active_stroke) return;
  if (s_active_stroke->type == SHAPE_FREEHAND &&
      decimate(s_active_stroke->points, s_active_stroke->points_count, texture_pos, g_state->zoom)) return;

  if (s_active_stroke->points_count >= s_active_stroke->points_capacity) {
    int new_cap = (s_active_stroke->points_capacity == 0) ? 32 : s_active_stroke->points_capacity * 2;
    Vector2* new_pts = realloc(s_active_stroke->points, sizeof(Vector2) * new_cap);
    assert(new_pts);
    s_active_stroke->points = new_pts;
    s_active_stroke->points_capacity = new_cap;
  }
  s_active_stroke->points[s_active_stroke->points_count++] = texture_pos;

  StrokeLayer* l = get_active_layer();
  if (s_active_stroke->tool == TOOL_HIGHLIGHTER) {
    l->hl_dirty = true;
  } else {
    l->dirty = true;
  }
}

static void stroke_end(void) {
  s_active_stroke = NULL;
  s_has_last_pos  = false;
}

void stroke_toggle_fill_last(void) {
  StrokeLayer* l = get_active_layer();
  for (int i = l->count - 1; i >= 0; i--) {
    Stroke* s = &l->strokes[i];
    if (s->type == SHAPE_POLYGON || s->type == SHAPE_NGON ||
        s->type == SHAPE_CIRCLE  || s->type == SHAPE_TABLE ||
        s->type == SHAPE_STEP_BADGE || s->type == SHAPE_TEXT) {
      s->filled = !s->filled;
      Color fc = g_state->fill_color;
      fc.a = (unsigned char)(g_state->shape_fill_opacity * 255.0f);
      s->fill_color = fc;
      l->dirty = true;
      g_state->shape_filled = s->filled;
      return;
    }
  }
  g_state->shape_filled = !g_state->shape_filled;
}

void step_badge_pop_last(void) {
  StrokeLayer* l = get_active_layer();
  for (int i = l->count - 1; i >= 0; i--) {
    if (l->strokes[i].type == SHAPE_STEP_BADGE) {
      stroke_free_data(&l->strokes[i]);
      for (int j = i; j < l->count - 1; j++) {
        l->strokes[j] = l->strokes[j + 1];
      }
      l->count--;
      l->dirty = true;
      if (g_state->step_badge_counter > 1) {
        g_state->step_badge_counter--;
      }
      return;
    }
  }
}

void badge_step_number_to_string(int num, BadgeMode mode, const char* custom, char* out, size_t out_sz) {
  if (!out || out_sz == 0) return;
  if (mode == BADGE_MODE_CUSTOM) {
    if (custom && custom[0] != '\0') {
      snprintf(out, out_sz, "%.15s", custom);
    } else {
      snprintf(out, out_sz, "1");
    }
    return;
  }
  if (mode == BADGE_MODE_ALPHA_UPPER || mode == BADGE_MODE_ALPHA_LOWER) {
    char buf[16];
    int idx = 0;
    int n = num;
    if (n <= 0) n = 1;
    while (n > 0 && idx < 14) {
      int rem = (n - 1) % 26;
      buf[idx++] = (mode == BADGE_MODE_ALPHA_UPPER ? 'A' : 'a') + rem;
      n = (n - 1) / 26;
    }
    int out_i = 0;
    for (int i = idx - 1; i >= 0 && (size_t)out_i < out_sz - 1; i--) {
      out[out_i++] = buf[i];
    }
    out[out_i] = '\0';
    return;
  }
  snprintf(out, out_sz, "%d", num);
}

void toggle_hide_overlay(void) {
  if (IsWindowState(FLAG_WINDOW_HIDDEN)) {
    ClearWindowState(FLAG_WINDOW_HIDDEN);
    SetWindowFocused();
    g_state->hide_overlay = false;
  } else {
    SetWindowState(FLAG_WINDOW_HIDDEN);
    g_state->hide_overlay = true;
    reset_all_variables(false);
  }
}

void polygon_cancel(void) {
  if (g_state->poly_pts) {
    free(g_state->poly_pts);
    g_state->poly_pts = NULL;
  }
  g_state->poly_pts_count = 0;
  g_state->poly_pts_capacity = 0;
  g_state->poly_active = false;
}

void polygon_pop_last_point(void) {
  if (!g_state->poly_active || g_state->poly_pts_count <= 0) return;
  g_state->poly_pts_count--;
  if (g_state->poly_pts_count == 0) {
    polygon_cancel();
  }
}

void polygon_commit(void) {
  if (!g_state->poly_active || g_state->poly_pts_count < 3) {
    polygon_cancel();
    return;
  }
  DrawLayer layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;
  stroke_begin(layer, TOOL_POLYGON, SHAPE_POLYGON, g_state->shape_thickness, g_state->shape_border_color);
  if (s_active_stroke) {
    s_active_stroke->points = malloc(sizeof(Vector2) * g_state->poly_pts_count);
    assert(s_active_stroke->points);
    memcpy(s_active_stroke->points, g_state->poly_pts, sizeof(Vector2) * g_state->poly_pts_count);
    s_active_stroke->points_count = g_state->poly_pts_count;
    s_active_stroke->points_capacity = g_state->poly_pts_count;
    s_active_stroke->filled = g_state->shape_filled;
    Color fc = g_state->fill_color;
    fc.a = (unsigned char)(g_state->shape_fill_opacity * 255.0f);
    s_active_stroke->fill_color = fc;
  }
  stroke_end();
  polygon_cancel();
}

static Texture2D s_live_tex = { 0 };
static char      s_last_live_buf[1024] = { 0 };
static float     s_last_live_sz = 0;
static bool      s_last_live_bold = false;
static bool      s_last_live_italic = false;
static Vector2   s_last_live_dim = { 0 };

void text_cancel_current(void) {
  g_state->is_editing_text = false;
  g_state->text_buffer[0] = '\0';
  g_state->text_cursor = 0;
  if (s_live_tex.id != 0) {
    UnloadTexture(s_live_tex);
    s_live_tex = (Texture2D){ 0 };
  }
  s_last_live_buf[0] = '\0';
  s_last_live_sz = 0;
  s_last_live_bold = false;
  s_last_live_italic = false;
  s_last_live_dim = (Vector2){ 0, 0 };
}

void text_commit_current(void) {
  if (!g_state->is_editing_text) return;
  if (strlen(g_state->text_buffer) > 0) {
    DrawLayer layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;
    StrokeLayer* l = get_layer(layer);
    if (l->count >= l->capacity) {
      int new_cap = (l->capacity == 0) ? 8 : l->capacity * 2;
      Stroke* new_strokes = realloc(l->strokes, sizeof(Stroke) * new_cap);
      assert(new_strokes);
      l->strokes = new_strokes;
      l->capacity = new_cap;
    }
    Stroke* s = &l->strokes[l->count++];
    memset(s, 0, sizeof(Stroke));
    s->type = SHAPE_TEXT;
    s->tool = TOOL_TEXT;
    s->thickness = g_state->text_font_size / g_state->zoom;
    s->color = g_state->shape_border_color;
    s->filled = g_state->shape_filled;
    Color fc = g_state->fill_color;
    fc.a = (unsigned char)(g_state->shape_fill_opacity * 255.0f);
    s->fill_color = fc;
    s->text = strdup(g_state->text_buffer);
    s->text_bold = g_state->text_bold;
    s->text_italic = g_state->text_italic;
    s->cached_tex = (Texture2D){ 0 };
    s->cached_zoom = 0.0f;
    s->cache_dirty = true;

    s->points = malloc(sizeof(Vector2));
    assert(s->points);
    s->points[0] = g_state->text_edit_world_pos;
    s->points_count = 1;
    s->points_capacity = 1;

    l->dirty = true;
  }
  text_cancel_current();
}

void draw_clear_layer(DrawLayer layer) {
  StrokeLayer* l = get_layer(layer);
  for (int i = 0; i < l->count; i++) {
    stroke_free_data(&l->strokes[i]);
  }
  free(l->strokes);
  l->strokes  = NULL;
  l->count    = 0;
  l->capacity = 0;
  l->dirty    = true;
  l->hl_dirty = true;
  if (s_active_stroke) s_active_stroke = NULL;
}

void draw_clear_current(void) {
  text_cancel_current();
  polygon_cancel();
  draw_clear_layer(g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE);
}

void draw_clear_all(void) {
  text_cancel_current();
  polygon_cancel();
  for (int i = 0; i < LAYER_COUNT; i++) {
    draw_clear_layer((DrawLayer)i);
  }
}

bool draw_is_dirty(DrawLayer layer) {
  StrokeLayer* l = get_layer(layer);
  return l->dirty || l->hl_dirty;
}

void draw_clear_dirty(DrawLayer layer) {
  get_layer(layer)->dirty = false;
}

void draw_cleanup(void) {
  text_cancel_current();
  if (s_hl_rt.id != 0) {
    UnloadRenderTexture(s_hl_rt);
    s_hl_rt   = (RenderTexture2D){ 0 };
    s_hl_rt_w = 0;
    s_hl_rt_h = 0;
  }
}

void draw_free_all_memory(void) {
  for (int i = 0; i < LAYER_COUNT; i++) {
    StrokeLayer* l = &s_layers[i];
    for (int j = 0; j < l->count; j++) {
      stroke_free_data(&l->strokes[j]);
    }
    free(l->strokes);
    l->strokes  = NULL;
    l->count    = 0;
    l->capacity = 0;
  }
}

// ── Styled Shape Rasterization ──────────────────────────────

static void draw_styled_segment(Vector2 a, Vector2 b, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap) {
  float len = Vector2Distance(a, b);
  if (len < 0.5f) return;
  float radius = thickness * 0.5f;

  if (style == STYLE_SOLID || dash_len <= 1.0f) {
    DrawLineEx(a, b, thickness, color);
    DrawCircleV(a, radius, color);
    DrawCircleV(b, radius, color);
    return;
  }

  Vector2 dir = Vector2Scale(Vector2Subtract(b, a), 1.0f / len);

  if (style == STYLE_DASHED) {
    float d_len = fmaxf(dash_len, 4.0f);
    float g_len = fmaxf(dash_gap, 2.0f);
    float t = 0.0f;
    while (t < len) {
      float t_end = fminf(t + d_len, len);
      Vector2 p0 = Vector2Add(a, Vector2Scale(dir, t));
      Vector2 p1 = Vector2Add(a, Vector2Scale(dir, t_end));
      DrawLineEx(p0, p1, thickness, color);
      DrawCircleV(p0, radius, color);
      DrawCircleV(p1, radius, color);
      t += d_len + g_len;
    }
  } else if (style == STYLE_DOTTED) {
    float dot_r = fmaxf(radius, 1.5f);
    float g_len = fmaxf(dash_gap, dot_r * 2.5f);
    float t = 0.0f;
    while (t <= len) {
      Vector2 p = Vector2Add(a, Vector2Scale(dir, t));
      DrawCircleV(p, dot_r, color);
      t += g_len;
    }
  }
}

static void draw_styled_arrow(Vector2 p0, Vector2 p1, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap) {
  float len = Vector2Distance(p0, p1);
  if (len < 1.0f) return;

  float head_len = fmaxf(thickness * 3.5f, 15.0f);
  if (head_len > len * 0.7f) head_len = len * 0.7f;
  float head_w   = head_len * 0.5f;

  Vector2 dir    = Vector2Scale(Vector2Subtract(p1, p0), 1.0f / len);
  Vector2 normal = (Vector2){ -dir.y, dir.x };

  Vector2 shaft_end = Vector2Subtract(p1, Vector2Scale(dir, head_len));
  draw_styled_segment(p0, shaft_end, thickness, color, style, dash_len, dash_gap);

  Vector2 a1 = Vector2Add(shaft_end, Vector2Scale(normal, head_w));
  Vector2 a2 = Vector2Subtract(shaft_end, Vector2Scale(normal, head_w));
  DrawTriangle(p1, a2, a1, color);
}

static inline float snap_angle_15(float rad) {
  const float step = 15.0f * DEG2RAD;
  return roundf(rad / step) * step;
}

static inline float ccw(Vector2 a, Vector2 b, Vector2 c) {
  return (c.y - a.y) * (b.x - a.x) - (b.y - a.y) * (c.x - a.x);
}

static inline bool point_in_triangle(Vector2 p, Vector2 a, Vector2 b, Vector2 c) {
  float c1 = ccw(a, b, p);
  float c2 = ccw(b, c, p);
  float c3 = ccw(c, a, p);
  return (c1 >= 0 && c2 >= 0 && c3 >= 0) || (c1 <= 0 && c2 <= 0 && c3 <= 0);
}

static void draw_filled_polygon(const Vector2* screen_pts, int n, Color fill_color) {
  if (n < 3) return;
  if (n == 3) {
    if (ccw(screen_pts[0], screen_pts[1], screen_pts[2]) > 0.0f) {
      DrawTriangle(screen_pts[0], screen_pts[1], screen_pts[2], fill_color);
    } else {
      DrawTriangle(screen_pts[0], screen_pts[2], screen_pts[1], fill_color);
    }
    return;
  }

  if (n > 256) n = 256;
  Vector2 pts[256];
  for (int i = 0; i < n; i++) pts[i] = screen_pts[i];

  float area = 0.0f;
  for (int i = 0; i < n; i++) {
    int next = (i + 1) % n;
    area += (pts[i].x * pts[next].y - pts[next].x * pts[i].y);
  }
  bool ccw_poly = (area > 0.0f);

  int count = n;
  int iter = 0;
  int max_iter = count * count * 2;
  int i = 0;

  while (count > 3 && iter++ < max_iter) {
    int prev = (i - 1 + count) % count;
    int next = (i + 1) % count;

    Vector2 a = pts[prev];
    Vector2 b = pts[i];
    Vector2 c = pts[next];

    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    bool is_convex = ccw_poly ? (cross > 0.001f) : (cross < -0.001f);

    if (is_convex) {
      bool ear = true;
      for (int j = 0; j < count; j++) {
        if (j == prev || j == i || j == next) continue;
        if (point_in_triangle(pts[j], a, b, c)) {
          ear = false;
          break;
        }
      }

      if (ear) {
        if (ccw(a, b, c) > 0.0f) {
          DrawTriangle(a, b, c, fill_color);
        } else {
          DrawTriangle(a, c, b, fill_color);
        }
        for (int k = i; k < count - 1; k++) {
          pts[k] = pts[k + 1];
        }
        count--;
        i = prev % count;
        continue;
      }
    }
    i = (i + 1) % count;
  }

  if (count == 3) {
    if (ccw(pts[0], pts[1], pts[2]) > 0.0f) {
      DrawTriangle(pts[0], pts[1], pts[2], fill_color);
    } else {
      DrawTriangle(pts[0], pts[2], pts[1], fill_color);
    }
  }
}

static void draw_styled_polygon(const Vector2* pts, int n, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap, bool filled, Color fill_color) {
  if (n < 3) return;
  if (filled) {
    draw_filled_polygon(pts, n, fill_color);
  }
  for (int i = 0; i < n; i++) {
    int next = (i + 1) % n;
    draw_styled_segment(pts[i], pts[next], thickness, color, style, dash_len, dash_gap);
  }
}

static void draw_styled_ngon(Vector2 center, Vector2 apex, int sides, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap, bool filled, Color fill_color) {
  if (sides < 3) sides = 3;
  if (sides > 16) sides = 16;

  Vector2 v = Vector2Subtract(apex, center);
  float radius = Vector2Length(v);
  if (radius < 1.0f) return;

  float base_angle = atan2f(v.y, v.x);
  float angle_step = (2.0f * PI) / (float)sides;

  Vector2 pts[16];
  for (int i = 0; i < sides; i++) {
    float a = base_angle + (float)i * angle_step;
    pts[i] = (Vector2){ center.x + cosf(a) * radius, center.y + sinf(a) * radius };
  }

  if (filled) {
    for (int i = 0; i < sides; i++) {
      int next = (i + 1) % sides;
      if (ccw(center, pts[i], pts[next]) > 0.0f) {
        DrawTriangle(center, pts[i], pts[next], fill_color);
      } else {
        DrawTriangle(center, pts[next], pts[i], fill_color);
      }
    }
  }

  for (int i = 0; i < sides; i++) {
    int next = (i + 1) % sides;
    draw_styled_segment(pts[i], pts[next], thickness, color, style, dash_len, dash_gap);
  }
}

static void draw_styled_circle(Vector2 center, float r, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap, bool filled, Color fill_color) {
  if (r <= 0.5f) return;
  if (filled) {
    DrawCircleV(center, r, fill_color);
  }

  float radius = thickness * 0.5f;
  if (style == STYLE_SOLID) {
    if (thickness > 1.5f) {
      DrawRing(center, fmaxf(0.0f, r - radius), r + radius, 0.0f, 360.0f, 72, color);
    } else {
      DrawCircleLines((int)center.x, (int)center.y, r, color);
    }
    return;
  }

  float circ = 2.0f * PI * r;
  if (style == STYLE_DASHED) {
    float d_len = fmaxf(dash_len, 4.0f);
    float g_len = fmaxf(dash_gap, 2.0f);
    float step_dist = d_len + g_len;
    int steps = (int)(circ / step_dist);
    if (steps < 4) steps = 4;
    float angle_per_step = (2.0f * PI) / (float)steps;
    float dash_angle = angle_per_step * (d_len / step_dist);

    for (int i = 0; i < steps; i++) {
      float a0 = (float)i * angle_per_step * RAD2DEG;
      float a1 = a0 + dash_angle * RAD2DEG;
      DrawRing(center, fmaxf(0.0f, r - radius), r + radius, a0, a1, 8, color);
    }
  } else if (style == STYLE_DOTTED) {
    float dot_r = fmaxf(radius, 1.5f);
    float g_len = fmaxf(dash_gap, dot_r * 2.5f);
    int steps = (int)(circ / g_len);
    if (steps < 4) steps = 4;
    float angle_per_step = (2.0f * PI) / (float)steps;

    for (int i = 0; i < steps; i++) {
      float a = (float)i * angle_per_step;
      Vector2 p = { center.x + cosf(a) * r, center.y + sinf(a) * r };
      DrawCircleV(p, dot_r, color);
    }
  }
}

static void draw_styled_table(Vector2 p0, Vector2 p1, int rows, int cols, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap, bool filled, Color fill_color) {
  if (rows < 1) rows = 1;
  if (cols < 1) cols = 1;

  float rx = fminf(p0.x, p1.x);
  float ry = fminf(p0.y, p1.y);
  float rw = fabsf(p1.x - p0.x);
  float rh = fabsf(p1.y - p0.y);

  if (rw < 4.0f || rh < 4.0f) return;

  if (filled) {
    DrawRectangleRec((Rectangle){ rx, ry, rw, rh }, fill_color);
  }

  // Outer border
  Vector2 tl = { rx, ry };
  Vector2 tr = { rx + rw, ry };
  Vector2 br = { rx + rw, ry + rh };
  Vector2 bl = { rx, ry + rh };

  draw_styled_segment(tl, tr, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(tr, br, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(br, bl, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(bl, tl, thickness, color, style, dash_len, dash_gap);

  // Horizontal divider lines
  for (int r = 1; r < rows; r++) {
    float y = ry + rh * ((float)r / (float)rows);
    draw_styled_segment((Vector2){ rx, y }, (Vector2){ rx + rw, y }, thickness, color, style, dash_len, dash_gap);
  }

  // Vertical divider lines
  for (int c = 1; c < cols; c++) {
    float x = rx + rw * ((float)c / (float)cols);
    draw_styled_segment((Vector2){ x, ry }, (Vector2){ x, ry + rh }, thickness, color, style, dash_len, dash_gap);
  }
}

static Texture2D render_cairo_text(const char* text, float font_size, bool bold, bool italic, Color text_color, bool filled, Color fill_color, Vector2* out_dim) {
  if (!text || text[0] == '\0') text = " ";
  if (font_size < 10.0f) font_size = 10.0f;

  cairo_surface_t* temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t* cr_m = cairo_create(temp_surf);
  cairo_select_font_face(cr_m, "Sans",
                         italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
                         bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr_m, font_size);

  char* dup = strdup(text);
  char* line = strtok(dup, "\n");
  double max_w = 0;
  int num_lines = 0;
  while (line) {
    cairo_text_extents_t ext;
    cairo_text_extents(cr_m, line, &ext);
    if (ext.x_advance > max_w) max_w = ext.x_advance;
    num_lines++;
    line = strtok(NULL, "\n");
  }
  free(dup);
  cairo_destroy(cr_m);
  cairo_surface_destroy(temp_surf);

  if (num_lines == 0) num_lines = 1;
  float line_height = font_size * 1.35f;
  float pad = 8.0f;
  int img_w = (int)ceilf((float)max_w + pad * 2.0f);
  int img_h = (int)ceilf((float)num_lines * line_height + pad * 2.0f);
  if (img_w < 16) img_w = 16;
  if (img_h < 16) img_h = 16;

  if (out_dim) {
    out_dim->x = (float)img_w;
    out_dim->y = (float)img_h;
  }

  cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, img_w, img_h);
  cairo_t* cr = cairo_create(surf);
  cairo_select_font_face(cr, "Sans",
                         italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
                         bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, font_size);

  if (filled) {
    cairo_rectangle(cr, 1.0, 1.0, img_w - 2.0, img_h - 2.0);
    cairo_set_source_rgba(cr, fill_color.r / 255.0, fill_color.g / 255.0, fill_color.b / 255.0, fill_color.a / 255.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, text_color.r / 255.0, text_color.g / 255.0, text_color.b / 255.0, 0.85);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
  } else {
    cairo_rectangle(cr, 1.0, 1.0, img_w - 2.0, img_h - 2.0);
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, 0.5);
    cairo_fill(cr);
  }

  cairo_set_source_rgba(cr, text_color.r / 255.0, text_color.g / 255.0, text_color.b / 255.0, text_color.a / 255.0);
  dup = strdup(text);
  line = strtok(dup, "\n");
  int line_idx = 0;
  while (line) {
    cairo_move_to(cr, pad, pad + (line_idx + 0.85f) * line_height);
    cairo_show_text(cr, line);
    line_idx++;
    line = strtok(NULL, "\n");
  }
  free(dup);

  cairo_surface_flush(surf);
  unsigned char* cairo_data = cairo_image_surface_get_data(surf);
  int stride = cairo_image_surface_get_stride(surf);

  unsigned char* rgba = malloc(img_w * img_h * 4);
  assert(rgba);
  for (int y = 0; y < img_h; y++) {
    uint32_t* row = (uint32_t*)(cairo_data + y * stride);
    for (int x = 0; x < img_w; x++) {
      uint32_t pixel = row[x];
      uint8_t a = (pixel >> 24) & 0xFF;
      uint8_t r = (pixel >> 16) & 0xFF;
      uint8_t g = (pixel >> 8) & 0xFF;
      uint8_t b = pixel & 0xFF;
      if (a > 0 && a < 255) {
        r = (uint8_t)fminf(255.0f, (float)r * 255.0f / (float)a);
        g = (uint8_t)fminf(255.0f, (float)g * 255.0f / (float)a);
        b = (uint8_t)fminf(255.0f, (float)b * 255.0f / (float)a);
      }
      int idx = (y * img_w + x) * 4;
      rgba[idx + 0] = r;
      rgba[idx + 1] = g;
      rgba[idx + 2] = b;
      rgba[idx + 3] = a;
    }
  }

  Image img = {
    .data = rgba,
    .width = img_w,
    .height = img_h,
    .mipmaps = 1,
    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  Texture2D tex = LoadTextureFromImage(img);
  UnloadImage(img);
  cairo_destroy(cr);
  cairo_surface_destroy(surf);
  return tex;
}

static Texture2D render_cairo_badge(const char* label, float radius, float border_w, Color border_color, bool filled, Color fill_color, Vector2* out_dim) {
  if (!label || label[0] == '\0') label = "1";
  if (radius < 12.0f) radius = 12.0f;
  if (border_w < 1.0f) border_w = 1.0f;

  cairo_surface_t* temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t* cr_m = cairo_create(temp_surf);
  cairo_select_font_face(cr_m, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  double font_size = radius * 1.15;
  cairo_set_font_size(cr_m, font_size);

  cairo_text_extents_t ext;
  cairo_text_extents(cr_m, label, &ext);
  double text_w = ext.width;
  double text_h = ext.height;
  cairo_destroy(cr_m);
  cairo_surface_destroy(temp_surf);

  double diam = radius * 2.0;
  double pad_x = radius * 0.55;
  double badge_w = fmax(diam, text_w + pad_x * 2.0);
  double badge_h = diam;

  int pad_margin = (int)ceilf(border_w) + 4;
  int img_w = (int)ceilf(badge_w + pad_margin * 2.0);
  int img_h = (int)ceilf(badge_h + pad_margin * 2.0);
  if (img_w < 32) img_w = 32;
  if (img_h < 32) img_h = 32;

  if (out_dim) {
    out_dim->x = (float)img_w;
    out_dim->y = (float)img_h;
  }

  cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, img_w, img_h);
  cairo_t* cr = cairo_create(surf);
  double cx = img_w * 0.5;
  double cy = img_h * 0.5;
  double r = radius;

  if (badge_w <= diam + 0.1) {
    cairo_arc(cr, cx, cy, r, 0, 2.0 * M_PI);
  } else {
    double x_left  = cx - (badge_w * 0.5) + r;
    double x_right = cx + (badge_w * 0.5) - r;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x_right, cy, r, -M_PI_2, M_PI_2);
    cairo_arc(cr, x_left,  cy, r, M_PI_2, 3.0 * M_PI_2);
    cairo_close_path(cr);
  }

  if (filled) {
    cairo_set_source_rgba(cr, fill_color.r / 255.0, fill_color.g / 255.0, fill_color.b / 255.0, fill_color.a / 255.0);
    cairo_fill_preserve(cr);
  } else {
    cairo_set_source_rgba(cr, 0.08, 0.08, 0.08, 0.85);
    cairo_fill_preserve(cr);
  }

  cairo_set_source_rgba(cr, border_color.r / 255.0, border_color.g / 255.0, border_color.b / 255.0, border_color.a / 255.0);
  cairo_set_line_width(cr, border_w);
  cairo_stroke(cr);

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, font_size);

  double tx = cx - (text_w / 2.0 + ext.x_bearing);
  double ty = cy - (text_h / 2.0 + ext.y_bearing);

  if (filled) {
    float lum = 0.299f * fill_color.r + 0.587f * fill_color.g + 0.114f * fill_color.b;
    if (lum > 150.0f && fill_color.a > 120) {
      cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, 1.0);
    } else {
      cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    }
  } else {
    cairo_set_source_rgba(cr, border_color.r / 255.0, border_color.g / 255.0, border_color.b / 255.0, 1.0);
  }

  cairo_move_to(cr, tx, ty);
  cairo_show_text(cr, label);

  cairo_surface_flush(surf);
  unsigned char* cairo_data = cairo_image_surface_get_data(surf);
  int stride = cairo_image_surface_get_stride(surf);

  unsigned char* rgba = malloc(img_w * img_h * 4);
  assert(rgba);
  for (int y = 0; y < img_h; y++) {
    uint32_t* row = (uint32_t*)(cairo_data + y * stride);
    for (int x = 0; x < img_w; x++) {
      uint32_t pixel = row[x];
      uint8_t a = (pixel >> 24) & 0xFF;
      uint8_t r_col = (pixel >> 16) & 0xFF;
      uint8_t g = (pixel >> 8) & 0xFF;
      uint8_t b = pixel & 0xFF;
      if (a > 0 && a < 255) {
        r_col = (uint8_t)fminf(255.0f, (float)r_col * 255.0f / (float)a);
        g = (uint8_t)fminf(255.0f, (float)g * 255.0f / (float)a);
        b = (uint8_t)fminf(255.0f, (float)b * 255.0f / (float)a);
      }
      int idx = (y * img_w + x) * 4;
      rgba[idx + 0] = r_col;
      rgba[idx + 1] = g;
      rgba[idx + 2] = b;
      rgba[idx + 3] = a;
    }
  }

  Image img = {
    .data = rgba,
    .width = img_w,
    .height = img_h,
    .mipmaps = 1,
    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  Texture2D tex = LoadTextureFromImage(img);
  UnloadImage(img);
  cairo_destroy(cr);
  cairo_surface_destroy(surf);
  return tex;
}

static void update_cairo_text_texture(Stroke* s, float zoom) {
  if (s->cached_tex.id != 0) {
    UnloadTexture(s->cached_tex);
    s->cached_tex = (Texture2D){ 0 };
  }
  float font_size = s->thickness * zoom;
  if (font_size < 10.0f) font_size = 10.0f;
  s->cached_tex = render_cairo_text(s->text, font_size, s->text_bold, s->text_italic, s->color, s->filled, s->fill_color, NULL);
  s->cached_zoom = zoom;
  s->cache_dirty = false;
}

static void update_cairo_badge_texture(Stroke* s, float zoom) {
  if (s->cached_tex.id != 0) {
    UnloadTexture(s->cached_tex);
    s->cached_tex = (Texture2D){ 0 };
  }
  float radius = fmaxf(s->badge_size > 0.0f ? s->badge_size : 22.0f, 14.0f) * zoom;
  float border_w = (s->badge_thickness > 0.0f ? s->badge_thickness : 2.0f) * zoom;
  char label[32];
  badge_step_number_to_string(s->step_number, s->badge_mode, s->text, label, sizeof(label));
  s->cached_tex = render_cairo_badge(label, radius, border_w, s->color, s->filled, s->fill_color, NULL);
  s->cached_zoom = zoom;
  s->cache_dirty = false;
}

// ── Stroke Rendering (Smooth Bezier & Shapes) ───────────────

static void render_stroke(const Stroke* stroke, float zoom, Vector2 pan, Color color) {
  (void)pan;
  int n = stroke->points_count;
  if (n <= 0) return;

  float screen_radius    = stroke->thickness * zoom;
  float screen_thickness = screen_radius * 2.0f;
  float d_len = stroke->dash_len * zoom;
  float d_gap = stroke->dash_gap * zoom;

  switch (stroke->type) {
    case SHAPE_FREEHAND: {
      if (n == 1) {
        Vector2 p = to_screen_coords(stroke->points[0]);
        DrawCircleV(p, screen_radius, color);
        return;
      }
      if (n == 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        DrawLineEx(p0, p1, screen_thickness, color);
        DrawCircleV(p0, screen_radius, color);
        DrawCircleV(p1, screen_radius, color);
        return;
      }

      Vector2 p0 = to_screen_coords(stroke->points[0]);
      Vector2 p1 = to_screen_coords(stroke->points[1]);
      Vector2 pn = to_screen_coords(stroke->points[n - 1]);

      DrawCircleV(p0, screen_radius, color);
      DrawCircleV(pn, screen_radius, color);

      Vector2 m_prev = Vector2Scale(Vector2Add(p0, p1), 0.5f);
      DrawLineEx(p0, m_prev, screen_thickness, color);

      for (int j = 1; j < n - 1; j++) {
        Vector2 pj   = to_screen_coords(stroke->points[j]);
        Vector2 pj1  = to_screen_coords(stroke->points[j + 1]);
        Vector2 m_curr = Vector2Scale(Vector2Add(pj, pj1), 0.5f);

        DrawSplineSegmentBezierQuadratic(m_prev, pj, m_curr, screen_thickness, color);
        m_prev = m_curr;
      }

      DrawLineEx(m_prev, pn, screen_thickness, color);
      break;
    }
    case SHAPE_LINE: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        draw_styled_segment(p0, p1, screen_thickness, color, stroke->style, d_len, d_gap);
      }
      break;
    }
    case SHAPE_ARROW: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        draw_styled_arrow(p0, p1, screen_thickness, color, stroke->style, d_len, d_gap);
      }
      break;
    }
    case SHAPE_POLYGON: {
      if (n >= 3) {
        Vector2* s_pts = malloc(sizeof(Vector2) * n);
        assert(s_pts);
        for (int i = 0; i < n; i++) {
          s_pts[i] = to_screen_coords(stroke->points[i]);
        }
        draw_styled_polygon(s_pts, n, screen_thickness, color, stroke->style, d_len, d_gap, stroke->filled, stroke->fill_color);
        free(s_pts);
      }
      break;
    }
    case SHAPE_NGON: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        int sides = (stroke->ngon_sides >= 3 && stroke->ngon_sides <= 16) ? stroke->ngon_sides : 6;
        draw_styled_ngon(p0, p1, sides, screen_thickness, color, stroke->style, d_len, d_gap, stroke->filled, stroke->fill_color);
      }
      break;
    }
    case SHAPE_CIRCLE: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        float r = Vector2Distance(p0, p1);
        draw_styled_circle(p0, r, screen_thickness, color, stroke->style, d_len, d_gap, stroke->filled, stroke->fill_color);
      }
      break;
    }
    case SHAPE_TABLE: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        draw_styled_table(p0, p1, stroke->table_rows, stroke->table_cols, screen_thickness, color, stroke->style, d_len, d_gap, stroke->filled, stroke->fill_color);
      }
      break;
    }
    case SHAPE_STEP_BADGE: {
      if (n >= 1) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Stroke* mut_s = (Stroke*)stroke;
        if (mut_s->cache_dirty || mut_s->cached_tex.id == 0 || fabsf(mut_s->cached_zoom - zoom) > 0.05f) {
          update_cairo_badge_texture(mut_s, zoom);
        }
        if (mut_s->cached_tex.id != 0) {
          float w = (float)mut_s->cached_tex.width;
          float h = (float)mut_s->cached_tex.height;
          DrawTexture(mut_s->cached_tex, (int)roundf(p0.x - w * 0.5f), (int)roundf(p0.y - h * 0.5f), WHITE);
        }
      }
      break;
    }
    case SHAPE_TEXT: {
      if (n >= 1 && stroke->text) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Stroke* mut_s = (Stroke*)stroke;
        if (mut_s->cache_dirty || mut_s->cached_tex.id == 0 || fabsf(mut_s->cached_zoom - zoom) > 0.05f) {
          update_cairo_text_texture(mut_s, zoom);
        }
        if (mut_s->cached_tex.id != 0) {
          DrawTexture(mut_s->cached_tex, (int)roundf(p0.x), (int)roundf(p0.y), WHITE);
        }
      }
      break;
    }
  }
}

void draw_layer_normal(DrawLayer layer) {
  rlDrawRenderBatchActive();
  rlColorMask(true, true, true, false);

  StrokeLayer* l = get_layer(layer);
  for (int i = 0; i < l->count; i++) {
    const Stroke* s = &l->strokes[i];
    if (s->tool == TOOL_HIGHLIGHTER) continue;
    render_stroke(s, g_state->zoom, g_state->pan, s->color);
  }

  // Active text editing overlay
  if (g_state->is_editing_text) {
    DrawLayer active_layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;
    if (layer == active_layer) {
      Vector2 sp = to_screen_coords(g_state->text_edit_world_pos);
      float font_size = g_state->text_font_size;
      Color c = g_state->shape_border_color;
      Color fc = g_state->fill_color;
      fc.a = (unsigned char)(g_state->shape_fill_opacity * 255.0f);

      if (s_live_tex.id == 0 || strcmp(s_last_live_buf, g_state->text_buffer) != 0 ||
          s_last_live_sz != font_size || s_last_live_bold != g_state->text_bold ||
          s_last_live_italic != g_state->text_italic) {
        if (s_live_tex.id != 0) UnloadTexture(s_live_tex);
        s_live_tex = render_cairo_text(g_state->text_buffer, font_size, g_state->text_bold, g_state->text_italic, c, g_state->shape_filled, fc, &s_last_live_dim);
        strncpy(s_last_live_buf, g_state->text_buffer, sizeof(s_last_live_buf) - 1);
        s_last_live_sz = font_size;
        s_last_live_bold = g_state->text_bold;
        s_last_live_italic = g_state->text_italic;
      }

      if (s_live_tex.id != 0) {
        DrawTexture(s_live_tex, (int)roundf(sp.x), (int)roundf(sp.y), WHITE);
      }

      DrawRectangleLinesEx((Rectangle){ sp.x, sp.y, s_last_live_dim.x, s_last_live_dim.y }, 1.5f, (Color){ 80, 140, 220, 255 });

      // Blinking cursor
      if (fmod(GetTime(), 0.8) < 0.4) {
        float cursor_x = sp.x + s_last_live_dim.x - 6.0f;
        float cursor_y = sp.y + 4.0f;
        DrawLineEx((Vector2){ cursor_x, cursor_y }, (Vector2){ cursor_x, cursor_y + font_size }, 2.0f, (Color){ 255, 255, 255, 240 });
      }
    }
  }

  // Active polygon drawing preview
  if (g_state->poly_active && g_state->poly_pts_count > 0) {
    DrawLayer active_layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;
    if (layer == active_layer) {
      Vector2 spos = get_cursor_screen_pos();
      Vector2 wpos = to_texture_coords(spos);

      if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        Vector2 prev = g_state->poly_pts[g_state->poly_pts_count - 1];
        float dx = wpos.x - prev.x;
        float dy = wpos.y - prev.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.001f) {
          float ang = snap_angle_15(atan2f(dy, dx));
          wpos.x = prev.x + cosf(ang) * dist;
          wpos.y = prev.y + sinf(ang) * dist;
        }
      }
      Vector2 cur_screen = to_screen_coords(wpos);

      float screen_thickness = g_state->shape_thickness * g_state->zoom * 2.0f;
      float d_len = g_state->shape_dash_len * g_state->zoom;
      float d_gap = g_state->shape_dash_gap * g_state->zoom;
      Color c = g_state->shape_border_color;

      for (int i = 0; i < g_state->poly_pts_count - 1; i++) {
        Vector2 a = to_screen_coords(g_state->poly_pts[i]);
        Vector2 b = to_screen_coords(g_state->poly_pts[i + 1]);
        draw_styled_segment(a, b, screen_thickness, c, g_state->shape_stroke_style, d_len, d_gap);
        DrawCircleV(a, screen_thickness * 0.6f, c);
      }
      Vector2 last_screen = to_screen_coords(g_state->poly_pts[g_state->poly_pts_count - 1]);
      DrawCircleV(last_screen, screen_thickness * 0.6f, c);

      draw_styled_segment(last_screen, cur_screen, screen_thickness, c, g_state->shape_stroke_style, d_len, d_gap);

      if (g_state->poly_pts_count >= 2) {
        Vector2 p0_screen = to_screen_coords(g_state->poly_pts[0]);
        bool hover_p0 = (Vector2Distance(spos, p0_screen) < 15.0f);
        if (hover_p0) {
          DrawCircleV(p0_screen, screen_thickness + 4.0f, (Color){ 80, 220, 100, 200 });
          DrawCircleLines((int)p0_screen.x, (int)p0_screen.y, screen_thickness + 6.0f, WHITE);
        } else {
          DrawCircleV(p0_screen, screen_thickness * 0.8f, (Color){ 80, 180, 255, 200 });
        }
        draw_styled_segment(cur_screen, p0_screen, fmaxf(screen_thickness * 0.5f, 1.5f), (Color){ c.r, c.g, c.b, 160 }, STYLE_DASHED, 6.0f, 6.0f);
      }
    }
  }

  rlDrawRenderBatchActive();
  rlColorMask(true, true, true, true);
}

// ── Dot Grid ────────────────────────────────────────────────

void draw_dot_grid(int sw, int sh, Vector2 pan, float zoom) {
  float spacing = 50.0F * zoom;
  if (spacing < 4.0F) spacing = 4.0F;

  float dot_r = 1.5F;
  if (zoom > 1.0F) dot_r = 1.5F + (zoom - 1.0F) * 0.2F;
  for (float sx = fmodf(pan.x, spacing) - spacing; sx < (float)sw; sx += spacing) {
    for (float sy = fmodf(pan.y, spacing) - spacing; sy < (float)sh; sy += spacing) {
      DrawCircleV((Vector2){ sx, sy }, dot_r, (Color){ 60, 60, 60, 255 });
    }
  }

  if (zoom > 4.0F) {
    float half_sp = spacing * 0.5F;
    float mini_r  = dot_r * 0.35F;
    Color mini_c  = (Color){ 50, 50, 50, 200 };
    for (float sx = fmodf(pan.x + half_sp, spacing) - spacing; sx < (float)sw; sx += spacing) {
      for (float sy = fmodf(pan.y + half_sp, spacing) - spacing; sy < (float)sh; sy += spacing) {
        DrawCircleV((Vector2){ sx, sy }, mini_r, mini_c);
      }
    }
  }
}

// ── Highlighter Rendering ───────────────────────────────────

void draw_render_highlighter(DrawLayer layer, bool view_changed) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  if (sw <= 0 || sh <= 0) return;

  StrokeLayer* l = get_layer(layer);
  int hl_count = 0;
  for (int i = 0; i < l->count; i++) {
    if (l->strokes[i].tool == TOOL_HIGHLIGHTER) hl_count++;
  }

  if (s_hl_rt.id == 0 || s_hl_rt_w != sw || s_hl_rt_h != sh) {
    if (s_hl_rt.id != 0) UnloadRenderTexture(s_hl_rt);
    s_hl_rt   = LoadRenderTexture(sw, sh);
    s_hl_rt_w = sw;
    s_hl_rt_h = sh;
    l->hl_dirty = true;
  }

  if (!view_changed && !l->hl_dirty) return;

  BeginTextureMode(s_hl_rt);
  ClearBackground((Color){ 0, 0, 0, 0 });
  if (hl_count > 0) {
    for (int i = 0; i < l->count; i++) {
      const Stroke* s = &l->strokes[i];
      if (s->tool != TOOL_HIGHLIGHTER) continue;
      Color c = s->color;
      c.a = 255;
      render_stroke(s, g_state->zoom, g_state->pan, c);
    }
  }
  EndTextureMode();
  l->hl_dirty = false;
}

void draw_composite_highlighter(void) {
  if (s_hl_rt.id == 0) return;
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  unsigned char ha_byte = (unsigned char)(255.0f * HIGHLIGHTER_ALPHA);
  Color tint = { ha_byte, ha_byte, ha_byte, ha_byte };

  rlDrawRenderBatchActive();
  rlColorMask(true, true, true, false);

  BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
  DrawTextureRec(
    s_hl_rt.texture,
    (Rectangle){ 0, 0, (float)sw, (float)-sh },
    (Vector2){ 0, 0 },
    tint
  );
  EndBlendMode();

  rlDrawRenderBatchActive();
  rlColorMask(true, true, true, true);
}

// ── Eraser Collision & Hit Testing ──────────────────────────

static bool segments_intersect(Vector2 a, Vector2 b, Vector2 c, Vector2 d) {
  return ((ccw(a, c, d) > 0.0f) != (ccw(b, c, d) > 0.0f)) &&
         ((ccw(a, b, c) > 0.0f) != (ccw(a, b, d) > 0.0f));
}

static float dist_to_segment(Vector2 p, Vector2 a, Vector2 b) {
  Vector2 ab = Vector2Subtract(b, a);
  Vector2 ap = Vector2Subtract(p, a);
  float ab_len2 = Vector2DotProduct(ab, ab);
  if (ab_len2 <= 0.00001f) return Vector2Distance(p, a);
  float t = Clamp(Vector2DotProduct(ap, ab) / ab_len2, 0.0f, 1.0f);
  Vector2 closest = Vector2Add(a, Vector2Scale(ab, t));
  return Vector2Distance(p, closest);
}

static float dist_segment_to_segment(Vector2 p0, Vector2 p1, Vector2 q0, Vector2 q1) {
  if (segments_intersect(p0, p1, q0, q1)) return 0.0f;
  float d = dist_to_segment(p0, q0, q1);
  d = fminf(d, dist_to_segment(p1, q0, q1));
  d = fminf(d, dist_to_segment(q0, p0, p1));
  d = fminf(d, dist_to_segment(q1, p0, p1));
  return d;
}

static bool stroke_hit_test(const Stroke* stroke, Vector2 p0, Vector2 p1, float eraser_radius, float zoom, Vector2 pan) {
  (void)pan;
  int n = stroke->points_count;
  if (n <= 0) return false;

  float stroke_radius = stroke->thickness * zoom;
  float threshold     = eraser_radius + stroke_radius;

  if (n == 1) {
    Vector2 sp = to_screen_coords(stroke->points[0]);
    return dist_to_segment(sp, p0, p1) < threshold;
  }

  for (int j = 0; j < n - 1; j++) {
    Vector2 q0 = to_screen_coords(stroke->points[j]);
    Vector2 q1 = to_screen_coords(stroke->points[j + 1]);
    if (dist_segment_to_segment(p0, p1, q0, q1) < threshold) {
      return true;
    }
  }

  if (stroke->type == SHAPE_POLYGON && n >= 3) {
    for (int i = 0; i < n; i++) {
      int next = (i + 1) % n;
      Vector2 a = to_screen_coords(stroke->points[i]);
      Vector2 b = to_screen_coords(stroke->points[next]);
      if (dist_segment_to_segment(p0, p1, a, b) < threshold) return true;
    }
  }

  if (stroke->type == SHAPE_NGON && n >= 2) {
    Vector2 center = to_screen_coords(stroke->points[0]);
    Vector2 apex   = to_screen_coords(stroke->points[1]);
    int sides = (stroke->ngon_sides >= 3 && stroke->ngon_sides <= 16) ? stroke->ngon_sides : 6;
    Vector2 v = Vector2Subtract(apex, center);
    float radius = Vector2Length(v);
    if (radius >= 1.0f) {
      float base_angle = atan2f(v.y, v.x);
      float angle_step = (2.0f * PI) / (float)sides;
      Vector2 pts[16];
      for (int i = 0; i < sides; i++) {
        float a = base_angle + (float)i * angle_step;
        pts[i] = (Vector2){ center.x + cosf(a) * radius, center.y + sinf(a) * radius };
      }
      for (int i = 0; i < sides; i++) {
        int next = (i + 1) % sides;
        if (dist_segment_to_segment(p0, p1, pts[i], pts[next]) < threshold) return true;
      }
      if (stroke->filled && dist_to_segment(center, p0, p1) < radius + eraser_radius) return true;
    }
  }

  if (stroke->type == SHAPE_CIRCLE && n >= 2) {
    Vector2 c = to_screen_coords(stroke->points[0]);
    Vector2 edge = to_screen_coords(stroke->points[1]);
    float r = Vector2Distance(c, edge);
    float d = dist_to_segment(c, p0, p1);
    if (fabsf(d - r) < threshold) return true;
    if (stroke->filled && d <= r + eraser_radius) return true;
  }

  if (stroke->type == SHAPE_STEP_BADGE && n >= 1) {
    Vector2 sp = to_screen_coords(stroke->points[0]);
    float badge_r = fmaxf((stroke->badge_size > 0.0f ? stroke->badge_size : 22.0f) * zoom, 14.0f);
    if (dist_to_segment(sp, p0, p1) < badge_r + eraser_radius) return true;
  }

  if (stroke->type == SHAPE_TEXT && n >= 1) {
    Vector2 sp = to_screen_coords(stroke->points[0]);
    if (dist_to_segment(sp, p0, p1) < threshold * 2.0f) return true;
  }

  if (stroke->type == SHAPE_TABLE && n >= 2) {
    Vector2 a  = to_screen_coords(stroke->points[0]);
    Vector2 b  = to_screen_coords(stroke->points[1]);
    float rx = fminf(a.x, b.x);
    float ry = fminf(a.y, b.y);
    float rw = fabsf(b.x - a.x);
    float rh = fabsf(b.y - a.y);
    Vector2 tl = { rx, ry };
    Vector2 tr = { rx + rw, ry };
    Vector2 br = { rx + rw, ry + rh };
    Vector2 bl = { rx, ry + rh };
    if (dist_segment_to_segment(p0, p1, tl, tr) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, tr, br) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, br, bl) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, bl, tl) < threshold) return true;
  }

  return false;
}

static void draw_erase(DrawLayer layer, Vector2 p0, Vector2 p1, float eraser_radius) {
  StrokeLayer* l = get_layer(layer);
  float zoom     = g_state->zoom;
  Vector2 pan    = g_state->pan;

  for (int i = l->count - 1; i >= 0; i--) {
    if (stroke_hit_test(&l->strokes[i], p0, p1, eraser_radius, zoom, pan)) {
      if (l->strokes[i].tool == TOOL_HIGHLIGHTER) {
        l->hl_dirty = true;
      } else {
        l->dirty = true;
      }
      stroke_free_data(&l->strokes[i]);
      memmove(&l->strokes[i], &l->strokes[i + 1], (l->count - i - 1) * sizeof(Stroke));
      l->count--;
    }
  }
}

// ── Draw Input Handling ─────────────────────────────────────

void handle_draw(void) {
  if (g_state->toolbox_open && toolbox_is_mouse_over()) return;
  if (g_state->keymaps_open) return;

  bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool pen_btn = g_tablet.present && (g_tablet.button1 || g_tablet.button2 || g_tablet.button3);
  bool pen_click = g_tablet.pen_just_pressed && !pen_btn && !ctrl;
  bool pen_down = g_tablet.logical_pen_down && !pen_btn && !ctrl;
  bool right_click = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || pen_click;
  bool right_held = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || pen_down;

  // Text tool click placement (STRICTLY Right-click or tablet pen)
  if (g_state->current_tool == TOOL_TEXT) {
    if (right_click) {
      Vector2 pos = get_cursor_screen_pos();
      text_commit_current();
      g_state->is_editing_text = true;
      g_state->text_edit_world_pos = to_texture_coords(pos);
      g_state->text_buffer[0] = '\0';
      g_state->text_cursor = 0;
    }
    return;
  }

  // If we were editing text and clicked elsewhere with right click, commit text
  if (g_state->is_editing_text) {
    if (right_click) {
      text_commit_current();
    }
  }

  // Step badge click placement (STRICTLY Right-click or tablet pen)
  if (g_state->current_tool == TOOL_STEP_BADGE) {
    if (right_click) {
      Vector2 pos = get_cursor_screen_pos();
      DrawLayer layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;
      stroke_begin(layer, TOOL_STEP_BADGE, SHAPE_STEP_BADGE, g_state->shape_thickness, g_state->shape_border_color);
      stroke_add_point(to_texture_coords(pos));
      stroke_end();
      if (g_state->badge_mode != BADGE_MODE_CUSTOM) {
        g_state->step_badge_counter++;
      }
    }
    return;
  }

  // Polygon point placement (STRICTLY Right-click or tablet pen)
  if (g_state->current_tool == TOOL_POLYGON) {
    if (right_click) {
      Vector2 spos = get_cursor_screen_pos();
      Vector2 wpos = to_texture_coords(spos);

      static double s_last_poly_click = 0;
      double now = GetTime();
      bool double_click = (now - s_last_poly_click < 0.35);
      s_last_poly_click = now;

      if (!g_state->poly_active) {
        g_state->poly_active = true;
        g_state->poly_pts_count = 0;
        if (g_state->poly_pts_capacity < 8) {
          g_state->poly_pts_capacity = 8;
          g_state->poly_pts = realloc(g_state->poly_pts, sizeof(Vector2) * g_state->poly_pts_capacity);
        }
        g_state->poly_pts[g_state->poly_pts_count++] = wpos;
      } else {
        Vector2 p0_screen = to_screen_coords(g_state->poly_pts[0]);
        bool near_p0 = (g_state->poly_pts_count >= 3 && Vector2Distance(spos, p0_screen) < 15.0f);
        if (near_p0 || (double_click && g_state->poly_pts_count >= 3)) {
          polygon_commit();
          return;
        }

        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
          Vector2 prev = g_state->poly_pts[g_state->poly_pts_count - 1];
          float dx = wpos.x - prev.x;
          float dy = wpos.y - prev.y;
          float dist = sqrtf(dx * dx + dy * dy);
          if (dist > 0.001f) {
            float ang = snap_angle_15(atan2f(dy, dx));
            wpos.x = prev.x + cosf(ang) * dist;
            wpos.y = prev.y + sinf(ang) * dist;
          }
        }

        if (g_state->poly_pts_count >= g_state->poly_pts_capacity) {
          g_state->poly_pts_capacity *= 2;
          g_state->poly_pts = realloc(g_state->poly_pts, sizeof(Vector2) * g_state->poly_pts_capacity);
        }
        g_state->poly_pts[g_state->poly_pts_count++] = wpos;
      }
    }
    return;
  }

  // Regular drawing (Pen, Eraser, Highlighter, Line, Arrow, Ngon, Circle, Table)
  // Left-click is STRICTLY PAN ONLY! Drawing is STRICTLY Right-click or tablet pen!
  bool should_draw = right_held;

  Vector2 pos   = get_cursor_screen_pos();
  DrawLayer layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;

  if (should_draw) {
    if (!g_state->is_drawing) {
      g_state->is_drawing = true;
      s_last_pos          = pos;
      s_has_last_pos      = true;

      if (g_state->current_tool == TOOL_ERASER) {
        draw_erase(layer, pos, pos, g_state->tool_eraser_size);
      } else {
        float size = g_state->tool_pen_size;
        Color stroke_color = g_configuration->draw_color;
        if (g_state->current_tool == TOOL_HIGHLIGHTER) {
          size = g_state->tool_highlighter_size;
        } else if (g_state->current_tool >= TOOL_LINE && g_state->current_tool <= TOOL_TABLE) {
          size = g_state->shape_thickness;
          stroke_color = g_state->shape_border_color;
        }

        ShapeType shape = SHAPE_FREEHAND;
        if (g_state->current_tool == TOOL_LINE) shape = SHAPE_LINE;
        else if (g_state->current_tool == TOOL_ARROW) shape = SHAPE_ARROW;
        else if (g_state->current_tool == TOOL_NGON) shape = SHAPE_NGON;
        else if (g_state->current_tool == TOOL_CIRCLE) shape = SHAPE_CIRCLE;
        else if (g_state->current_tool == TOOL_TABLE) shape = SHAPE_TABLE;

        stroke_begin(layer, g_state->current_tool, shape, size, stroke_color);
        stroke_add_point(to_texture_coords(pos));
        if (shape != SHAPE_FREEHAND) {
          // Add second point for shape live preview
          stroke_add_point(to_texture_coords(pos));
        }
      }
    } else {
      if (g_state->current_tool == TOOL_ERASER) {
        Vector2 p_prev = s_has_last_pos ? s_last_pos : pos;
        draw_erase(layer, p_prev, pos, g_state->tool_eraser_size);
      } else if (s_active_stroke) {
        if (s_active_stroke->type == SHAPE_FREEHAND) {
          stroke_add_point(to_texture_coords(pos));
        } else {
          // Live drag preview for shapes
          Vector2 world_pos = to_texture_coords(pos);
          if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            // Shift constraint with 15-degree universal snapping
            Vector2 p0 = s_active_stroke->points[0];
            if (s_active_stroke->type == SHAPE_TABLE) {
              float dx = world_pos.x - p0.x;
              float dy = world_pos.y - p0.y;
              float sz = fmaxf(fabsf(dx), fabsf(dy));
              world_pos.x = p0.x + (dx >= 0 ? sz : -sz);
              world_pos.y = p0.y + (dy >= 0 ? sz : -sz);
            } else if (s_active_stroke->type == SHAPE_LINE ||
                       s_active_stroke->type == SHAPE_ARROW ||
                       s_active_stroke->type == SHAPE_NGON) {
              float dx = world_pos.x - p0.x;
              float dy = world_pos.y - p0.y;
              float dist = sqrtf(dx * dx + dy * dy);
              if (dist > 0.001f) {
                float snap = snap_angle_15(atan2f(dy, dx));
                world_pos.x = p0.x + cosf(snap) * dist;
                world_pos.y = p0.y + sinf(snap) * dist;
              }
            }
          }
          if (s_active_stroke->points_count >= 2) {
            s_active_stroke->points[1] = world_pos;
          }
          // Ngon arrow keys while dragging
          if (s_active_stroke->type == SHAPE_NGON) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_RIGHT)) {
              if (g_state->ngon_sides < 16) g_state->ngon_sides++;
              s_active_stroke->ngon_sides = g_state->ngon_sides;
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_LEFT)) {
              if (g_state->ngon_sides > 3) g_state->ngon_sides--;
              s_active_stroke->ngon_sides = g_state->ngon_sides;
            }
          }
          // Table arrow keys while dragging
          if (s_active_stroke->type == SHAPE_TABLE) {
            if (IsKeyPressed(KEY_UP)) { g_state->table_rows++; s_active_stroke->table_rows = g_state->table_rows; }
            if (IsKeyPressed(KEY_DOWN) && g_state->table_rows > 1) { g_state->table_rows--; s_active_stroke->table_rows = g_state->table_rows; }
            if (IsKeyPressed(KEY_RIGHT)) { g_state->table_cols++; s_active_stroke->table_cols = g_state->table_cols; }
            if (IsKeyPressed(KEY_LEFT) && g_state->table_cols > 1) { g_state->table_cols--; s_active_stroke->table_cols = g_state->table_cols; }
          }
          StrokeLayer* l = get_active_layer();
          l->dirty = true;
        }
      }
      s_last_pos     = pos;
      s_has_last_pos = true;
    }
  } else {
    if (g_state->is_drawing) {
      stroke_end();
      g_state->is_drawing = false;
    }
  }
}

// ── Compatibility Wrappers ──────────────────────────────────

void lines_draw(void) { draw_layer_normal(LAYER_IMAGE); }
bool is_lines_dirty(void) { return draw_is_dirty(LAYER_IMAGE); }
bool is_bb_lines_dirty(void) { return draw_is_dirty(LAYER_BLACKBOARD); }
void clear_lines_dirty(void) { draw_clear_dirty(LAYER_IMAGE); }
void clear_bb_lines_dirty(void) { draw_clear_dirty(LAYER_BLACKBOARD); }
void lines_clear(void) { draw_clear_layer(LAYER_IMAGE); }
void lines_erase_at(Vector2 screen_pos) { draw_erase(LAYER_IMAGE, screen_pos, screen_pos, g_state->tool_eraser_size); }
void bb_lines_clear(void) { draw_clear_layer(LAYER_BLACKBOARD); }
void bb_lines_erase_at(Vector2 screen_pos) { draw_erase(LAYER_BLACKBOARD, screen_pos, screen_pos, g_state->tool_eraser_size); }
void bb_lines_draw(void) { draw_layer_normal(LAYER_BLACKBOARD); }
void hl_clear(void) { draw_clear_layer(LAYER_IMAGE); }
void bb_hl_clear(void) { draw_clear_layer(LAYER_BLACKBOARD); }
void hl_lines_clear(void) { draw_clear_all(); }
void hl_lines_erase_at(Vector2 screen_pos) {
  draw_erase(LAYER_IMAGE, screen_pos, screen_pos, g_state->tool_eraser_size);
  draw_erase(LAYER_BLACKBOARD, screen_pos, screen_pos, g_state->tool_eraser_size);
}
bool hl_render_rt(void) {
  draw_render_highlighter(g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE, true);
  return true;
}
void hl_composite(void) { draw_composite_highlighter(); }
void hl_lines_draw(void) {
  draw_render_highlighter(g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE, true);
  draw_composite_highlighter();
}
