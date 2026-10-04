#include "roomer.h"

static bool   s_flashlight_manual    = false;
static double s_size_indicator_until = 0;

static void handle_reset(void);
static void handle_fit(void);
static void handle_panning(void);
static void handle_zoom(void);
static void handle_tablet_zoom(void);
static void handle_flashlight(void);
static void handle_toolbox(void);

static void handle_text_input(void) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    text_cancel_current();
    return;
  }

  bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

  if (ctrl && IsKeyPressed(KEY_C)) {
    text_cancel_current();
    return;
  }

  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
    if (shift) {
      int len = (int)strlen(g_state->text_buffer);
      if (len < (int)sizeof(g_state->text_buffer) - 2) {
        g_state->text_buffer[len] = '\n';
        g_state->text_buffer[len + 1] = '\0';
        g_state->text_cursor = len + 1;
      }
    } else {
      text_commit_current();
      return;
    }
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    int len = (int)strlen(g_state->text_buffer);
    if (len > 0) {
      g_state->text_buffer[len - 1] = '\0';
      g_state->text_cursor = len - 1;
    }
  }

  int ch = GetCharPressed();
  while (ch > 0) {
    int len = (int)strlen(g_state->text_buffer);
    if (len < (int)sizeof(g_state->text_buffer) - 2) {
      g_state->text_buffer[len] = (char)ch;
      g_state->text_buffer[len + 1] = '\0';
      g_state->text_cursor = len + 1;
    }
    ch = GetCharPressed();
  }
}

void handle_inputs(void) {
  tablet_poll();

  if (g_state->is_editing_text) {
    handle_text_input();
    handle_draw();
    return;
  }

  if (g_state->is_editing_badge_text) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
      g_state->is_editing_badge_text = false;
      return;
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
      int len = (int)strlen(g_state->badge_custom_text);
      if (len > 0) g_state->badge_custom_text[len - 1] = '\0';
      return;
    }
    int ch = GetCharPressed();
    while (ch > 0) {
      int len = (int)strlen(g_state->badge_custom_text);
      if (len < 15 && ch >= 32 && ch <= 126) {
        g_state->badge_custom_text[len] = (char)ch;
        g_state->badge_custom_text[len + 1] = '\0';
      }
      ch = GetCharPressed();
    }
    return;
  }

  // Super + H toggles hide overlay
  bool super = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
  if (super && IsKeyPressed(KEY_H)) {
    toggle_hide_overlay();
    return;
  }

  // Polygon active keyboard interactions
  if (g_state->poly_active) {
    if (IsKeyPressed(KEY_ESCAPE)) {
      polygon_cancel();
      return;
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
      polygon_pop_last_point();
      return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
      polygon_commit();
      return;
    }
  }

  handle_toolbox();
  if (g_state->toolbox_open) toolbox_handle_input();

  if (!super && IsKeyPressed(KEY_H)) {
    g_state->keymaps_open = !g_state->keymaps_open;
    if (g_state->keymaps_open) g_state->toolbox_open = false;
  }
  if (g_state->keymaps_open && IsKeyPressed(KEY_ESCAPE)) {
    g_state->keymaps_open = false;
    return;
  }

  if (g_state->toolbox_open && IsKeyPressed(KEY_ESCAPE)) {
    g_state->toolbox_open = false;
    return;
  }

  // Delete key resets all variables to initial values
  if (!g_state->is_editing_text && !g_state->is_editing_badge_text && IsKeyPressed(KEY_DELETE)) {
    reset_all_variables(true);
    return;
  }

  if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
    reset_all_variables(true);
    g_state->should_quit = true;
    return;
  }

  handle_reset();
  handle_fit();
  handle_panning();
  handle_zoom();
  handle_tablet_zoom();
  handle_flashlight();
  handle_draw();
}

