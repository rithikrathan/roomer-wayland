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
  if (stroke->text) {
    free(stroke->text);
    stroke->text = NULL;
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
  s->table_rows = g_state->table_rows;
  s->table_cols = g_state->table_cols;

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
    if (s->type == SHAPE_RECTANGLE || s->type == SHAPE_CIRCLE ||
        s->type == SHAPE_TRIANGLE  || s->type == SHAPE_TABLE ||
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
    s->thickness = g_state->tool_pen_size / g_state->zoom;
    s->color = g_configuration->draw_color;
    s->filled = g_state->shape_filled;
    Color fc = g_state->fill_color;
    fc.a = (unsigned char)(g_state->shape_fill_opacity * 255.0f);
    s->fill_color = fc;
    s->text = strdup(g_state->text_buffer);

    s->points = malloc(sizeof(Vector2));
    assert(s->points);
    s->points[0] = g_state->text_edit_world_pos;
    s->points_count = 1;
    s->points_capacity = 1;

    l->dirty = true;
  }
  g_state->is_editing_text = false;
  g_state->text_buffer[0] = '\0';
  g_state->text_cursor = 0;
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

static void draw_styled_triangle(Vector2 p0, Vector2 p1, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap, bool filled, Color fill_color) {
  Vector2 top = { (p0.x + p1.x) * 0.5f, fminf(p0.y, p1.y) };
  Vector2 bl  = { fminf(p0.x, p1.x), fmaxf(p0.y, p1.y) };
  Vector2 br  = { fmaxf(p0.x, p1.x), fmaxf(p0.y, p1.y) };

  if (filled) {
    DrawTriangle(top, bl, br, fill_color);
  }

  draw_styled_segment(top, bl, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(bl, br, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(br, top, thickness, color, style, dash_len, dash_gap);
}

static void draw_styled_rectangle(Vector2 p0, Vector2 p1, float thickness, Color color, StrokeStyle style, float dash_len, float dash_gap, bool filled, Color fill_color) {
  float rx = fminf(p0.x, p1.x);
  float ry = fminf(p0.y, p1.y);
  float rw = fabsf(p1.x - p0.x);
  float rh = fabsf(p1.y - p0.y);

  if (filled) {
    DrawRectangleRec((Rectangle){ rx, ry, rw, rh }, fill_color);
  }

  Vector2 tl = { rx, ry };
  Vector2 tr = { rx + rw, ry };
  Vector2 br = { rx + rw, ry + rh };
  Vector2 bl = { rx, ry + rh };

  draw_styled_segment(tl, tr, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(tr, br, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(br, bl, thickness, color, style, dash_len, dash_gap);
  draw_styled_segment(bl, tl, thickness, color, style, dash_len, dash_gap);
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

static void draw_step_badge(Vector2 p0, int number, float thickness, Color color, bool filled, Color fill_color) {
  (void)fill_color;
  float badge_r = fmaxf(thickness * 3.2f, 16.0f);
  Font font = get_app_font();

  if (filled) {
    DrawCircleV(p0, badge_r, color);
    DrawCircleLinesV(p0, badge_r + 1.0f, (Color){ 0, 0, 0, 180 });
  } else {
    DrawCircleV(p0, badge_r, (Color){ 10, 10, 10, 210 });
    DrawCircleLinesV(p0, badge_r + 1.0f, (Color){ 0, 0, 0, 150 });
    DrawCircleLinesV(p0, badge_r, color);
  }

  char num_str[16];
  snprintf(num_str, sizeof(num_str), "%d", number);

  float font_size = badge_r * 1.35f;
  Vector2 text_dim = MeasureTextEx(font, num_str, font_size, 1.0f);
  Vector2 text_pos = { p0.x - text_dim.x * 0.5f, p0.y - text_dim.y * 0.5f };

  Color txt_color;
  if (filled) {
    float lum = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
    txt_color = (lum > 140.0f) ? (Color){ 0, 0, 0, 255 } : (Color){ 255, 255, 255, 255 };
  } else {
    txt_color = color;
  }

  DrawTextEx(font, num_str, text_pos, font_size, 1.0f, txt_color);
}

static void draw_text_stroke(Vector2 p0, const char* text, float thickness, Color color, bool filled, Color fill_color) {
  if (!text || text[0] == '\0') return;
  Font font = get_app_font();
  float font_size = fmaxf(thickness * 5.0f, 18.0f);

  Vector2 dim = MeasureTextEx(font, text, font_size, 1.0f);
  float pad = 6.0f;

  if (filled) {
    DrawRectangleRec((Rectangle){ p0.x - pad, p0.y - pad, dim.x + pad * 2.0f, dim.y + pad * 2.0f }, fill_color);
    DrawRectangleLinesEx((Rectangle){ p0.x - pad, p0.y - pad, dim.x + pad * 2.0f, dim.y + pad * 2.0f }, 1.5f, color);
  } else {
    DrawRectangleRec((Rectangle){ p0.x - pad, p0.y - pad, dim.x + pad * 2.0f, dim.y + pad * 2.0f }, (Color){ 0, 0, 0, 100 });
  }

  DrawTextEx(font, text, p0, font_size, 1.0f, color);
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
    case SHAPE_TRIANGLE: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        draw_styled_triangle(p0, p1, screen_thickness, color, stroke->style, d_len, d_gap, stroke->filled, stroke->fill_color);
      }
      break;
    }
    case SHAPE_RECTANGLE: {
      if (n >= 2) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        Vector2 p1 = to_screen_coords(stroke->points[1]);
        draw_styled_rectangle(p0, p1, screen_thickness, color, stroke->style, d_len, d_gap, stroke->filled, stroke->fill_color);
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
        draw_step_badge(p0, stroke->step_number, screen_thickness, color, stroke->filled, stroke->fill_color);
      }
      break;
    }
    case SHAPE_TEXT: {
      if (n >= 1 && stroke->text) {
        Vector2 p0 = to_screen_coords(stroke->points[0]);
        draw_text_stroke(p0, stroke->text, screen_thickness, color, stroke->filled, stroke->fill_color);
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

  // Active text editing overlay
  if (g_state->is_editing_text) {
    DrawLayer active_layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;
    if (layer == active_layer) {
      Vector2 sp = to_screen_coords(g_state->text_edit_world_pos);
      Font font = get_app_font();
      float font_size = fmaxf(g_state->tool_pen_size * 5.0f, 18.0f);
      Color c = g_configuration->draw_color;
      Color fc = g_state->fill_color;
      fc.a = (unsigned char)(g_state->shape_fill_opacity * 255.0f);

      Vector2 dim = MeasureTextEx(font, g_state->text_buffer, font_size, 1.0f);
      float pad = 6.0f;
      float w = fmaxf(dim.x, 20.0f);
      float h = fmaxf(dim.y, font_size);

      DrawRectangleRec((Rectangle){ sp.x - pad, sp.y - pad, w + pad * 2.0f, h + pad * 2.0f },
                       g_state->shape_filled ? fc : (Color){ 0, 0, 0, 160 });
      DrawRectangleLinesEx((Rectangle){ sp.x - pad, sp.y - pad, w + pad * 2.0f, h + pad * 2.0f },
                           1.5f, (Color){ 80, 140, 220, 255 });

      DrawTextEx(font, g_state->text_buffer, sp, font_size, 1.0f, c);

      // Blinking cursor
      if (fmod(GetTime(), 0.8) < 0.4) {
        float cursor_x = sp.x + dim.x + 2.0f;
        float cursor_y = sp.y;
        DrawLineEx((Vector2){ cursor_x, cursor_y }, (Vector2){ cursor_x, cursor_y + font_size }, 2.0f, (Color){ 255, 255, 255, 240 });
      }
    }
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

  if (stroke->type == SHAPE_TRIANGLE && n >= 2) {
    Vector2 a = to_screen_coords(stroke->points[0]);
    Vector2 b = to_screen_coords(stroke->points[1]);
    Vector2 top = { (a.x + b.x) * 0.5f, fminf(a.y, b.y) };
    Vector2 bl  = { fminf(a.x, b.x), fmaxf(a.y, b.y) };
    Vector2 br  = { fmaxf(a.x, b.x), fmaxf(a.y, b.y) };
    if (dist_segment_to_segment(p0, p1, top, bl) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, bl, br) < threshold) return true;
    if (dist_segment_to_segment(p0, p1, br, top) < threshold) return true;
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
    float badge_r = fmaxf(stroke_radius * 1.6f, 16.0f);
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

  // Text tool click placement
  if (g_state->current_tool == TOOL_TEXT) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || g_tablet.pen_just_pressed) {
      Vector2 pos = get_cursor_screen_pos();
      text_commit_current();
      g_state->is_editing_text = true;
      g_state->text_edit_world_pos = to_texture_coords(pos);
      g_state->text_buffer[0] = '\0';
      g_state->text_cursor = 0;
    }
    return;
  }

  // If we were editing text and switched tool or clicked elsewhere
  if (g_state->is_editing_text) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || g_tablet.pen_just_pressed) {
      text_commit_current();
    }
  }

  // Step badge click placement
  if (g_state->current_tool == TOOL_STEP_BADGE) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || g_tablet.pen_just_pressed) {
      Vector2 pos = get_cursor_screen_pos();
      DrawLayer layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;
      stroke_begin(layer, TOOL_STEP_BADGE, SHAPE_STEP_BADGE, g_state->tool_pen_size, g_configuration->draw_color);
      stroke_add_point(to_texture_coords(pos));
      stroke_end();
      g_state->step_badge_counter++;
    }
    return;
  }

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
        else if (g_state->current_tool == TOOL_ARROW) shape = SHAPE_ARROW;
        else if (g_state->current_tool == TOOL_TRIANGLE) shape = SHAPE_TRIANGLE;
        else if (g_state->current_tool == TOOL_RECTANGLE) shape = SHAPE_RECTANGLE;
        else if (g_state->current_tool == TOOL_CIRCLE) shape = SHAPE_CIRCLE;
        else if (g_state->current_tool == TOOL_TABLE) shape = SHAPE_TABLE;

        stroke_begin(layer, g_state->current_tool, shape, size, g_configuration->draw_color);
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
            // Shift constraint
            Vector2 p0 = s_active_stroke->points[0];
            if (s_active_stroke->type == SHAPE_RECTANGLE || s_active_stroke->type == SHAPE_TABLE) {
              float dx = world_pos.x - p0.x;
              float dy = world_pos.y - p0.y;
              float sz = fmaxf(fabsf(dx), fabsf(dy));
              world_pos.x = p0.x + (dx >= 0 ? sz : -sz);
              world_pos.y = p0.y + (dy >= 0 ? sz : -sz);
            } else if (s_active_stroke->type == SHAPE_LINE || s_active_stroke->type == SHAPE_ARROW) {
              float dx = world_pos.x - p0.x;
              float dy = world_pos.y - p0.y;
              float angle = atan2f(dy, dx);
              float snap = roundf(angle / (PI * 0.25f)) * (PI * 0.25f);
              float dist = sqrtf(dx * dx + dy * dy);
              world_pos.x = p0.x + cosf(snap) * dist;
              world_pos.y = p0.y + sinf(snap) * dist;
            }
          }
          if (s_active_stroke->points_count >= 2) {
            s_active_stroke->points[1] = world_pos;
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
