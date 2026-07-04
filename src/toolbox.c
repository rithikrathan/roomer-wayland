#include "roomer.h"
#include "pen_png.h"
#include "eraser_png.h"
#include "highlighter_png.h"
#include "trash_png.h"
#include "fit_png.h"
#include "font_ttf.h"

#define BOX_W      210
#define BOX_PAD    8
#define ROW_H      38
#define ROW_GAP    4
#define BTN_W      88
#define ICON_SZ    28
#define FONT_SZ    26
#define COLOR_SZ   44
#define GAP_MD     8
static Vector2 s_popup_pos = { 0 };

// ── total popup height ──────────────────────────────────────

static float box_height(void) {
  return BOX_PAD + 6 * ROW_H + 5 * ROW_GAP + BOX_PAD;
}

// ── row y offsets ───────────────────────────────────────────

static float row_y(int row) {
  return s_popup_pos.y + BOX_PAD + row * (ROW_H + ROW_GAP);
}

static float btn_x_left(void)  { return s_popup_pos.x + BOX_PAD; }
static float btn_x_right(void) { return s_popup_pos.x + BOX_PAD + BTN_W + GAP_MD; }

// ── lazy-loaded assets ──────────────────────────────────────

static Texture2D pen_tex   = { 0 };
static Texture2D eras_tex  = { 0 };
static Texture2D hl_tex    = { 0 };
static Texture2D trash_tex = { 0 };
static Texture2D fit_tex   = { 0 };
static Font      tool_font = { 0 };
static bool      assets_loaded = false;

static void load_assets(void) {
  if (assets_loaded) return;
  assets_loaded = true;

  Image img = LoadImageFromMemory(".png", assets_pen_white_png, (int)assets_pen_white_png_len);
  if (img.data != NULL) {
    ImageResize(&img, ICON_SZ, ICON_SZ);
    pen_tex = LoadTextureFromImage(img);
    UnloadImage(img);
  }

  img = LoadImageFromMemory(".png", assets_eraser_white_png, (int)assets_eraser_white_png_len);
  if (img.data != NULL) {
    ImageResize(&img, ICON_SZ, ICON_SZ);
    eras_tex = LoadTextureFromImage(img);
    UnloadImage(img);
  }

  img = LoadImageFromMemory(".png", assets_highlighter_white_png, (int)assets_highlighter_white_png_len);
  if (img.data != NULL) {
    ImageResize(&img, ICON_SZ, ICON_SZ);
    hl_tex = LoadTextureFromImage(img);
    UnloadImage(img);
  }

  img = LoadImageFromMemory(".png", assets_trash_white_png, (int)assets_trash_white_png_len);
  if (img.data != NULL) {
    ImageResize(&img, ICON_SZ, ICON_SZ);
    trash_tex = LoadTextureFromImage(img);
    UnloadImage(img);
  }

  img = LoadImageFromMemory(".png", assets_fit_white_png, (int)assets_fit_white_png_len);
  if (img.data != NULL) {
    ImageResize(&img, ICON_SZ, ICON_SZ);
    fit_tex = LoadTextureFromImage(img);
    UnloadImage(img);
  }

  tool_font = LoadFontFromMemory(".ttf", assets_InconsolataLGCNerdFont_Regular_ttf,
                                 (int)assets_InconsolataLGCNerdFont_Regular_ttf_len,
                                 FONT_SZ, NULL, 0);
  if (tool_font.texture.id == 0) tool_font = GetFontDefault();
}

static void txt(float x, float y, const char* s, Color c) {
  DrawTextEx(tool_font, s, (Vector2){ x, y }, FONT_SZ, 1, c);
}

static void draw_tool_button(float x, float y, const char* label, Texture2D tex, bool active_left, bool active_right) {
  bool active = active_left || active_right;
  Color bg = active ? (Color){ 50, 115, 210, 255 } : (Color){ 40, 40, 40, 220 };
  DrawRectangle(x, y, BTN_W, ROW_H, bg);
  if (tex.id > 0) {
    float ix = x + (BTN_W - ICON_SZ) / 2;
    float iy = y + (ROW_H - ICON_SZ) / 2;
    DrawTexture(tex, (int)ix, (int)iy, WHITE);
  } else {
    float tx = x + (BTN_W - MeasureTextEx(tool_font, label, FONT_SZ, 1).x) / 2;
    float ty = y + (ROW_H - FONT_SZ) / 2;
    txt(tx, ty, label, WHITE);
  }
}

