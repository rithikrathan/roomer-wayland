#include "roomer.h"

static bool s_flashlight_manual = false;

static void handle_reset(void);
static void handle_fit(void);
static void handle_panning(void);
static void handle_zoom(void);
static void handle_tablet_zoom(void);
static void handle_flashlight(void);
static void handle_toolbox(void);

void handle_inputs(void) {
  tablet_poll();
  handle_toolbox();
  if (g_state->toolbox_open) toolbox_handle_input();

  if (IsKeyPressed(KEY_H)) {
    g_state->keymaps_open = !g_state->keymaps_open;
    if (g_state->keymaps_open) g_state->toolbox_open = false;
  }
  if (g_state->keymaps_open && IsKeyPressed(KEY_ESCAPE)) g_state->keymaps_open = false;

  handle_reset();
  handle_fit();
  handle_panning();
  handle_zoom();
  handle_tablet_zoom();
  handle_flashlight();
  handle_draw();
}

static void handle_reset(void) {
  if (IsKeyPressed(KEY_ZERO)) {
    bool     saved_toolbox     = g_state->toolbox_open;
    ToolType saved_tool        = g_state->current_tool;
    float    saved_pen_size    = g_state->tool_pen_size;
    float    saved_eraser_size = g_state->tool_eraser_size;
    Color    saved_color       = g_configuration->draw_color;

    *g_state            = g_initial_state;
    s_flashlight_manual = false;
    lines_clear();
    hl_lines_clear();

    g_state->toolbox_open     = saved_toolbox;
    g_state->current_tool     = saved_tool;
    g_state->tool_pen_size    = saved_pen_size;
    g_state->tool_eraser_size = saved_eraser_size;
    g_configuration->draw_color = saved_color;
  }
}

static void handle_fit(void) {
  if (IsKeyPressed(KEY_A) && g_state->image_w > 0 && g_state->image_h > 0) {
    float fw = (float)GetScreenWidth();
    float fh = (float)GetScreenHeight();
    g_state->target_zoom = fminf(fw / (float)g_state->image_w, fh / (float)g_state->image_h);
    g_state->target_pan.x = (fw - (float)g_state->image_w * g_state->target_zoom) / 2;
    g_state->target_pan.y = (fh - (float)g_state->image_h * g_state->target_zoom) / 2;
  }
}

static void handle_panning(void) {
  if (g_state->is_drawing) return;
  if (g_state->toolbox_open && toolbox_is_mouse_over()) return;

  bool mouse_pan = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool pen_pan   = g_tablet.present && g_tablet.button1;

  // Block mouse pan while pen is touching (pen draws, doesn't pan)
  if (mouse_pan && g_tablet.present && g_tablet.touching) return;

  if (!mouse_pan && !pen_pan) return;

  Vector2 mouse_delta    = GetMouseDelta();
  g_state->target_pan.x += mouse_delta.x;
  g_state->target_pan.y += mouse_delta.y;
}

static void handle_zoom(void) {
  float wheel = GetMouseWheelMove();
  if (wheel != 0 && !g_state->is_drawing && !g_state->flashlight_enabled) {
    Vector2 mouse_pos = GetMousePosition();
    float   prev_zoom = g_state->zoom;
    Vector2 world     = { (mouse_pos.x - g_state->pan.x) / prev_zoom, (mouse_pos.y - g_state->pan.y) / prev_zoom };
    float   mult      = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 0.33F : 1.0F;
    g_state->target_zoom = Clamp(
        g_state->target_zoom + wheel * g_configuration->zoom_step * mult,
        g_configuration->zoom_min, g_configuration->zoom_max
    );
    g_state->target_pan.x = mouse_pos.x - world.x * g_state->target_zoom;
    g_state->target_pan.y = mouse_pos.y - world.y * g_state->target_zoom;
  }
}

// ── zoom via tablet (anchor-distance) ──────────────────────

static bool    s_tab_zoom_active    = false;
static float   s_tab_zoom_ref_zoom = 1.0F;
static Vector2 s_tab_zoom_ref_pan  = { 0 };
static float   s_tab_zoom_ref_dist = 0.0F;

static void handle_tablet_zoom(void) {
  if (!g_tablet.present) return;

  bool ctrl    = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool zoom_on = (ctrl && g_tablet.touching) || g_tablet.button2;

  if (zoom_on) {
    if (!s_tab_zoom_active) {
      s_tab_zoom_active    = true;
      s_tab_zoom_ref_zoom  = g_state->zoom;
      s_tab_zoom_ref_pan   = g_state->pan;
      Vector2 center = { GetScreenWidth() / 2.0F, GetScreenHeight() / 2.0F };
      s_tab_zoom_ref_dist = Vector2Distance(GetMousePosition(), center);
      if (s_tab_zoom_ref_dist < 1.0F) s_tab_zoom_ref_dist = 1.0F;
    } else {
      Vector2 center = { GetScreenWidth() / 2.0F, GetScreenHeight() / 2.0F };
      float dist = Vector2Distance(GetMousePosition(), center);
      g_state->target_zoom = Clamp(
          s_tab_zoom_ref_zoom * (dist / s_tab_zoom_ref_dist),
          g_configuration->zoom_min, g_configuration->zoom_max
      );
      Vector2 world_center = {
          (center.x - s_tab_zoom_ref_pan.x) / s_tab_zoom_ref_zoom,
          (center.y - s_tab_zoom_ref_pan.y) / s_tab_zoom_ref_zoom
      };
      g_state->target_pan.x = center.x - world_center.x * g_state->target_zoom;
      g_state->target_pan.y = center.y - world_center.y * g_state->target_zoom;
    }
  } else {
    s_tab_zoom_active = false;
  }
}

// ── flashlight ──────────────────────────────────────────────

static void handle_flashlight(void) {
  if (IsKeyPressed(KEY_F)) { s_flashlight_manual = !s_flashlight_manual; }

  g_state->flashlight_enabled = s_flashlight_manual;

  float mouse_wheel_delta = GetMouseWheelMove();
  if (g_state->flashlight_enabled && mouse_wheel_delta != 0) {
    g_state->target_flashlight_radius -= mouse_wheel_delta * g_configuration->flashlight_radius_step;
    g_state->target_flashlight_radius =
        Clamp(g_state->target_flashlight_radius, g_configuration->flashlight_radius_min, g_configuration->flashlight_radius_max);
  }
}

static void handle_toolbox(void) {
  if (IsKeyPressed(KEY_C)) { g_state->keymaps_open = false; toolbox_toggle(); }
  if (IsKeyPressed(KEY_X)) {
    Color tmp = g_configuration->draw_color;
    g_configuration->draw_color = g_state->color2;
    g_state->color2 = tmp;
  }
  if (IsKeyPressed(KEY_ONE))   { g_state->current_tool = TOOL_PEN; if (g_state->tool_pen_size > 10.0F) g_state->tool_pen_size = 10.0F; }
  if (IsKeyPressed(KEY_TWO))   g_state->current_tool = TOOL_ERASER;
  if (IsKeyPressed(KEY_THREE)) g_state->current_tool = TOOL_HIGHLIGHTER;
  if (IsKeyPressed(KEY_B))     g_state->black_board_enabled = !g_state->black_board_enabled;
}
