#include "roomer.h"

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
  s->type = shape;
  s->tool = tool;
  s->points = NULL;
  s->points_count = 0;
  s->points_capacity = 0;
  s->thickness = screen_size / g_state->zoom;
  s->color = color;

  if (tool == TOOL_HIGHLIGHTER) {
    l->hl_dirty = true;
  } else {
    l->dirty = true;
  }
  s_active_stroke = s;
}

static void stroke_add_point(Vector2 texture_pos) {
  if (!s_active_stroke) return;
  if (decimate(s_active_stroke->points, s_active_stroke->points_count, texture_pos, g_state->zoom)) return;

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
  draw_clear_layer(g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE);
}

void draw_clear_all(void) {
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

// ── Stroke Rendering (Smooth Bezier & Shapes) ───────────────

static void render_stroke(const Stroke* stroke, float zoom, Vector2 pan, Color color) {
  (void)pan;
  int n = stroke->points_count;
  if (n <= 0) return;

  float screen_radius    = stroke->thickness * zoom;
  float screen_thickness = screen_radius * 2.0f;

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
        DrawLineEx(p0, p1, screen_thickness, color);
        DrawCircleV(p0, screen_radius, color);
        DrawCircleV(p1, screen_radius, color);
      }
      break;
    }
    case SHAPE_RECTANGLE: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        float rx = fminf(p0.x, p1.x);
        float ry = fminf(p0.y, p1.y);
        float rw = fabsf(p1.x - p0.x);
        float rh = fabsf(p1.y - p0.y);
        DrawRectangleLinesEx((Rectangle){ rx, ry, rw, rh }, screen_thickness, color);
      }
      break;
    }
    case SHAPE_CIRCLE: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        float r = Vector2Distance(p0, p1);
        if (screen_thickness > 1.5f) {
          DrawRing(p0, fmaxf(0.0f, r - screen_radius), r + screen_radius, 0.0f, 360.0f, 64, color);
        } else {
          DrawCircleLines((int)p0.x, (int)p0.y, r, color);
        }
      }
      break;
    }
    case SHAPE_ARROW: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        DrawLineEx(p0, p1, screen_thickness, color);
        DrawCircleV(p0, screen_radius, color);

        Vector2 dir    = Vector2Normalize(Vector2Subtract(p1, p0));
        Vector2 normal = (Vector2){ -dir.y, dir.x };
        float head_len = fmaxf(screen_thickness * 3.5f, 15.0f);
        float head_w   = head_len * 0.5f;
        Vector2 a1 = Vector2Add(Vector2Subtract(p1, Vector2Scale(dir, head_len)), Vector2Scale(normal, head_w));
        Vector2 a2 = Vector2Subtract(Vector2Subtract(p1, Vector2Scale(dir, head_len)), Vector2Scale(normal, head_w));
        DrawTriangle(p1, a2, a1, color);
      }
      break;
    }
  }
}

void draw_layer_normal(DrawLayer layer) {
  StrokeLayer* l = get_layer(layer);
  for (int i = 0; i < l->count; i++) {
    const Stroke* s = &l->strokes[i];
    if (s->tool == TOOL_HIGHLIGHTER) continue;
    render_stroke(s, g_state->zoom, g_state->pan, s->color);
  }
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

  BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
  DrawTextureRec(
    s_hl_rt.texture,
    (Rectangle){ 0, 0, (float)sw, (float)-sh },
    (Vector2){ 0, 0 },
    tint
  );
  EndBlendMode();
}

// ── Eraser Collision & Hit Testing ──────────────────────────

static inline float ccw(Vector2 a, Vector2 b, Vector2 c) {
  return (c.y - a.y) * (b.x - a.x) - (b.y - a.y) * (c.x - a.x);
}

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

  if (stroke->type == SHAPE_RECTANGLE && n >= 2) {
    Vector2 a  = to_screen_coords(stroke->points[0]);
    Vector2 b  = to_screen_coords(stroke->points[1]);
    Vector2 c1 = (Vector2){ a.x, b.y };
    Vector2 c2 = (Vector2){ b.x, a.y };
    if (dist_segment_to_segment(p0, p1, a, c1) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, c1, b) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, b, c2) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, c2, a) < threshold) return true;
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

  bool ctrl        = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool right_held  = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
  bool pen_down    = g_tablet.logical_pen_down && !g_tablet.button1 && !g_tablet.button2 && !g_tablet.button3 && !ctrl;
  bool should_draw = right_held || pen_down;

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
        float size = (g_state->current_tool == TOOL_HIGHLIGHTER)
                     ? g_state->tool_highlighter_size
                     : g_state->tool_pen_size;
        ShapeType shape = SHAPE_FREEHAND;
        if (g_state->current_tool == TOOL_LINE) shape = SHAPE_LINE;
        else if (g_state->current_tool == TOOL_RECTANGLE) shape = SHAPE_RECTANGLE;
        else if (g_state->current_tool == TOOL_CIRCLE) shape = SHAPE_CIRCLE;
        else if (g_state->current_tool == TOOL_ARROW) shape = SHAPE_ARROW;

        stroke_begin(layer, g_state->current_tool, shape, size, g_configuration->draw_color);
        stroke_add_point(to_texture_coords(pos));
      }
    } else {
      Vector2 p_prev = s_has_last_pos ? s_last_pos : pos;
      if (g_state->current_tool == TOOL_ERASER) {
        draw_erase(layer, p_prev, pos, g_state->tool_eraser_size);
      } else {
        stroke_add_point(to_texture_coords(pos));
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