// ── tooltip state ──────────────────────────────────────────

typedef struct {
  Rectangle rect;
  double    hover_start;
  bool      hovering;
  const char* label;
} TooltipState;

static TooltipState s_tip = { 0 };

static void update_tooltip(const char* label, Rectangle btn_rect) {
  Vector2 m = GetMousePosition();
  bool hit = CheckCollisionPointRec(m, btn_rect);
  if (!hit) {
    s_tip.hovering = false;
    return;
  }
  if (s_tip.hovering && s_tip.label == label) return;
  s_tip.hovering    = true;
  s_tip.hover_start = GetTime();
  s_tip.label       = label;
  s_tip.rect        = btn_rect;
}

static void draw_tooltip_if_hovering(void) {
  if (!s_tip.hovering) return;
  if (GetTime() - s_tip.hover_start < 0.5) return;

  Font f = tool_font;
  Vector2 ms = MeasureTextEx(f, s_tip.label, FONT_SZ, 1);
  float tw = ms.x + 12;
  float th = ms.y + 6;
  float tx = s_tip.rect.x + (s_tip.rect.width - tw) / 2;
  float ty = s_popup_pos.y + box_height() + 4;

  DrawRectangle((int)tx, (int)ty, (int)tw, (int)th, (Color){ 30, 30, 30, 230 });
  DrawRectangleLines((int)tx, (int)ty, (int)tw, (int)th, (Color){ 120, 120, 120, 255 });
  DrawTextEx(f, s_tip.label, (Vector2){ tx + 6, ty + 3 }, FONT_SZ, 1, WHITE);
}

// ── public API ─────────────────────────────────────────────

void toolbox_toggle(void) {
  g_state->toolbox_open = !g_state->toolbox_open;
  if (g_state->toolbox_open) {
    Vector2 m = GetMousePosition();
    float bw = BOX_W;
    float bh = box_height();
    s_popup_pos.x = m.x - bw / 2;
    s_popup_pos.y = m.y - bh - 10;
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    if (s_popup_pos.x < 4) s_popup_pos.x = 4;
    if (s_popup_pos.x + bw > sw - 4) s_popup_pos.x = sw - 4 - bw;
    if (s_popup_pos.y < 4) s_popup_pos.y = m.y + 10;
    if (s_popup_pos.y + bh > sh - 4) s_popup_pos.y = sh - 4 - bh;
  }
}

bool toolbox_is_open(void) {
  return g_state->toolbox_open;
}

bool toolbox_is_mouse_over(void) {
  if (!g_state->toolbox_open) return false;
  return CheckCollisionPointRec(GetMousePosition(), (Rectangle){ s_popup_pos.x, s_popup_pos.y, BOX_W, box_height() });
}

// ── slider / edit state ─────────────────────────────────────
#define SLIDER_H   6
#define THUMB_R    7
#define SLIDER_MIN 1.0F

static bool  s_dragging_slider = false;
static bool  s_editing_size    = false;
static char  s_edit_buf[8]    = { 0 };
static int   s_edit_len        = 0;
static double s_edit_start     = 0;
static bool  s_dragging_zoom   = false;

static float zoom_to_slider(float z) {
  if (z <= 1.0F) return (z - g_configuration->zoom_min) / (1.0F - g_configuration->zoom_min) - 1.0F;
  return (z - 1.0F) / (g_configuration->zoom_max - 1.0F);
}

static float slider_to_zoom(float s) {
  if (s <= 0) return g_configuration->zoom_min + (1.0F - g_configuration->zoom_min) * (s + 1.0F);
  return 1.0F + (g_configuration->zoom_max - 1.0F) * s;
}

