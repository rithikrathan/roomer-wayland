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

static Line* redo_lines          = NULL;
static int   redo_count    = 0;
static int   redo_capacity = 0;

static void redo_clear(void) {
  for (int i = 0; i < redo_count; i++) free(redo_lines[i].points);
  free(redo_lines);
  redo_lines    = NULL;
  redo_count    = 0;
  redo_capacity = 0;
}

static void    line_begin(void);
static void    line_add_point(Vector2 texture_pos);
static Vector2 to_texture_coords(Vector2 screen_pos);
static Vector2 to_screen_coords(Vector2 texture_pos);
static float   dist_to_segment(Vector2 p, Vector2 a, Vector2 b);

void handle_draw(void) {
  bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool right_down = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
  bool pen_down   = g_tablet.touching && !g_tablet.button1 && !g_tablet.button2 && !ctrl;
  bool should_draw = right_down || pen_down;

  if (should_draw && !g_state->is_drawing) {
    if (g_state->current_tool == TOOL_ERASER) {
      lines_erase_at(GetMousePosition());
    } else {
      line_begin();
      g_state->is_drawing = true;
    }
  }

  if (g_state->is_drawing && !right_down && !pen_down) {
    g_state->is_drawing = false;
  }

  if (g_state->is_drawing) {
    line_add_point(to_texture_coords(GetMousePosition()));
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
  redo_clear();
}

void lines_undo(void) {
  if (lines_count == 0) return;
  Line* src = &lines[lines_count - 1];

  if (redo_count >= redo_capacity) {
    int   new_cap = (redo_capacity == 0) ? 4 : redo_capacity * 2;
    Line* p       = realloc(redo_lines, sizeof(Line) * new_cap);
    assert(p);
    redo_lines    = p;
    redo_capacity = new_cap;
  }
  redo_lines[redo_count] = *src;
  redo_count++;

  free(src->points);
  lines_count--;
}

void lines_redo(void) {
  if (redo_count == 0) return;
  Line* src = &redo_lines[redo_count - 1];

  if (lines_count >= lines_capacity) {
    int   new_cap = (lines_capacity == 0) ? 4 : lines_capacity * 2;
    Line* p       = realloc(lines, sizeof(Line) * new_cap);
    assert(p);
    lines          = p;
    lines_capacity = new_cap;
  }
  lines[lines_count] = *src;
  lines_count++;
  redo_count--;
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
  redo_clear();
  if (lines_count >= lines_capacity) {
    int   new_lines_capacity = (lines_capacity == 0) ? 4 : lines_capacity * 2;
    Line* new_lines          = realloc(lines, sizeof(Line) * new_lines_capacity);
    assert(new_lines);

    lines          = new_lines;
    lines_capacity = new_lines_capacity;
  }

  float base_size = (g_state->current_tool == TOOL_PEN) ? g_state->tool_pen_size : g_state->tool_eraser_size;

  if (g_state->current_tool == TOOL_PEN && g_tablet.present) {
    base_size *= (0.3F + 0.7F * g_tablet.pressure);
    if (base_size < 1.0F) base_size = 1.0F;
  }

  Line* new_line = &lines[lines_count++];
  *new_line      = (Line){
    .thickness = base_size / g_state->zoom,
    .color     = g_configuration->draw_color,
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

static Vector2 to_texture_coords(Vector2 screen_pos) {
  return Vector2Scale(Vector2Subtract(screen_pos, g_state->pan), 1.0F / g_state->zoom);
}

static Vector2 to_screen_coords(Vector2 texture_pos) {
  return Vector2Add(Vector2Scale(texture_pos, g_state->zoom), g_state->pan);
}

static float dist_to_segment(Vector2 p, Vector2 a, Vector2 b) {
  Vector2 ab = Vector2Subtract(b, a);
  Vector2 ap = Vector2Subtract(p, a);
  float   t  = Clamp(Vector2DotProduct(ap, ab) / Vector2DotProduct(ab, ab), 0.0F, 1.0F);
  Vector2 closest = Vector2Add(a, Vector2Scale(ab, t));
  return Vector2Distance(p, closest);
}
