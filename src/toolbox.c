#include "roomer.h"
#include "pen_png.h"
#include "eraser_png.h"
#include "highlighter_png.h"
#include "trash_png.h"
#include "fit_png.h"
#include "font_ttf.h"

#define BOX_W      270
#define BOX_PAD    10
#define ROW_H      34
#define ROW_GAP    6
#define COLS_4_W   58
#define GAP_4      6
#define ICON_SZ    20
#define FONT_SZ    18
#define COLOR_SZ   38
#define ROW_COUNT  8

static Vector2 s_popup_pos = { 0 };

static float box_height(void) {
  return BOX_PAD + ROW_COUNT * ROW_H + (ROW_COUNT - 1) * ROW_GAP + BOX_PAD;
}

static float row_y(int row) {
  return s_popup_pos.y + BOX_PAD + row * (ROW_H + ROW_GAP);
}

static float col_x(int col) {
  return s_popup_pos.x + BOX_PAD + col * (COLS_4_W + GAP_4);
}

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

Font get_app_font(void) {
  load_assets();
  return tool_font;
}

static void txt(float x, float y, const char* s, Color c) {
  DrawTextEx(tool_font, s, (Vector2){ (int)x, (int)y }, FONT_SZ, 1, c);
}

static void draw_btn(float x, float y, float w, float h, const char* label, Texture2D tex, bool active) {
  Color bg = active ? (Color){ 50, 115, 210, 255 } : (Color){ 38, 38, 38, 225 };
  DrawRectangle((int)x, (int)y, (int)w, (int)h, bg);
  DrawRectangleLines((int)x, (int)y, (int)w, (int)h, active ? (Color){ 100, 170, 255, 255 } : (Color){ 65, 65, 65, 220 });

  if (tex.id > 0) {
    float ix = x + (w - ICON_SZ) / 2;
    float iy = y + (h - ICON_SZ) / 2;
    DrawTexture(tex, (int)ix, (int)iy, WHITE);
  } else {
    Vector2 sz = MeasureTextEx(tool_font, label, FONT_SZ, 1);
    float tx = x + (w - sz.x) / 2;
    float ty = y + (h - sz.y) / 2;
    DrawTextEx(tool_font, label, (Vector2){ (int)tx, (int)ty }, FONT_SZ, 1, WHITE);
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
    if (s_tip.hovering && s_tip.label == label) s_tip.hovering = false;
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
  if (GetTime() - s_tip.hover_start < 0.4) return;

  Font f = tool_font;
  Vector2 ms = MeasureTextEx(f, s_tip.label, FONT_SZ, 1);
  float tw = ms.x + 12;
  float th = ms.y + 6;
  float tx = s_tip.rect.x + (s_tip.rect.width - tw) / 2;
  float ty = s_popup_pos.y + box_height() + 4;

  float sw = (float)GetScreenWidth();
  if (tx < 4) tx = 4;
  if (tx + tw > sw - 4) tx = sw - 4 - tw;

  DrawRectangle((int)tx, (int)ty, (int)tw, (int)th, (Color){ 25, 25, 25, 235 });
  DrawRectangleLines((int)tx, (int)ty, (int)tw, (int)th, (Color){ 110, 110, 110, 255 });
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
#define THUMB_R    6

static bool   s_dragging_slider = false;
static bool   s_editing_size    = false;
static char   s_edit_buf[8]     = { 0 };
static int    s_edit_len        = 0;
static double s_edit_start      = 0;
static bool   s_dragging_zoom   = false;

static float zoom_to_slider(float z) {
  if (z <= 1.0F) {
    float u = cbrtf((z - g_configuration->zoom_min) / (1.0F - g_configuration->zoom_min));
    return u - 1.0F;
  }
  float u = cbrtf((z - 1.0F) / (g_configuration->zoom_max - 1.0F));
  return u;
}

static float slider_to_zoom(float s) {
  if (s <= 0) {
    float u = s + 1.0F;
    return g_configuration->zoom_min + u * u * u * (1.0F - g_configuration->zoom_min);
  }
  float u = s;
  return 1.0F + u * u * u * (g_configuration->zoom_max - 1.0F);
}

static float slider_min(void) {
  if (g_state->current_tool == TOOL_HIGHLIGHTER) return 10.0F;
  if (g_state->current_tool == TOOL_ERASER) return 5.0F;
  return 0.5F;
}

static float slider_max(void) {
  if (g_state->current_tool == TOOL_PEN) return 10.0F;
  return 60.0F;
}

static float current_size(void) {
  if (g_state->current_tool == TOOL_ERASER) return g_state->tool_eraser_size;
  if (g_state->current_tool == TOOL_HIGHLIGHTER) return g_state->tool_highlighter_size;
  return g_state->tool_pen_size;
}

static void set_size(float v) {
  v = Clamp(v, slider_min(), slider_max());
  if (g_state->current_tool == TOOL_ERASER) g_state->tool_eraser_size = v;
  else if (g_state->current_tool == TOOL_HIGHLIGHTER) g_state->tool_highlighter_size = v;
  else g_state->tool_pen_size = v;
}

static float s_size_mult = -1.0F;

static float mult_to_size(float mult) {
  return slider_min() + mult * (slider_max() - slider_min());
}

static float size_to_mult(float size) {
  float range = slider_max() - slider_min();
  if (range < 0.001f) return 0.0f;
  return (size - slider_min()) / range;
}

void toolbox_handle_input(void) {
  if (!g_state->toolbox_open) return;
  load_assets();

  if (s_size_mult < 0) s_size_mult = size_to_mult(current_size());

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
        if (v > 0) { set_size(v); s_size_mult = size_to_mult(v); }
      }
      s_editing_size = false;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      s_editing_size = false;
    }
    if (!in_box && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (s_edit_len > 0) {
        float v = (float)atof(s_edit_buf);
        if (v > 0) { set_size(v); s_size_mult = size_to_mult(v); }
      }
      s_editing_size = false;
    }
    return;
  }

  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_tablet.pen_just_pressed && !s_dragging_slider && !s_dragging_zoom) return;

  // ── size slider drag ──────────────────────────────────────
  if (s_dragging_slider) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || g_tablet.logical_pen_down) {
      char szbuf[16];
      snprintf(szbuf, sizeof(szbuf), "%.1f", mult_to_size(s_size_mult));
      float val_w = MeasureTextEx(tool_font, szbuf, FONT_SZ, 1).x + 8;
      float sl_x  = s_popup_pos.x + BOX_PAD;
      float sl_w  = BOX_W - BOX_PAD * 2 - val_w - 4;
      if (sl_w < 20) sl_w = 20;
      s_size_mult = Clamp((m.x - sl_x) / sl_w, 0.0F, 1.0F);
      set_size(mult_to_size(s_size_mult));
      return;
    }
    s_dragging_slider = false;
    return;
  }

  // ── zoom slider drag ──────────────────────────────────────
  if (s_dragging_zoom) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || g_tablet.logical_pen_down) {
      char zbuf[16];
      snprintf(zbuf, sizeof(zbuf), "%.1f", g_state->zoom);
      float val_w = MeasureTextEx(tool_font, zbuf, FONT_SZ, 1).x + 8;
      float sl_x  = s_popup_pos.x + BOX_PAD;
      float sl_w  = BOX_W - BOX_PAD * 2 - val_w - 4;
      if (sl_w < 20) sl_w = 20;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0F, 1.0F);
      float s = -1.0F + t * 2.0F;
      g_state->target_zoom = Clamp(slider_to_zoom(s), g_configuration->zoom_min, g_configuration->zoom_max);
      return;
    }
    s_dragging_zoom = false;
    return;
  }

  if (!in_box) return;

  // Row 0: [Pen] [Eraser] [HL] [Line]
  float r0 = row_y(0);
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r0, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_PEN; if (g_state->tool_pen_size > 10.0F) g_state->tool_pen_size = 10.0F; set_size(mult_to_size(s_size_mult)); return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r0, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_ERASER; set_size(mult_to_size(s_size_mult)); return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r0, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_HIGHLIGHTER; set_size(mult_to_size(s_size_mult)); return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r0, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_LINE; return; }

  // Row 1: [Arrow] [Triangle] [Rectangle] [Circle]
  float r1 = row_y(1);
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_ARROW; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_TRIANGLE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_RECTANGLE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_CIRCLE; return; }

  // Row 2: [Step Badge] [Text] [Table] [Blackboard]
  float r2 = row_y(2);
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r2, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_STEP_BADGE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r2, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_TEXT; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r2, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_TABLE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r2, COLS_4_W, ROW_H })) { g_state->black_board_enabled = !g_state->black_board_enabled; return; }

  // Row 3: Shape Settings [Solid] [Dashed] [Dotted] [Fill]
  float r3 = row_y(3);
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r3, COLS_4_W, ROW_H })) { g_state->shape_stroke_style = STYLE_SOLID; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r3, COLS_4_W, ROW_H })) { g_state->shape_stroke_style = STYLE_DASHED; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r3, COLS_4_W, ROW_H })) { g_state->shape_stroke_style = STYLE_DOTTED; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r3, COLS_4_W, ROW_H })) { stroke_toggle_fill_last(); return; }

  // Row 4: Size slider + editable value
  float r4 = row_y(4);
  char szbuf[16];
  snprintf(szbuf, sizeof(szbuf), "%.1f", mult_to_size(s_size_mult));
  float val_w = MeasureTextEx(tool_font, szbuf, FONT_SZ, 1).x + 8;
  float val_x = s_popup_pos.x + BOX_W - BOX_PAD - val_w;

  if (CheckCollisionPointRec(m, (Rectangle){ val_x, r4, val_w, ROW_H })) {
    s_editing_size = true;
    s_edit_len     = snprintf(s_edit_buf, sizeof(s_edit_buf), "%.1f", mult_to_size(s_size_mult));
    s_edit_start   = GetTime();
    return;
  }

  float sl_x = s_popup_pos.x + BOX_PAD;
  float sl_w = BOX_W - BOX_PAD * 2 - val_w - 4;
  if (sl_w < 20) sl_w = 20;
  if (CheckCollisionPointRec(m, (Rectangle){ sl_x, r4, sl_w, ROW_H })) {
    s_size_mult = Clamp((m.x - sl_x) / sl_w, 0.0F, 1.0F);
    set_size(mult_to_size(s_size_mult));
    s_dragging_slider = true;
    return;
  }

  // Row 5: Colors + swap
  float r5 = row_y(5);
  float cc_w   = COLOR_SZ;
  float sw_sz  = 24;
  float sw_gap = 10;
  float cc_tot = cc_w + sw_gap + sw_sz + sw_gap + cc_w;
  float cc_x   = s_popup_pos.x + (BOX_W - cc_tot) / 2;
  float c2_x   = cc_x + cc_w + sw_gap + sw_sz + sw_gap;
  float swap_x = cc_x + cc_w + sw_gap;

  if (CheckCollisionPointRec(m, (Rectangle){ cc_x, r5, cc_w, ROW_H })) {
    g_state->color1 = open_color_picker(g_state->color1);
    g_state->active_swatch = 0;
    g_configuration->draw_color = g_state->color1;
    return;
  }
  if (CheckCollisionPointRec(m, (Rectangle){ swap_x, r5, sw_sz, ROW_H })) {
    g_state->active_swatch = !g_state->active_swatch;
    g_configuration->draw_color = g_state->active_swatch ? g_state->color2 : g_state->color1;
    return;
  }
  if (CheckCollisionPointRec(m, (Rectangle){ c2_x, r5, cc_w, ROW_H })) {
    g_state->color2 = open_color_picker(g_state->color2);
    g_state->active_swatch = 1;
    g_configuration->draw_color = g_state->color2;
    return;
  }

  // Row 6: Zoom slider
  float r6 = row_y(6);
  char zbuf[16];
  snprintf(zbuf, sizeof(zbuf), "%.1f", g_state->zoom);
  float zval_w = MeasureTextEx(tool_font, zbuf, FONT_SZ, 1).x + 8;
  float zsl_x  = s_popup_pos.x + BOX_PAD;
  float zsl_w  = BOX_W - BOX_PAD * 2 - zval_w - 4;
  if (zsl_w < 20) zsl_w = 20;
  if (CheckCollisionPointRec(m, (Rectangle){ zsl_x, r6, zsl_w, ROW_H })) {
    float t = Clamp((m.x - zsl_x) / zsl_w, 0.0F, 1.0F);
    float s = -1.0F + t * 2.0F;
    g_state->target_zoom = Clamp(slider_to_zoom(s), g_configuration->zoom_min, g_configuration->zoom_max);
    s_dragging_zoom = true;
    return;
  }

  // Row 7: [Clear] [Fit]
  float r7 = row_y(7);
  float half_w = (BOX_W - BOX_PAD * 2 - GAP_4) / 2;
  if (CheckCollisionPointRec(m, (Rectangle){ s_popup_pos.x + BOX_PAD, r7, half_w, ROW_H })) {
    draw_clear_current();
    return;
  }
  if (CheckCollisionPointRec(m, (Rectangle){ s_popup_pos.x + BOX_PAD + half_w + GAP_4, r7, half_w, ROW_H })) {
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

void toolbox_sync_size(void) {
  s_size_mult = size_to_mult(current_size());
}

void toolbox_render(void) {
  if (!g_state->toolbox_open) return;
  load_assets();
  s_size_mult = size_to_mult(current_size());

  float bx = s_popup_pos.x;
  float by = s_popup_pos.y;
  float bh = box_height();

  DrawRectangle((int)bx, (int)by, BOX_W, (int)bh, (Color){ 20, 20, 20, 230 });
  DrawRectangleLines((int)bx, (int)by, BOX_W, (int)bh, (Color){ 65, 65, 65, 255 });

  ToolType cur = g_state->current_tool;

  // Row 0: [Pen] [Eraser] [HL] [Line]
  float r0 = row_y(0);
  draw_btn(col_x(0), r0, COLS_4_W, ROW_H, "Pen", pen_tex, cur == TOOL_PEN);
  update_tooltip("Pen (1)", (Rectangle){ col_x(0), r0, COLS_4_W, ROW_H });

  draw_btn(col_x(1), r0, COLS_4_W, ROW_H, "Eras", eras_tex, cur == TOOL_ERASER);
  update_tooltip("Eraser (2)", (Rectangle){ col_x(1), r0, COLS_4_W, ROW_H });

  draw_btn(col_x(2), r0, COLS_4_W, ROW_H, "HL", hl_tex, cur == TOOL_HIGHLIGHTER);
  update_tooltip("Highlighter (Shift+1)", (Rectangle){ col_x(2), r0, COLS_4_W, ROW_H });

  draw_btn(col_x(3), r0, COLS_4_W, ROW_H, "/", (Texture2D){ 0 }, cur == TOOL_LINE);
  update_tooltip("Straight Line (3)", (Rectangle){ col_x(3), r0, COLS_4_W, ROW_H });

  // Row 1: [Arrow] [Triangle] [Rectangle] [Circle]
  float r1 = row_y(1);
  draw_btn(col_x(0), r1, COLS_4_W, ROW_H, "->", (Texture2D){ 0 }, cur == TOOL_ARROW);
  update_tooltip("Arrow (4)", (Rectangle){ col_x(0), r1, COLS_4_W, ROW_H });

  draw_btn(col_x(1), r1, COLS_4_W, ROW_H, "/\\", (Texture2D){ 0 }, cur == TOOL_TRIANGLE);
  update_tooltip("Triangle (5)", (Rectangle){ col_x(1), r1, COLS_4_W, ROW_H });

  draw_btn(col_x(2), r1, COLS_4_W, ROW_H, "[]", (Texture2D){ 0 }, cur == TOOL_RECTANGLE);
  update_tooltip("Rectangle (6)", (Rectangle){ col_x(2), r1, COLS_4_W, ROW_H });

  draw_btn(col_x(3), r1, COLS_4_W, ROW_H, "()", (Texture2D){ 0 }, cur == TOOL_CIRCLE);
  update_tooltip("Circle (7)", (Rectangle){ col_x(3), r1, COLS_4_W, ROW_H });

  // Row 2: [Step Badge] [Text] [Table] [Board]
  float r2 = row_y(2);
  draw_btn(col_x(0), r2, COLS_4_W, ROW_H, "(1)", (Texture2D){ 0 }, cur == TOOL_STEP_BADGE);
  update_tooltip("Step Badge (8) [- to pop]", (Rectangle){ col_x(0), r2, COLS_4_W, ROW_H });

  draw_btn(col_x(1), r2, COLS_4_W, ROW_H, "Txt", (Texture2D){ 0 }, cur == TOOL_TEXT);
  update_tooltip("Text Tool (9)", (Rectangle){ col_x(1), r2, COLS_4_W, ROW_H });

  draw_btn(col_x(2), r2, COLS_4_W, ROW_H, "#", (Texture2D){ 0 }, cur == TOOL_TABLE);
  update_tooltip("Table Tool (0) [Arrows resize]", (Rectangle){ col_x(2), r2, COLS_4_W, ROW_H });

  draw_btn(col_x(3), r2, COLS_4_W, ROW_H, "Brd", (Texture2D){ 0 }, g_state->black_board_enabled);
  update_tooltip("Blackboard (B)", (Rectangle){ col_x(3), r2, COLS_4_W, ROW_H });

  // Row 3: Shape Settings [Solid] [Dashed] [Dotted] [Fill]
  float r3 = row_y(3);
  draw_btn(col_x(0), r3, COLS_4_W, ROW_H, "Solid", (Texture2D){ 0 }, g_state->shape_stroke_style == STYLE_SOLID);
  update_tooltip("Solid Stroke", (Rectangle){ col_x(0), r3, COLS_4_W, ROW_H });

  draw_btn(col_x(1), r3, COLS_4_W, ROW_H, "Dash", (Texture2D){ 0 }, g_state->shape_stroke_style == STYLE_DASHED);
  update_tooltip("Dashed Stroke (Shift+Num / [ ])", (Rectangle){ col_x(1), r3, COLS_4_W, ROW_H });

  draw_btn(col_x(2), r3, COLS_4_W, ROW_H, "Dots", (Texture2D){ 0 }, g_state->shape_stroke_style == STYLE_DOTTED);
  update_tooltip("Dotted Stroke", (Rectangle){ col_x(2), r3, COLS_4_W, ROW_H });

  draw_btn(col_x(3), r3, COLS_4_W, ROW_H, g_state->shape_filled ? "Fill*" : "Fill", (Texture2D){ 0 }, g_state->shape_filled);
  update_tooltip("Fill Mode (Ctrl+F)", (Rectangle){ col_x(3), r3, COLS_4_W, ROW_H });

  // Row 4: Size label + slider + editable value
  float r4 = row_y(4);
  float sz  = mult_to_size(s_size_mult);
  char szbuf[16];
  snprintf(szbuf, sizeof(szbuf), "%.1f", sz);
  const char* val_str = s_editing_size ? s_edit_buf : szbuf;
  Color val_col = s_editing_size ? (Color){ 255, 255, 100, 255 } : WHITE;
  float val_w = MeasureTextEx(tool_font, val_str, FONT_SZ, 1).x + 8;
  float val_x = s_popup_pos.x + BOX_W - BOX_PAD - val_w;
  float val_y = r4 + (ROW_H - FONT_SZ) / 2;

  txt(s_popup_pos.x + BOX_PAD, r4 + 2, "Size", (Color){ 170, 170, 170, 255 });

  float sl_x = s_popup_pos.x + BOX_PAD;
  float sl_w = BOX_W - BOX_PAD * 2 - val_w - 4;
  if (sl_w < 20) sl_w = 20;
  float track_y = r4 + ROW_H - SLIDER_H - 4;
  DrawRectangle((int)sl_x, (int)track_y, (int)sl_w, SLIDER_H, (Color){ 55, 55, 55, 255 });
  if (s_size_mult > 0)
    DrawRectangle((int)sl_x, (int)track_y, (int)(s_size_mult * sl_w), SLIDER_H, (Color){ 80, 140, 220, 255 });

  float thumb_x = sl_x + s_size_mult * sl_w;
  DrawCircleV((Vector2){ thumb_x, track_y + SLIDER_H / 2.0F }, THUMB_R, (Color){ 210, 210, 210, 255 });

  txt(val_x, val_y, val_str, val_col);
  if (s_editing_size && fmod(GetTime() - s_edit_start, 1.0) < 0.5) {
    float cur_x = val_x + MeasureTextEx(tool_font, s_edit_buf, FONT_SZ, 1).x;
    DrawRectangle((int)cur_x, (int)val_y, 2, FONT_SZ, val_col);
  }
  update_tooltip("Size (+ / -)", (Rectangle){ sl_x, r4, sl_w, ROW_H });

  // Row 5: Colors + swap
  float r5 = row_y(5);
  float cc_w   = COLOR_SZ;
  float sw_sz  = 24;
  float sw_gap = 10;
  float cc_tot = cc_w + sw_gap + sw_sz + sw_gap + cc_w;
  float cc_x   = s_popup_pos.x + (BOX_W - cc_tot) / 2;
  float c2_x   = cc_x + cc_w + sw_gap + sw_sz + sw_gap;
  float swap_x = cc_x + cc_w + sw_gap;

  DrawRectangle((int)cc_x, (int)r5, (int)cc_w, ROW_H, g_state->color1);
  DrawRectangleLines((int)cc_x, (int)r5, (int)cc_w, ROW_H, (g_state->active_swatch == 0) ? (Color){ 100, 170, 255, 255 } : (Color){ 65, 65, 65, 220 });
  update_tooltip("Color 1 (click to pick)", (Rectangle){ cc_x, r5, cc_w, ROW_H });

  draw_btn(swap_x, r5, sw_sz, ROW_H, "<>", (Texture2D){ 0 }, false);
  update_tooltip("Swap Colors (X)", (Rectangle){ swap_x, r5, sw_sz, ROW_H });

  DrawRectangle((int)c2_x, (int)r5, (int)cc_w, ROW_H, g_state->color2);
  DrawRectangleLines((int)c2_x, (int)r5, (int)cc_w, ROW_H, (g_state->active_swatch == 1) ? (Color){ 100, 170, 255, 255 } : (Color){ 65, 65, 65, 220 });
  update_tooltip("Color 2 (click to pick)", (Rectangle){ c2_x, r5, cc_w, ROW_H });

  // Row 6: Zoom slider
  float r6 = row_y(6);
  char zbuf[16];
  snprintf(zbuf, sizeof(zbuf), "%.1fx", g_state->zoom);
  float zval_w = MeasureTextEx(tool_font, zbuf, FONT_SZ, 1).x + 8;
  float zval_x = s_popup_pos.x + BOX_W - BOX_PAD - zval_w;
  float zval_y = r6 + (ROW_H - FONT_SZ) / 2;

  txt(s_popup_pos.x + BOX_PAD, r6 + 2, "Zoom", (Color){ 170, 170, 170, 255 });

  float zsl_x = s_popup_pos.x + BOX_PAD;
  float zsl_w = BOX_W - BOX_PAD * 2 - zval_w - 4;
  if (zsl_w < 20) zsl_w = 20;
  float ztrack_y = r6 + ROW_H - SLIDER_H - 4;
  DrawRectangle((int)zsl_x, (int)ztrack_y, (int)zsl_w, SLIDER_H, (Color){ 55, 55, 55, 255 });

  float zt = Clamp((zoom_to_slider(g_state->zoom) + 1.0F) / 2.0F, 0.0F, 1.0F);
  if (zt > 0)
    DrawRectangle((int)zsl_x, (int)ztrack_y, (int)(zt * zsl_w), SLIDER_H, (Color){ 80, 140, 220, 255 });

  float zthumb_x = zsl_x + zt * zsl_w;
  DrawCircleV((Vector2){ zthumb_x, ztrack_y + SLIDER_H / 2.0F }, THUMB_R, (Color){ 210, 210, 210, 255 });
  txt(zval_x, zval_y, zbuf, WHITE);
  update_tooltip("Zoom (wheel / drag)", (Rectangle){ zsl_x, r6, zsl_w, ROW_H });

  // Row 7: [Clear] [Fit]
  float r7 = row_y(7);
  float half_w = (BOX_W - BOX_PAD * 2 - GAP_4) / 2;
  draw_btn(s_popup_pos.x + BOX_PAD, r7, half_w, ROW_H, "Clear", trash_tex, false);
  update_tooltip("Clear current annotations", (Rectangle){ s_popup_pos.x + BOX_PAD, r7, half_w, ROW_H });

  draw_btn(s_popup_pos.x + BOX_PAD + half_w + GAP_4, r7, half_w, ROW_H, "Fit", fit_tex, false);
  update_tooltip("Fit to screen (A)", (Rectangle){ s_popup_pos.x + BOX_PAD + half_w + GAP_4, r7, half_w, ROW_H });

  draw_tooltip_if_hovering();
}

// ── keymaps popup ───────────────────────────────────────────

void keymaps_render(void) {
  if (!g_state->keymaps_open) return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  float pw = 520.0f;
  float ph = 430.0f;
  float px = (sw - pw) / 2.0f;
  float py = (sh - ph) / 2.0f - 10.0f;

  DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){ 20, 20, 20, 235 });
  DrawRectangleLines((int)px, (int)py, (int)pw, (int)ph, (Color){ 65, 65, 65, 255 });

  int fs = 18;
  int ly = (int)py + 16;
  int gap = 24;
  int lx1 = (int)px + 24;
  int lx2 = (int)px + 270;

  DrawText("Roomer Keybindings", lx1, ly, 22, (Color){ 210, 210, 210, 255 });
  ly += gap + 10;

  DrawText("1       Pen",                   lx1, ly, fs, WHITE);
  DrawText("Shift+1 Highlighter",           lx1, ly + gap, fs, WHITE);
  DrawText("2       Eraser",                lx1, ly + gap * 2, fs, WHITE);
  DrawText("3       Straight Line",         lx1, ly + gap * 3, fs, WHITE);
  DrawText("4       Arrow",                 lx1, ly + gap * 4, fs, WHITE);
  DrawText("5       Triangle",              lx1, ly + gap * 5, fs, WHITE);
  DrawText("6       Rectangle",             lx1, ly + gap * 6, fs, WHITE);
  DrawText("7       Circle",                lx1, ly + gap * 7, fs, WHITE);
  DrawText("8       Step Badge",            lx1, ly + gap * 8, fs, WHITE);
  DrawText("9       Text Tool",             lx1, ly + gap * 9, fs, WHITE);
  DrawText("0       Table Tool",            lx1, ly + gap * 10, fs, WHITE);

  DrawText("Ctrl+F    Toggle Fill",         lx2, ly, fs, WHITE);
  DrawText("Shift+3-7 Cycle Solid/Dash/Dot",lx2, ly + gap, fs, WHITE);
  DrawText("[ / ]     Dash Spacing",        lx2, ly + gap * 2, fs, WHITE);
  DrawText("-         Pop Step Badge",      lx2, ly + gap * 3, fs, WHITE);
  DrawText("Arrows    Table Rows/Cols",     lx2, ly + gap * 4, fs, WHITE);
  DrawText("+ / -     Brush / Eraser Size", lx2, ly + gap * 5, fs, WHITE);
  DrawText("X         Swap Colors",         lx2, ly + gap * 6, fs, WHITE);
  DrawText("B         Blackboard",          lx2, ly + gap * 7, fs, WHITE);
  DrawText("C         Toolbox",             lx2, ly + gap * 8, fs, WHITE);
  DrawText("F         Flashlight",          lx2, ly + gap * 9, fs, WHITE);
  DrawText("A         Fit to Screen",       lx2, ly + gap * 10, fs, WHITE);

  ly += gap * 11 + 10;

  DrawText("Right Drag / Pen: Draw   |   Left Drag: Pan   |   Wheel: Zoom   |   Esc/Q: Close", lx1, ly, 15, (Color){ 170, 170, 170, 255 });
}