static float slider_max(void) {
  return (g_state->current_tool == TOOL_ERASER) ? 60.0F : 30.0F;
}

static float current_size(void) {
  return (g_state->current_tool == TOOL_ERASER) ? g_state->tool_eraser_size : g_state->tool_pen_size;
}

static void set_size(float v) {
  v = Clamp(v, SLIDER_MIN, slider_max());
  if (g_state->current_tool == TOOL_ERASER) g_state->tool_eraser_size = v;
  else g_state->tool_pen_size = v;
}

void toolbox_handle_input(void) {
  if (!g_state->toolbox_open) return;
  load_assets();

  Vector2 m = GetMousePosition();
  bool in_box = CheckCollisionPointRec(m, (Rectangle){ s_popup_pos.x, s_popup_pos.y, BOX_W, box_height() });

  // ── editing mode ──────────────────────────────────────────
  if (s_editing_size) {
    int c = GetCharPressed();
    while (c > 0) {
      if ((c >= '0' && c <= '9') || (c == '.' && !memchr(s_edit_buf, '.', s_edit_len))) {
        if (s_edit_len < (int)sizeof(s_edit_buf) - 1) {
          s_edit_buf[s_edit_len++] = (char)c;
          s_edit_buf[s_edit_len]   = 0;
        }
      }
      c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && s_edit_len > 0) {
      s_edit_buf[--s_edit_len] = 0;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
      if (s_edit_len > 0) {
        float v = (float)atof(s_edit_buf);
        if (v > 0) set_size(v);
      }
      s_editing_size = false;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      s_editing_size = false;
    }
    if (!in_box && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      // commit on click outside
      if (s_edit_len > 0) {
        float v = (float)atof(s_edit_buf);
        if (v > 0) set_size(v);
      }
      s_editing_size = false;
    }
    return;  // consume all input while editing
  }

  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_tablet.pen_just_pressed && !s_dragging_slider && !s_dragging_zoom) return;

  // ── size slider drag ──────────────────────────────────────
  if (s_dragging_slider) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      float lab_w = MeasureTextEx(tool_font, "Size", FONT_SZ, 1).x + 4;
      float val_x = s_popup_pos.x + BOX_PAD + lab_w;
      char szbuf[16];
      snprintf(szbuf, sizeof(szbuf), "%.1f", current_size());
      float val_w = MeasureTextEx(tool_font, szbuf, FONT_SZ, 1).x + 8;
      float sl_x  = val_x + val_w + 4;
      float sl_w  = s_popup_pos.x + BOX_W - BOX_PAD - sl_x;
      if (sl_w < 20) sl_w = 20;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0F, 1.0F);
      set_size(SLIDER_MIN + t * (slider_max() - SLIDER_MIN));
      return;
    }
    s_dragging_slider = false;
    return;
  }

  // ── zoom slider drag ──────────────────────────────────────
  if (s_dragging_zoom) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      float lab_w = MeasureTextEx(tool_font, "Zoom", FONT_SZ, 1).x + 4;
      float val_x = s_popup_pos.x + BOX_PAD + lab_w;
      float val_w = 36;
      float sl_x  = val_x + val_w + 4;
      float sl_w  = s_popup_pos.x + BOX_W - BOX_PAD - sl_x;
      if (sl_w < 20) sl_w = 20;
      float s = -1.0F + Clamp((m.x - sl_x) / sl_w, 0.0F, 1.0F) * 2.0F;
      g_state->target_zoom = Clamp(slider_to_zoom(s), g_configuration->zoom_min, g_configuration->zoom_max);
      return;
    }
    s_dragging_zoom = false;
    return;
  }

  if (!in_box) return;

  float xl = btn_x_left();
  float xr = btn_x_right();

  // Row 0: [Pen] [Eraser]
  if (CheckCollisionPointRec(m, (Rectangle){ xl, row_y(0), BTN_W, ROW_H })) { g_state->current_tool = TOOL_PEN; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ xr, row_y(0), BTN_W, ROW_H })) { g_state->current_tool = TOOL_ERASER; return; }

  // Row 1: [Highlighter] [Colour Picker]
  if (CheckCollisionPointRec(m, (Rectangle){ xl, row_y(1), BTN_W, ROW_H })) { g_state->current_tool = TOOL_HIGHLIGHTER; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ xr, row_y(1), BTN_W, ROW_H })) { g_state->current_tool = TOOL_COLOURPICKER; return; }

  // Row 2: Size label, editable value, slider
  float cy2 = row_y(2);
  float lab_w = MeasureTextEx(tool_font, "Size", FONT_SZ, 1).x + 4;
  float val_x = s_popup_pos.x + BOX_PAD + lab_w;
  char szbuf[16];
  snprintf(szbuf, sizeof(szbuf), "%.1f", current_size());
  float val_w = MeasureTextEx(tool_font, szbuf, FONT_SZ, 1).x + 8;

  // Click on value → start editing
  if (CheckCollisionPointRec(m, (Rectangle){ val_x, cy2, val_w, ROW_H })) {
    s_editing_size = true;
    s_edit_len     = snprintf(s_edit_buf, sizeof(s_edit_buf), "%.1f", current_size());
    s_edit_start   = GetTime();
    return;
  }

  // Click/drag on slider
  float sl_x = val_x + val_w + 4;
  float sl_w = s_popup_pos.x + BOX_W - BOX_PAD - sl_x;
  if (sl_w < 20) sl_w = 20;
  if (CheckCollisionPointRec(m, (Rectangle){ sl_x, cy2, sl_w, ROW_H })) {
    float t = Clamp((m.x - sl_x) / sl_w, 0.0F, 1.0F);
    set_size(SLIDER_MIN + t * (slider_max() - SLIDER_MIN));
    s_dragging_slider = true;
    return;
  }

  // Row 3: Color cells
  float cy3 = row_y(3);
  float cc_w = COLOR_SZ;
  float cc_gap = GAP_MD;
  float cc_total = cc_w * 2 + cc_gap;
  float cc_x = s_popup_pos.x + (BOX_W - cc_total) / 2;

  if (CheckCollisionPointRec(m, (Rectangle){ cc_x, cy3, cc_w, ROW_H })) {
    g_configuration->draw_color = open_color_picker(g_configuration->draw_color);
    return;
  }
  if (CheckCollisionPointRec(m, (Rectangle){ cc_x + cc_w + cc_gap, cy3, cc_w, ROW_H })) {
    g_state->color2 = open_color_picker(g_state->color2);
    return;
  }

  // Row 4: Zoom slider
  {
    float cy4 = row_y(4);
    float lab_w = MeasureTextEx(tool_font, "Zoom", FONT_SZ, 1).x + 4;
    float val_x = s_popup_pos.x + BOX_PAD + lab_w;
    float val_w = 36;
    char zbuf[16];
    snprintf(zbuf, sizeof(zbuf), "%.1f", g_state->zoom);
    float zv_w = MeasureTextEx(tool_font, zbuf, FONT_SZ, 1).x + 8;
    if (val_w < zv_w) val_w = zv_w;
    float sl_x = val_x + val_w + 4;
    float sl_w = s_popup_pos.x + BOX_W - BOX_PAD - sl_x;
    if (sl_w < 20) sl_w = 20;

    // Click on slider
    if (CheckCollisionPointRec(m, (Rectangle){ sl_x, cy4, sl_w, ROW_H })) {
      float s = -1.0F + Clamp((m.x - sl_x) / sl_w, 0.0F, 1.0F) * 2.0F;
      g_state->target_zoom = Clamp(slider_to_zoom(s), g_configuration->zoom_min, g_configuration->zoom_max);
      s_dragging_zoom = true;
      return;
    }
  }

  // Row 5: [Clear] [Fit]
  float cy5 = row_y(5);
  if (CheckCollisionPointRec(m, (Rectangle){ xl, cy5, BTN_W, ROW_H })) {
    hl_lines_clear();
    if (g_state->black_board_enabled) bb_lines_clear();
    else lines_clear();
    return;
  }
  if (CheckCollisionPointRec(m, (Rectangle){ xr, cy5, BTN_W, ROW_H })) {
    if (g_state->image_w > 0 && g_state->image_h > 0) {
      float fw = (float)GetScreenWidth();
      float fh = (float)GetScreenHeight();
      g_state->target_zoom = fminf(fw / (float)g_state->image_w, fh / (float)g_state->image_h);
      g_state->target_pan.x = (fw - (float)g_state->image_w * g_state->target_zoom) / 2;
      g_state->target_pan.y = (fh - (float)g_state->image_h * g_state->target_zoom) / 2;
    }
    return;
  }
}