void reset_all_variables(bool clear_drawings) {
  int w = g_state->image_w;
  int h = g_state->image_h;
  polygon_cancel();
  text_cancel_current();
  if (clear_drawings) {
    draw_clear_all();
  }
  *g_state = g_initial_state;
  g_state->image_w = w;
  g_state->image_h = h;
  if (w > 0 && h > 0) {
    float fw = (float)GetScreenWidth();
    float fh = (float)GetScreenHeight();
    g_state->target_pan.x = (fw - (float)w * g_state->target_zoom) / 2.0f;
    g_state->target_pan.y = (fh - (float)h * g_state->target_zoom) / 2.0f;
    g_state->pan = g_state->target_pan;
  }
  g_configuration->draw_color = g_state->color1;
  hud_tooltip_show(clear_drawings ? "Reset All (Initial State)" : "Variables Reinitialized");
}

static void handle_reset(void) {
  bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  if (IsKeyPressed(KEY_DELETE) || (shift && IsKeyPressed(KEY_ZERO)) || (ctrl && IsKeyPressed(KEY_R))) {
    reset_all_variables(true);
  }
}

static void handle_fit(void) {
  if (IsKeyPressed(KEY_A) && g_state->image_w > 0 && g_state->image_h > 0) {
    float fw              = (float)GetScreenWidth();
    float fh              = (float)GetScreenHeight();
    g_state->target_zoom  = fminf(fw / (float)g_state->image_w, fh / (float)g_state->image_h);
    g_state->target_pan.x = (fw - (float)g_state->image_w * g_state->target_zoom) / 2;
    g_state->target_pan.y = (fh - (float)g_state->image_h * g_state->target_zoom) / 2;
  }
}

static Vector2 s_last_tablet_pos     = { 0 };
static bool    s_has_last_tablet_pos = false;

static void handle_panning(void) {
  if (g_state->is_drawing) return;

  bool mouse_pan = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool pen_button = g_tablet.present && (g_tablet.button1 || g_tablet.button2 || g_tablet.button3);

  // Block mouse pan if pen is touching without barrel button (pen draws, doesn't pan)
  if (mouse_pan && g_tablet.present && g_tablet.touching && !pen_button) return;

  // If not pressing pen button, don't pan when over toolbox
  if (!pen_button && g_state->toolbox_open && toolbox_is_mouse_over()) return;

  if (!mouse_pan && !pen_button) {
    s_has_last_tablet_pos = false;
    return;
  }

  if (pen_button) {
    Vector2 cur_pos = get_cursor_screen_pos();
    if (s_has_last_tablet_pos) {
      Vector2 delta = { cur_pos.x - s_last_tablet_pos.x, cur_pos.y - s_last_tablet_pos.y };
      g_state->target_pan.x += delta.x;
      g_state->target_pan.y += delta.y;
    }
    s_last_tablet_pos = cur_pos;
    s_has_last_tablet_pos = true;
  } else if (mouse_pan) {
    Vector2 mouse_delta    = GetMouseDelta();
    g_state->target_pan.x += mouse_delta.x;
    g_state->target_pan.y += mouse_delta.y;
  }
}

static void handle_zoom(void) {
  float wheel          = GetMouseWheelMove();
  bool  ctrl           = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  float keyboard_delta = 0;
  if (ctrl && IsKeyPressed(KEY_EQUAL)) keyboard_delta = 0.5F;
  if (ctrl && IsKeyPressed(KEY_MINUS)) keyboard_delta = -0.5F;
  if (ctrl && IsKeyPressed(KEY_KP_ADD)) keyboard_delta = 0.5F;
  if (ctrl && IsKeyPressed(KEY_KP_SUBTRACT)) keyboard_delta = -0.5F;

  float delta = (wheel != 0) ? wheel * g_configuration->zoom_step : keyboard_delta;
  if (delta != 0 && !g_state->is_drawing && !g_state->flashlight_enabled) {
    Vector2 anchor = (wheel != 0) ? GetMousePosition() : (Vector2){ (float)GetScreenWidth() / 2.0F, (float)GetScreenHeight() / 2.0F };
    float   prev_zoom     = g_state->zoom;
    Vector2 world         = { (anchor.x - g_state->pan.x) / prev_zoom, (anchor.y - g_state->pan.y) / prev_zoom };
    float   mult          = (wheel != 0 && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) ? 0.33F : 1.0F;
    g_state->target_zoom  = Clamp(g_state->target_zoom + delta * mult, g_configuration->zoom_min, g_configuration->zoom_max);
    g_state->target_pan.x = anchor.x - world.x * g_state->target_zoom;
    g_state->target_pan.y = anchor.y - world.y * g_state->target_zoom;
  }
}