void toolbox_render(void) {
  if (!g_state->toolbox_open) return;
  load_assets();

  float bx = s_popup_pos.x;
  float by = s_popup_pos.y;
  float bh = box_height();

  DrawRectangle(bx, by, BOX_W, bh, (Color){ 20, 20, 20, 220 });
  DrawRectangleLines(bx, by, BOX_W, bh, (Color){ 60, 60, 60, 255 });

  float xl = btn_x_left();
  float xr = btn_x_right();
  ToolType cur = g_state->current_tool;

  // Row 0: [Pen] [Eraser]
  draw_tool_button(xl, row_y(0), "Pen", pen_tex, cur == TOOL_PEN, false);
  update_tooltip("Pen (1)", (Rectangle){ xl, row_y(0), BTN_W, ROW_H });
  draw_tool_button(xr, row_y(0), "Eraser", eras_tex, cur == TOOL_ERASER, false);
  update_tooltip("Eraser (2)", (Rectangle){ xr, row_y(0), BTN_W, ROW_H });

  // Row 1: [Highlighter] [Colour Picker]
  draw_tool_button(xl, row_y(1), "Highlight", hl_tex, cur == TOOL_HIGHLIGHTER, false);
  update_tooltip("Highlighter (3)", (Rectangle){ xl, row_y(1), BTN_W, ROW_H });
  draw_tool_button(xr, row_y(1), "Pick", (Texture2D){ 0 }, cur == TOOL_COLOURPICKER, false);
  update_tooltip("Colour picker", (Rectangle){ xr, row_y(1), BTN_W, ROW_H });

  // Row 2: Size label + editable value + slider
  float cy2 = row_y(2);
  float txt_y = cy2 + (ROW_H - FONT_SZ) / 2;
  float cx = s_popup_pos.x + BOX_PAD;

  txt(cx, txt_y, "Size", (Color){ 180, 180, 180, 255 });
  float lab_w = MeasureTextEx(tool_font, "Size", FONT_SZ, 1).x + 4;
  cx += lab_w;

  float sz = current_size();
  char szbuf[16];
  snprintf(szbuf, sizeof(szbuf), "%.1f", sz);
  const char* val_str = s_editing_size ? s_edit_buf : szbuf;
  Color val_col = s_editing_size ? (Color){ 255, 255, 100, 255 } : WHITE;
  txt(cx, txt_y, val_str, val_col);
  float val_w = MeasureTextEx(tool_font, val_str, FONT_SZ, 1).x + 8;

  // Cursor blink when editing
  if (s_editing_size && fmod(GetTime() - s_edit_start, 1.0) < 0.5) {
    float cur_x = cx + MeasureTextEx(tool_font, val_str, FONT_SZ, 1).x;
    DrawRectangle((int)cur_x, (int)(cy2 + 6), 2, (int)(ROW_H - 12), (Color){ 255, 255, 100, 255 });
  }

  cx += val_w + 4;

  // Slider track
  float sl_w = s_popup_pos.x + BOX_W - BOX_PAD - cx;
  if (sl_w < 20) sl_w = 20;
  float track_y = cy2 + (ROW_H - SLIDER_H) / 2;
  DrawRectangle(cx, track_y, sl_w, SLIDER_H, (Color){ 60, 60, 60, 255 });
  // Filled portion
  float t = (sz - SLIDER_MIN) / (slider_max() - SLIDER_MIN);
  if (t > 0)
    DrawRectangle(cx, track_y, (int)(t * sl_w), SLIDER_H, (Color){ 80, 140, 220, 255 });

  // Thumb
  float thumb_x = cx + t * sl_w;
  DrawCircleV((Vector2){ thumb_x, track_y + SLIDER_H / 2.0F }, THUMB_R, (Color){ 200, 200, 200, 255 });
  DrawCircleV((Vector2){ thumb_x, track_y + SLIDER_H / 2.0F }, THUMB_R - 2, (Color){ 240, 240, 240, 255 });

  update_tooltip("Drag to adjust size", (Rectangle){ cx, cy2, sl_w, ROW_H });

  // Row 3: Color cells
  float cy3 = row_y(3);
  float cc_w = COLOR_SZ;
  float cc_gap = GAP_MD;
  float cc_total = cc_w * 2 + cc_gap;
  float cc_x = s_popup_pos.x + (BOX_W - cc_total) / 2;
  float cc_y_off = cy3 + (ROW_H - cc_w) / 2;

  DrawRectangle(cc_x, cc_y_off, cc_w, cc_w, g_configuration->draw_color);
  DrawRectangleLines(cc_x, cc_y_off, cc_w, cc_w, WHITE);
  update_tooltip("Color 1 (active)", (Rectangle){ cc_x, cy3, cc_w, ROW_H });

  float c2x = cc_x + cc_w + cc_gap;
  DrawRectangle(c2x, cc_y_off, cc_w, cc_w, g_state->color2);
  DrawRectangleLines(c2x, cc_y_off, cc_w, cc_w, (Color){ 80, 80, 80, 255 });
  update_tooltip("Color 2 (swap with X)", (Rectangle){ c2x, cy3, cc_w, ROW_H });

  // Row 4: Zoom slider
  {
    float cy4 = row_y(4);
    float txt_y4 = cy4 + (ROW_H - FONT_SZ) / 2;
    float zx = s_popup_pos.x + BOX_PAD;
    txt(zx, txt_y4, "Zoom", (Color){ 180, 180, 180, 255 });
    zx += MeasureTextEx(tool_font, "Zoom", FONT_SZ, 1).x + 4;

    char zbuf[16];
    snprintf(zbuf, sizeof(zbuf), "%.1f", g_state->zoom);
    float zv_w = MeasureTextEx(tool_font, zbuf, FONT_SZ, 1).x + 8;
    txt(zx, txt_y4, zbuf, WHITE);
    zx += zv_w + 4;

    float zsl_w = s_popup_pos.x + BOX_W - BOX_PAD - zx;
    if (zsl_w < 20) zsl_w = 20;
    float track_y4 = cy4 + (ROW_H - SLIDER_H) / 2;
    DrawRectangle(zx, track_y4, zsl_w, SLIDER_H, (Color){ 60, 60, 60, 255 });

    float zt = (zoom_to_slider(g_state->zoom) + 1.0F) / 2.0F;
    zt = Clamp(zt, 0.0F, 1.0F);
    if (zt > 0)
      DrawRectangle(zx, track_y4, (int)(zt * zsl_w), SLIDER_H, (Color){ 80, 140, 220, 255 });

    float zthumb_x = zx + zt * zsl_w;
    DrawCircleV((Vector2){ zthumb_x, track_y4 + SLIDER_H / 2.0F }, THUMB_R, (Color){ 200, 200, 200, 255 });
    DrawCircleV((Vector2){ zthumb_x, track_y4 + SLIDER_H / 2.0F }, THUMB_R - 2, (Color){ 240, 240, 240, 255 });
    update_tooltip("Zoom (drag to adjust)", (Rectangle){ zx, cy4, zsl_w, ROW_H });
  }

  // Row 5: [Clear] [Fit]
  float cy5 = row_y(5);
  draw_tool_button(xl, cy5, "Clear", trash_tex, false, false);
  update_tooltip("Clear strokes (active layer)", (Rectangle){ xl, cy5, BTN_W, ROW_H });
  draw_tool_button(xr, cy5, "Fit", fit_tex, false, false);
  update_tooltip("Fit image to screen", (Rectangle){ xr, cy5, BTN_W, ROW_H });

  draw_tooltip_if_hovering();
}