// ── zoom via tablet (anchor-distance) ──────────────────────

static bool    s_tab_zoom_active   = false;
static float   s_tab_zoom_ref_zoom = 1.0F;
static Vector2 s_tab_zoom_ref_pan  = { 0 };
static float   s_tab_zoom_ref_dist = 0.0F;

static void handle_tablet_zoom(void) {
  if (!g_tablet.present) return;

  bool ctrl    = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool zoom_on = ctrl && (g_tablet.touching || g_tablet.button1 || g_tablet.button2 || g_tablet.button3);

  if (zoom_on) {
    if (!s_tab_zoom_active) {
      s_tab_zoom_active   = true;
      s_tab_zoom_ref_zoom = g_state->zoom;
      s_tab_zoom_ref_pan  = g_state->pan;
      Vector2 center      = { GetScreenWidth() / 2.0F, GetScreenHeight() / 2.0F };
      s_tab_zoom_ref_dist = Vector2Distance(GetMousePosition(), center);
      if (s_tab_zoom_ref_dist < 1.0F) s_tab_zoom_ref_dist = 1.0F;
    } else {
      Vector2 center        = { GetScreenWidth() / 2.0F, GetScreenHeight() / 2.0F };
      float   dist          = Vector2Distance(GetMousePosition(), center);
      g_state->target_zoom  = Clamp(s_tab_zoom_ref_zoom * (dist / s_tab_zoom_ref_dist), g_configuration->zoom_min, g_configuration->zoom_max);
      Vector2 world_center  = { (center.x - s_tab_zoom_ref_pan.x) / s_tab_zoom_ref_zoom, (center.y - s_tab_zoom_ref_pan.y) / s_tab_zoom_ref_zoom };
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

static float* current_tool_size_ptr(void) {
  if (g_state->current_tool == TOOL_ERASER) return &g_state->tool_eraser_size;
  if (g_state->current_tool == TOOL_HIGHLIGHTER) return &g_state->tool_highlighter_size;
  return &g_state->tool_pen_size;
}

static float current_tool_size_min(void) {
  if (g_state->current_tool == TOOL_HIGHLIGHTER) return 10.0F;
  if (g_state->current_tool == TOOL_ERASER) return 6.0F;
  return 0.5F;
}

static float current_tool_size_max(void) {
  if (g_state->current_tool == TOOL_PEN) return 8.0F;
  if (g_state->current_tool == TOOL_HIGHLIGHTER) return 36.0F;
  if (g_state->current_tool == TOOL_ERASER) return 36.0F;
  return 36.0F;
}

static void handle_size_keys(void) {
  bool   ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  float  step = 0.3F;
  float* size = current_tool_size_ptr();
  float  s_min = current_tool_size_min();
  float  s_max = current_tool_size_max();

  static double s_last_repeat = 0;
  bool          plus          = !ctrl && (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD));
  bool          minus         = !ctrl && (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT));
  if (!plus && !minus) {
    s_last_repeat          = 0;
    s_size_indicator_until = 0;
    return;
  }

  double now   = GetTime();
  double delay = (s_last_repeat == 0) ? 0.0 : 0.06;
  if (now - s_last_repeat < delay) return;
  s_last_repeat = now;

  if (plus) *size = fminf(*size + step, s_max);
  if (minus) *size = fmaxf(*size - step, s_min);
  toolbox_sync_size();
  s_size_indicator_until = now + 1.5;
}

void draw_size_indicator(void) {
  if (g_state->keymaps_open) return;
  if (g_state->toolbox_open && toolbox_is_mouse_over()) return;

  Vector2 m = get_cursor_screen_pos();
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  if (m.x < 0 || m.x > (float)sw || m.y < 0 || m.y > (float)sh) return;

  if (g_state->current_tool == TOOL_TEXT) {
    float font_size = g_state->text_font_size;
    DrawLineEx((Vector2){ m.x - 4, m.y }, (Vector2){ m.x + 4, m.y }, 1.5f, WHITE);
    DrawLineEx((Vector2){ m.x, m.y }, (Vector2){ m.x, m.y + font_size }, 2.0f, WHITE);
    DrawLineEx((Vector2){ m.x - 4, m.y + font_size }, (Vector2){ m.x + 4, m.y + font_size }, 1.5f, WHITE);
    return;
  }

  if (g_state->current_tool == TOOL_STEP_BADGE) {
    float badge_r = g_state->badge_size;
    Color c = g_state->shape_border_color;
    char num_str[32];
    badge_step_number_to_string(g_state->step_badge_counter, g_state->badge_mode, g_state->badge_custom_text, num_str, sizeof(num_str));
    Font font = get_app_font();
    float font_size = badge_r * 1.15f;
    Vector2 text_dim = MeasureTextEx(font, num_str, font_size, 1.0f);

    float diam = badge_r * 2.0f;
    float pad_x = badge_r * 0.55f;
    float badge_w = fmaxf(diam, text_dim.x + pad_x * 2.0f);
    float badge_h = diam;

    if (badge_w <= diam + 0.1f) {
      DrawCircleV(m, badge_r, (Color){ c.r, c.g, c.b, 60 });
      DrawCircleLinesV(m, badge_r, c);
    } else {
      Rectangle rec = { m.x - badge_w * 0.5f, m.y - badge_h * 0.5f, badge_w, badge_h };
      DrawRectangleRounded(rec, 0.5f, 16, (Color){ c.r, c.g, c.b, 60 });
      DrawRectangleRoundedLinesEx(rec, 0.5f, 16, 2.0f, c);
    }

    Vector2 text_pos = { m.x - text_dim.x * 0.5f, m.y - text_dim.y * 0.5f };
    DrawTextEx(font, num_str, text_pos, font_size, 1.0f, c);
    return;
  }

  if (g_state->current_tool == TOOL_POLYGON && !g_state->poly_active) {
    Color c = g_state->shape_border_color;
    DrawCircleLines((int)m.x, (int)m.y, 6.0f, c);
    DrawLine((int)m.x - 10, (int)m.y, (int)m.x + 10, (int)m.y, c);
    DrawLine((int)m.x, (int)m.y - 10, (int)m.x, (int)m.y + 10, c);
    return;
  }

  float sz = *current_tool_size_ptr();
  if (g_state->current_tool >= TOOL_LINE && g_state->current_tool <= TOOL_TABLE) {
    sz = g_state->shape_thickness;
  }
  bool adjusting = (GetTime() <= s_size_indicator_until);

  if (g_state->current_tool == TOOL_ERASER) {
    DrawCircleV(m, sz, (Color){ 255, 255, 255, adjusting ? 45 : 20 });
    DrawCircleLinesV(m, sz + 1.0f, (Color){ 0, 0, 0, 140 });
    DrawCircleLinesV(m, sz, (Color){ 240, 240, 240, 220 });
    if (sz > 3.0f) {
      DrawCircleLinesV(m, sz - 1.0f, (Color){ 0, 0, 0, 60 });
    }
  } else {
    Color c = g_configuration->draw_color;
    if (g_state->current_tool >= TOOL_LINE && g_state->current_tool <= TOOL_TABLE) {
      c = g_state->shape_border_color;
    }
    unsigned char alpha_fill = (g_state->current_tool == TOOL_HIGHLIGHTER) ? 40 : 25;
    if (adjusting) alpha_fill += 30;

    DrawCircleV(m, sz, (Color){ c.r, c.g, c.b, alpha_fill });
    DrawCircleLinesV(m, sz + 1.0f, (Color){ 0, 0, 0, 130 });
    DrawCircleLinesV(m, sz, (Color){ c.r, c.g, c.b, 230 });
  }

  if (adjusting) {
    float ch = fminf(sz * 0.5f, 8.0f);
    if (ch < 4.0f) ch = 4.0f;
    DrawLineV((Vector2){ m.x - ch, m.y }, (Vector2){ m.x + ch, m.y }, (Color){ 255, 255, 255, 200 });
    DrawLineV((Vector2){ m.x, m.y - ch }, (Vector2){ m.x, m.y + ch }, (Color){ 255, 255, 255, 200 });
  }
}

// ── HUD Tooltip Notification ────────────────────────────────

static char   s_hud_text[64] = { 0 };
static double s_hud_time     = -10.0;

void hud_tooltip_show(const char* text) {
  if (!text) return;
  strncpy(s_hud_text, text, sizeof(s_hud_text) - 1);
  s_hud_text[sizeof(s_hud_text) - 1] = '\0';
  s_hud_time = GetTime();
}

void tool_notify_current(void) {
  char buf[64];
  const char* style_str = (g_state->shape_stroke_style == STYLE_DASHED) ? "Dashed" :
                          (g_state->shape_stroke_style == STYLE_DOTTED) ? "Dotted" : "Solid";
  switch (g_state->current_tool) {
    case TOOL_PEN:
      snprintf(buf, sizeof(buf), "Pen");
      break;
    case TOOL_HIGHLIGHTER:
      snprintf(buf, sizeof(buf), "Highlighter");
      break;
    case TOOL_ERASER:
      snprintf(buf, sizeof(buf), "Eraser");
      break;
    case TOOL_LINE:
      snprintf(buf, sizeof(buf), "Straight Line (%s)", style_str);
      break;
    case TOOL_ARROW:
      snprintf(buf, sizeof(buf), "Arrow (%s)", style_str);
      break;
    case TOOL_POLYGON:
      snprintf(buf, sizeof(buf), "Polygon Tool (%s)", style_str);
      break;
    case TOOL_NGON:
      snprintf(buf, sizeof(buf), "%d-Gon Tool (%s)", g_state->ngon_sides, style_str);
      break;
    case TOOL_CIRCLE:
      snprintf(buf, sizeof(buf), "Circle (%s)", style_str);
      break;
    case TOOL_TABLE:
      snprintf(buf, sizeof(buf), "Table Tool %dx%d (%s)", g_state->table_rows, g_state->table_cols, style_str);
      break;
    case TOOL_STEP_BADGE:
      if (g_state->badge_mode == BADGE_MODE_NUMERIC)
        snprintf(buf, sizeof(buf), "Step Badge (#%d)", g_state->step_badge_counter);
      else if (g_state->badge_mode == BADGE_MODE_ALPHA_UPPER)
        snprintf(buf, sizeof(buf), "Step Badge (A..Z)");
      else if (g_state->badge_mode == BADGE_MODE_ALPHA_LOWER)
        snprintf(buf, sizeof(buf), "Step Badge (a..z)");
      else
        snprintf(buf, sizeof(buf), "Step Badge (Custom)");
      break;
    case TOOL_TEXT:
      snprintf(buf, sizeof(buf), "Text Tool");
      break;
    default:
      snprintf(buf, sizeof(buf), "Tool Selected");
      break;
  }
  hud_tooltip_show(buf);
}

void hud_tooltip_render(void) {
  double elapsed = GetTime() - s_hud_time;
  if (elapsed < 0.0 || elapsed > 1.4) return;

  float alpha = 1.0f;
  if (elapsed > 0.95) {
    alpha = (float)(1.0 - (elapsed - 0.95) / 0.45);
    if (alpha < 0.0f) alpha = 0.0f;
  }

  Font font = get_app_font();
  float font_size = 18.0f;
  Vector2 text_dim = MeasureTextEx(font, s_hud_text, font_size, 1.0f);

  float pad_x = 18.0f;
  float pad_y = 9.0f;
  float box_w = text_dim.x + pad_x * 2.0f;
  float box_h = text_dim.y + pad_y * 2.0f;
  float sw = (float)GetScreenWidth();
  float box_x = (sw - box_w) * 0.5f;
  float box_y = 28.0f;

  Rectangle rec = { box_x, box_y, box_w, box_h };
  Color bg_col     = { 22, 22, 28, (unsigned char)(235.0f * alpha) };
  Color border_col = { 80, 145, 230, (unsigned char)(210.0f * alpha) };
  Color text_col   = { 250, 250, 250, (unsigned char)(255.0f * alpha) };

  DrawRectangleRounded(rec, 0.45f, 16, bg_col);
  DrawRectangleRoundedLinesEx(rec, 0.45f, 16, 1.5f, border_col);

  Vector2 text_pos = { box_x + pad_x, box_y + pad_y };
  DrawTextEx(font, s_hud_text, text_pos, font_size, 1.0f, text_col);
}

static void handle_toolbox(void) {
  bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

  // Ctrl+F toggles fill on last shape or default fill mode
  if (ctrl && IsKeyPressed(KEY_F)) {
    stroke_toggle_fill_last();
    hud_tooltip_show(g_state->shape_filled ? "Fill: ON" : "Fill: OFF");
    return;
  }

  // Bracket keys adjust dash spacing
  if (IsKeyPressed(KEY_LEFT_BRACKET)) {
    g_state->shape_dash_len = fmaxf(4.0f, g_state->shape_dash_len - 2.0f);
    g_state->shape_dash_gap = fmaxf(2.0f, g_state->shape_dash_gap - 1.0f);
    char dbuf[32];
    snprintf(dbuf, sizeof(dbuf), "Dash Spacing: %.0fpx", g_state->shape_dash_gap);
    hud_tooltip_show(dbuf);
  }
  if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
    g_state->shape_dash_len = fminf(40.0f, g_state->shape_dash_len + 2.0f);
    g_state->shape_dash_gap = fminf(30.0f, g_state->shape_dash_gap + 1.0f);
    char dbuf[32];
    snprintf(dbuf, sizeof(dbuf), "Dash Spacing: %.0fpx", g_state->shape_dash_gap);
    hud_tooltip_show(dbuf);
  }

  if (IsKeyPressed(KEY_C)) {
    g_state->keymaps_open = false;
    toolbox_toggle();
  }
  if (IsKeyPressed(KEY_X)) {
    g_state->active_swatch      = !g_state->active_swatch;
    g_configuration->draw_color = g_state->active_swatch ? g_state->color2 : g_state->color1;
    hud_tooltip_show(g_state->active_swatch ? "Color 2 Active" : "Color 1 Active");
  }
  if (IsKeyPressed(KEY_B)) {
    g_state->black_board_enabled = !g_state->black_board_enabled;
    hud_tooltip_show(g_state->black_board_enabled ? "Blackboard: ON" : "Blackboard: OFF");
  }

  // Minus key pops step badge if step badge tool is active
  if (!ctrl && IsKeyPressed(KEY_MINUS) && g_state->current_tool == TOOL_STEP_BADGE) {
    step_badge_pop_last();
    hud_tooltip_show("Popped Badge");
  }

  // Tool 1: Pen / Highlighter (Shift+1)
  if (IsKeyPressed(KEY_ONE)) {
    polygon_cancel();
    if (shift) {
      g_state->current_tool = TOOL_HIGHLIGHTER;
    } else {
      g_state->current_tool = TOOL_PEN;
      if (g_state->tool_pen_size > 8.0F) g_state->tool_pen_size = 8.0F;
    }
    tool_notify_current();
  }

  // Tool 2: Eraser
  if (IsKeyPressed(KEY_TWO)) {
    polygon_cancel();
    g_state->current_tool = TOOL_ERASER;
    tool_notify_current();
  }

  // Tool 3: Straight Line
  if (IsKeyPressed(KEY_THREE)) {
    if (g_state->current_tool == TOOL_LINE) {
      g_state->shape_stroke_style = (g_state->shape_stroke_style + 1) % 3;
    } else {
      polygon_cancel();
      g_state->current_tool = TOOL_LINE;
      g_state->shape_stroke_style = shift ? STYLE_DASHED : STYLE_SOLID;
    }
    tool_notify_current();
  }

  // Tool 4: Arrow
  if (IsKeyPressed(KEY_FOUR)) {
    if (g_state->current_tool == TOOL_ARROW) {
      g_state->shape_stroke_style = (g_state->shape_stroke_style + 1) % 3;
    } else {
      polygon_cancel();
      g_state->current_tool = TOOL_ARROW;
      g_state->shape_stroke_style = shift ? STYLE_DASHED : STYLE_SOLID;
    }
    tool_notify_current();
  }

  // Tool 5: Polygon
  if (IsKeyPressed(KEY_FIVE)) {
    if (g_state->current_tool == TOOL_POLYGON) {
      g_state->shape_stroke_style = (g_state->shape_stroke_style + 1) % 3;
    } else {
      polygon_cancel();
      g_state->current_tool = TOOL_POLYGON;
      g_state->shape_stroke_style = shift ? STYLE_DASHED : STYLE_SOLID;
    }
    tool_notify_current();
  }

  // Tool 6: N-Gon
  if (IsKeyPressed(KEY_SIX)) {
    if (g_state->current_tool == TOOL_NGON) {
      g_state->shape_stroke_style = (g_state->shape_stroke_style + 1) % 3;
    } else {
      polygon_cancel();
      g_state->current_tool = TOOL_NGON;
      g_state->shape_stroke_style = shift ? STYLE_DASHED : STYLE_SOLID;
    }
    tool_notify_current();
  }

  // Tool 7: Circle
  if (IsKeyPressed(KEY_SEVEN)) {
    if (g_state->current_tool == TOOL_CIRCLE) {
      g_state->shape_stroke_style = (g_state->shape_stroke_style + 1) % 3;
    } else {
      polygon_cancel();
      g_state->current_tool = TOOL_CIRCLE;
      g_state->shape_stroke_style = shift ? STYLE_DASHED : STYLE_SOLID;
    }
    tool_notify_current();
  }

  // Tool 8: Step Badge
  if (IsKeyPressed(KEY_EIGHT)) {
    polygon_cancel();
    g_state->current_tool = TOOL_STEP_BADGE;
    tool_notify_current();
  }

  // Tool 9: Text
  if (IsKeyPressed(KEY_NINE)) {
    polygon_cancel();
    g_state->current_tool = TOOL_TEXT;
    tool_notify_current();
  }

  // Tool 0: Table (plain 0 without shift/ctrl)
  if (!shift && !ctrl && IsKeyPressed(KEY_ZERO)) {
    if (g_state->current_tool == TOOL_TABLE) {
      g_state->shape_stroke_style = (g_state->shape_stroke_style + 1) % 3;
    } else {
      polygon_cancel();
      g_state->current_tool = TOOL_TABLE;
      g_state->shape_stroke_style = STYLE_SOLID;
    }
    tool_notify_current();
  }

  handle_size_keys();
}
