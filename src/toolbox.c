#include "roomer.h"
#include "pen_png.h"
#include "eraser_png.h"
#include "highlighter_png.h"
#include "trash_png.h"
#include "fit_png.h"
#include "font_ttf.h"
#include "line_png.h"
#include "arrow_png.h"
#include "triangle_png.h"
#include "rect_png.h"
#include "circle_png.h"
#include "table_png.h"
#include "badge_png.h"
#include "text_png.h"
#include "board_png.h"

#define BOX_W      280
#define BOX_PAD    10
#define ROW_H      32
#define ROW_GAP    6
#define COLS_4_W   60
#define GAP_4      6
#define ICON_SZ    20
#define FONT_SZ    17
#define COLOR_SZ   38
#define ROW_COUNT  8
#define SLIDER_H   6
#define THUMB_R    6

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
static Texture2D board_tex = { 0 };
static Texture2D line_tex  = { 0 };
static Texture2D arrow_tex = { 0 };
static Texture2D tri_tex   = { 0 };
static Texture2D rect_tex  = { 0 };
static Texture2D circ_tex  = { 0 };
static Texture2D table_tex = { 0 };
static Texture2D badge_tex = { 0 };
static Texture2D text_tex  = { 0 };
static Font      tool_font = { 0 };
static bool      assets_loaded = false;

static Texture2D load_icon_from_mem(const unsigned char* data, unsigned int len) {
  Image img = LoadImageFromMemory(".png", data, (int)len);
  if (img.data == NULL) return (Texture2D){ 0 };
  ImageResize(&img, ICON_SZ, ICON_SZ);
  Texture2D tex = LoadTextureFromImage(img);
  UnloadImage(img);
  return tex;
}

static void load_assets(void) {
  if (assets_loaded) return;
  assets_loaded = true;

  pen_tex   = load_icon_from_mem(assets_pen_white_png, assets_pen_white_png_len);
  eras_tex  = load_icon_from_mem(assets_eraser_white_png, assets_eraser_white_png_len);
  hl_tex    = load_icon_from_mem(assets_highlighter_white_png, assets_highlighter_white_png_len);
  trash_tex = load_icon_from_mem(assets_trash_white_png, assets_trash_white_png_len);
  fit_tex   = load_icon_from_mem(assets_fit_white_png, assets_fit_white_png_len);
  board_tex = load_icon_from_mem(assets_board_white_png, assets_board_white_png_len);
  line_tex  = load_icon_from_mem(assets_line_white_png, assets_line_white_png_len);
  arrow_tex = load_icon_from_mem(assets_arrow_white_png, assets_arrow_white_png_len);
  tri_tex   = load_icon_from_mem(assets_triangle_white_png, assets_triangle_white_png_len);
  rect_tex  = load_icon_from_mem(assets_rect_white_png, assets_rect_white_png_len);
  circ_tex  = load_icon_from_mem(assets_circle_white_png, assets_circle_white_png_len);
  table_tex = load_icon_from_mem(assets_table_white_png, assets_table_white_png_len);
  badge_tex = load_icon_from_mem(assets_badge_white_png, assets_badge_white_png_len);
  text_tex  = load_icon_from_mem(assets_text_white_png, assets_text_white_png_len);

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
  DrawTextEx(tool_font, s, (Vector2){ (float)(int)x, (float)(int)y }, FONT_SZ, 1, c);
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
    DrawTextEx(tool_font, label, (Vector2){ (float)(int)tx, (float)(int)ty }, FONT_SZ, 1, WHITE);
  }
}

static void draw_swatch(float x, float y, float w, float h, Color color, bool active, bool show_pen_icon) {
  DrawRectangle((int)x, (int)y, (int)w, (int)h, color);
  DrawRectangleLines((int)x, (int)y, (int)w, (int)h, active ? (Color){ 100, 170, 255, 255 } : (Color){ 65, 65, 65, 220 });

  if (show_pen_icon && pen_tex.id > 0) {
    Color inv = { (unsigned char)(255 - color.r), (unsigned char)(255 - color.g), (unsigned char)(255 - color.b), 255 };
    DrawTexture(pen_tex, (int)(x + 3), (int)(y + h - ICON_SZ - 3), inv);
  }
}

// ── Tooltip State ───────────────────────────────────────────

typedef struct {
  Rectangle   rect;
  double      hover_start;
  bool        hovering;
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
  if (GetTime() - s_tip.hover_start < 0.35) return;

  Font f = tool_font;
  Vector2 ms = MeasureTextEx(f, s_tip.label, FONT_SZ, 1);
  float tw = ms.x + 12;
  float th = ms.y + 6;
  float tx = s_tip.rect.x + (s_tip.rect.width - tw) / 2;
  float ty = s_popup_pos.y + box_height() + 4;

  float sw = (float)GetScreenWidth();
  if (tx < 4) tx = 4;
  if (tx + tw > sw - 4) tx = sw - 4 - tw;

  DrawRectangle((int)tx, (int)ty, (int)tw, (int)th, (Color){ 25, 25, 25, 240 });
  DrawRectangleLines((int)tx, (int)ty, (int)tw, (int)th, (Color){ 110, 110, 110, 255 });
  DrawTextEx(f, s_tip.label, (Vector2){ (float)(int)(tx + 6), (float)(int)(ty + 3) }, FONT_SZ, 1, WHITE);
}

// ── Public API ──────────────────────────────────────────────

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

// ── Sliders / Dragging State ────────────────────────────────

static bool s_dragging_size    = false;
static bool s_dragging_border  = false;
static bool s_dragging_opacity = false;
static bool s_dragging_font_sz = false;
static bool s_dragging_zoom    = false;

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

static float current_pen_size(void) {
  if (g_state->current_tool == TOOL_ERASER) return g_state->tool_eraser_size;
  if (g_state->current_tool == TOOL_HIGHLIGHTER) return g_state->tool_highlighter_size;
  return g_state->tool_pen_size;
}

static void set_pen_size(float v) {
  if (g_state->current_tool == TOOL_ERASER) g_state->tool_eraser_size = Clamp(v, 5.0f, 60.0f);
  else if (g_state->current_tool == TOOL_HIGHLIGHTER) g_state->tool_highlighter_size = Clamp(v, 10.0f, 60.0f);
  else g_state->tool_pen_size = Clamp(v, 0.5f, 10.0f);
}

void toolbox_sync_size(void) {
  // Sync wrapper if needed
}

static void draw_slider(float x, float y, float w, float h, const char* label, const char* val_str, float t) {
  float val_w = MeasureTextEx(tool_font, val_str, FONT_SZ, 1).x + 6;
  float lbl_w = MeasureTextEx(tool_font, label, FONT_SZ, 1).x + 8;
  float val_x = x + w - val_w;
  float val_y = y + (h - FONT_SZ) / 2;

  txt(x, y + 2, label, (Color){ 175, 175, 175, 255 });
  txt(val_x, val_y, val_str, WHITE);

  float track_x = x + lbl_w;
  float track_w = w - lbl_w - val_w - 4;
  if (track_w < 20) track_w = 20;
  float track_y = y + h - SLIDER_H - 4;

  DrawRectangle((int)track_x, (int)track_y, (int)track_w, SLIDER_H, (Color){ 55, 55, 55, 255 });
  if (t > 0.0f) {
    DrawRectangle((int)track_x, (int)track_y, (int)(t * track_w), SLIDER_H, (Color){ 80, 140, 220, 255 });
  }

  float thumb_x = track_x + t * track_w;
  DrawCircleV((Vector2){ thumb_x, track_y + SLIDER_H / 2.0F }, THUMB_R, (Color){ 220, 220, 220, 255 });
}

// ── Input Handling ──────────────────────────────────────────

void toolbox_handle_input(void) {
  if (!g_state->toolbox_open) return;
  load_assets();

  Vector2 m = GetMousePosition();
  bool in_box = CheckCollisionPointRec(m, (Rectangle){ s_popup_pos.x, s_popup_pos.y, BOX_W, box_height() });
  bool mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || g_tablet.logical_pen_down;
  bool mouse_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || g_tablet.pen_just_pressed;

  // Active drags
  if (s_dragging_size) {
    if (mouse_down) {
      float sl_x = s_popup_pos.x + BOX_PAD + 45;
      float sl_w = BOX_W - BOX_PAD * 2 - 45 - 45;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      float s_min = (g_state->current_tool == TOOL_PEN) ? 0.5f : ((g_state->current_tool == TOOL_ERASER) ? 5.0f : 10.0f);
      float s_max = (g_state->current_tool == TOOL_PEN) ? 10.0f : 60.0f;
      set_pen_size(s_min + t * (s_max - s_min));
      return;
    }
    s_dragging_size = false;
  }

  if (s_dragging_border) {
    if (mouse_down) {
      float sl_x = s_popup_pos.x + BOX_PAD + 55;
      float sl_w = BOX_W - BOX_PAD * 2 - 55 - 45;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      if (g_state->current_tool == TOOL_STEP_BADGE) {
        g_state->badge_border_thickness = Clamp(1.0f + t * 14.0f, 1.0f, 15.0f);
      } else {
        g_state->shape_thickness = Clamp(1.0f + t * 29.0f, 1.0f, 30.0f);
      }
      return;
    }
    s_dragging_border = false;
  }

  if (s_dragging_opacity) {
    if (mouse_down) {
      float sl_x = s_popup_pos.x + BOX_PAD + 55;
      float sl_w = BOX_W - BOX_PAD * 2 - 55 - 45;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      g_state->shape_fill_opacity = t;
      return;
    }
    s_dragging_opacity = false;
  }

  if (s_dragging_font_sz) {
    if (mouse_down) {
      float sl_x = s_popup_pos.x + BOX_PAD + 45;
      float sl_w = BOX_W - BOX_PAD * 2 - 45 - 45;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      g_state->text_font_size = roundf(12.0f + t * 60.0f);
      return;
    }
    s_dragging_font_sz = false;
  }

  if (s_dragging_zoom) {
    if (mouse_down) {
      float sl_x = s_popup_pos.x + BOX_PAD + 45;
      float sl_w = BOX_W - BOX_PAD * 2 - 45 - 45;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      float s = -1.0F + t * 2.0F;
      g_state->target_zoom = Clamp(slider_to_zoom(s), g_configuration->zoom_min, g_configuration->zoom_max);
      return;
    }
    s_dragging_zoom = false;
  }

  if (!mouse_pressed || !in_box) return;

  ToolType cur = g_state->current_tool;

  // Row 0: [Pen] [Eraser] [HL] [Board]
  float r0 = row_y(0);
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r0, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_PEN; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r0, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_ERASER; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r0, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_HIGHLIGHTER; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r0, COLS_4_W, ROW_H })) { g_state->black_board_enabled = !g_state->black_board_enabled; return; }

  // Row 1: [Line] [Arrow] [Triangle] [Rectangle]
  float r1 = row_y(1);
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_LINE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_ARROW; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_TRIANGLE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r1, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_RECTANGLE; return; }

  // Row 2: [Circle] [Table] [Step Badge] [Text]
  float r2 = row_y(2);
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r2, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_CIRCLE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r2, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_TABLE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r2, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_STEP_BADGE; return; }
  if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r2, COLS_4_W, ROW_H })) { g_state->current_tool = TOOL_TEXT; return; }

  // ── Contextual Rows 3, 4, 5 ───────────────────────────────
  float r3 = row_y(3);
  float r4 = row_y(4);
  float r5 = row_y(5);

  if (cur == TOOL_PEN || cur == TOOL_HIGHLIGHTER || cur == TOOL_ERASER) {
    // Row 3: Pen Size Slider
    float sl_x = s_popup_pos.x + BOX_PAD + 45;
    float sl_w = BOX_W - BOX_PAD * 2 - 45 - 45;
    if (CheckCollisionPointRec(m, (Rectangle){ sl_x, r3, sl_w, ROW_H })) {
      s_dragging_size = true;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      float s_min = (cur == TOOL_PEN) ? 0.5f : ((cur == TOOL_ERASER) ? 5.0f : 10.0f);
      float s_max = (cur == TOOL_PEN) ? 10.0f : 60.0f;
      set_pen_size(s_min + t * (s_max - s_min));
      return;
    }

    // Row 4: Colors Swatches + Swap
    if (cur != TOOL_ERASER) {
      float cc_w   = COLOR_SZ;
      float sw_sz  = 26;
      float sw_gap = 12;
      float cc_tot = cc_w + sw_gap + sw_sz + sw_gap + cc_w;
      float cc_x   = s_popup_pos.x + (BOX_W - cc_tot) / 2;
      float c2_x   = cc_x + cc_w + sw_gap + sw_sz + sw_gap;
      float swap_x = cc_x + cc_w + sw_gap;

      if (CheckCollisionPointRec(m, (Rectangle){ cc_x, r4, cc_w, ROW_H })) {
        g_state->color1 = open_color_picker(g_state->color1);
        g_state->active_swatch = 0;
        g_configuration->draw_color = g_state->color1;
        return;
      }
      if (CheckCollisionPointRec(m, (Rectangle){ swap_x, r4, sw_sz, ROW_H })) {
        g_state->active_swatch = !g_state->active_swatch;
        g_configuration->draw_color = g_state->active_swatch ? g_state->color2 : g_state->color1;
        return;
      }
      if (CheckCollisionPointRec(m, (Rectangle){ c2_x, r4, cc_w, ROW_H })) {
        g_state->color2 = open_color_picker(g_state->color2);
        g_state->active_swatch = 1;
        g_configuration->draw_color = g_state->color2;
        return;
      }
    }
  } else if (cur == TOOL_TABLE) {
    // Row 3: Rows & Cols counters [ - R + ] [ - C + ]
    float btn_w = 28;
    float rx0 = s_popup_pos.x + BOX_PAD;
    if (CheckCollisionPointRec(m, (Rectangle){ rx0, r3, btn_w, ROW_H }) && g_state->table_rows > 1) { g_state->table_rows--; return; }
    if (CheckCollisionPointRec(m, (Rectangle){ rx0 + 64, r3, btn_w, ROW_H })) { g_state->table_rows++; return; }

    float cx0 = rx0 + 135;
    if (CheckCollisionPointRec(m, (Rectangle){ cx0, r3, btn_w, ROW_H }) && g_state->table_cols > 1) { g_state->table_cols--; return; }
    if (CheckCollisionPointRec(m, (Rectangle){ cx0 + 64, r3, btn_w, ROW_H })) { g_state->table_cols++; return; }

    // Row 4: Border thickness slider + Fill toggle
    float sl_x = s_popup_pos.x + BOX_PAD + 55;
    float sl_w = BOX_W - BOX_PAD * 2 - 55 - 45;
    if (CheckCollisionPointRec(m, (Rectangle){ sl_x, r4, sl_w, ROW_H })) {
      s_dragging_border = true;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      g_state->shape_thickness = Clamp(1.0f + t * 29.0f, 1.0f, 30.0f);
      return;
    }

    // Row 5: Border Color (yad) + Fill Color (yad) + Fill Opacity slider
    float bx = s_popup_pos.x + BOX_PAD;
    if (CheckCollisionPointRec(m, (Rectangle){ bx, r5, 42, ROW_H })) {
      g_state->shape_border_color = open_color_picker(g_state->shape_border_color);
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ bx + 48, r5, 42, ROW_H })) {
      g_state->fill_color = open_color_picker(g_state->fill_color);
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ bx + 96, r5, 42, ROW_H })) {
      stroke_toggle_fill_last();
      return;
    }
    float op_x = bx + 144;
    float op_w = BOX_W - BOX_PAD * 2 - 144;
    if (CheckCollisionPointRec(m, (Rectangle){ op_x, r5, op_w, ROW_H })) {
      s_dragging_opacity = true;
      g_state->shape_fill_opacity = Clamp((m.x - op_x) / op_w, 0.0f, 1.0f);
      return;
    }
  } else if (cur == TOOL_STEP_BADGE) {
    // Row 3: Counter [#1] [Reset] [Pop -] [Fill]
    float b_w = (BOX_W - BOX_PAD * 2 - GAP_4 * 3) / 4;
    if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r3, b_w, ROW_H })) {
      g_state->step_badge_counter = 1;
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r3, b_w, ROW_H })) {
      step_badge_pop_last();
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r3, b_w, ROW_H })) {
      stroke_toggle_fill_last();
      return;
    }

    // Row 4: Badge circle border thickness slider
    float sl_x = s_popup_pos.x + BOX_PAD + 55;
    float sl_w = BOX_W - BOX_PAD * 2 - 55 - 45;
    if (CheckCollisionPointRec(m, (Rectangle){ sl_x, r4, sl_w, ROW_H })) {
      s_dragging_border = true;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      g_state->badge_border_thickness = Clamp(1.0f + t * 14.0f, 1.0f, 15.0f);
      return;
    }

    // Row 5: Border Color (yad) + Fill Color (yad) + Fill Opacity
    float bx = s_popup_pos.x + BOX_PAD;
    if (CheckCollisionPointRec(m, (Rectangle){ bx, r5, 48, ROW_H })) {
      g_state->shape_border_color = open_color_picker(g_state->shape_border_color);
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ bx + 54, r5, 48, ROW_H })) {
      g_state->fill_color = open_color_picker(g_state->fill_color);
      return;
    }
    float op_x = bx + 110;
    float op_w = BOX_W - BOX_PAD * 2 - 110;
    if (CheckCollisionPointRec(m, (Rectangle){ op_x, r5, op_w, ROW_H })) {
      s_dragging_opacity = true;
      g_state->shape_fill_opacity = Clamp((m.x - op_x) / op_w, 0.0f, 1.0f);
      return;
    }
  } else if (cur == TOOL_TEXT) {
    // Row 3: Font Size slider + [B] [I]
    float bi_w = 26;
    float bi_x1 = s_popup_pos.x + BOX_W - BOX_PAD - bi_w * 2 - 4;
    float bi_x2 = bi_x1 + bi_w + 4;
    if (CheckCollisionPointRec(m, (Rectangle){ bi_x1, r3, bi_w, ROW_H })) {
      g_state->text_bold = !g_state->text_bold;
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ bi_x2, r3, bi_w, ROW_H })) {
      g_state->text_italic = !g_state->text_italic;
      return;
    }

    float sl_x = s_popup_pos.x + BOX_PAD + 40;
    float sl_w = bi_x1 - sl_x - 45;
    if (CheckCollisionPointRec(m, (Rectangle){ sl_x, r3, sl_w, ROW_H })) {
      s_dragging_font_sz = true;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      g_state->text_font_size = roundf(12.0f + t * 60.0f);
      return;
    }

    // Row 4: Text Color (yad) + Fill Color (yad) + Box Fill Toggle
    float bx = s_popup_pos.x + BOX_PAD;
    if (CheckCollisionPointRec(m, (Rectangle){ bx, r4, 60, ROW_H })) {
      g_state->shape_border_color = open_color_picker(g_state->shape_border_color);
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ bx + 66, r4, 60, ROW_H })) {
      g_state->fill_color = open_color_picker(g_state->fill_color);
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ bx + 132, r4, BOX_W - BOX_PAD * 2 - 132, ROW_H })) {
      stroke_toggle_fill_last();
      return;
    }

    // Row 5: Background Opacity Slider
    float op_x = s_popup_pos.x + BOX_PAD + 55;
    float op_w = BOX_W - BOX_PAD * 2 - 55 - 45;
    if (CheckCollisionPointRec(m, (Rectangle){ op_x, r5, op_w, ROW_H })) {
      s_dragging_opacity = true;
      g_state->shape_fill_opacity = Clamp((m.x - op_x) / op_w, 0.0f, 1.0f);
      return;
    }
  } else {
    // Default Shapes (Line, Arrow, Triangle, Rect, Circle)
    // Row 3: [Solid] [Dash] [Dots] [Fill*]
    if (CheckCollisionPointRec(m, (Rectangle){ col_x(0), r3, COLS_4_W, ROW_H })) { g_state->shape_stroke_style = STYLE_SOLID; return; }
    if (CheckCollisionPointRec(m, (Rectangle){ col_x(1), r3, COLS_4_W, ROW_H })) { g_state->shape_stroke_style = STYLE_DASHED; return; }
    if (CheckCollisionPointRec(m, (Rectangle){ col_x(2), r3, COLS_4_W, ROW_H })) { g_state->shape_stroke_style = STYLE_DOTTED; return; }
    if (CheckCollisionPointRec(m, (Rectangle){ col_x(3), r3, COLS_4_W, ROW_H })) { stroke_toggle_fill_last(); return; }

    // Row 4: Border thickness slider
    float sl_x = s_popup_pos.x + BOX_PAD + 55;
    float sl_w = BOX_W - BOX_PAD * 2 - 55 - 45;
    if (CheckCollisionPointRec(m, (Rectangle){ sl_x, r4, sl_w, ROW_H })) {
      s_dragging_border = true;
      float t = Clamp((m.x - sl_x) / sl_w, 0.0f, 1.0f);
      g_state->shape_thickness = Clamp(1.0f + t * 29.0f, 1.0f, 30.0f);
      return;
    }

    // Row 5: Border Color (yad) + Fill Color (yad) + Fill Opacity slider
    float bx = s_popup_pos.x + BOX_PAD;
    if (CheckCollisionPointRec(m, (Rectangle){ bx, r5, 48, ROW_H })) {
      g_state->shape_border_color = open_color_picker(g_state->shape_border_color);
      return;
    }
    if (CheckCollisionPointRec(m, (Rectangle){ bx + 54, r5, 48, ROW_H })) {
      g_state->fill_color = open_color_picker(g_state->fill_color);
      return;
    }
    float op_x = bx + 110;
    float op_w = BOX_W - BOX_PAD * 2 - 110;
    if (CheckCollisionPointRec(m, (Rectangle){ op_x, r5, op_w, ROW_H })) {
      s_dragging_opacity = true;
      g_state->shape_fill_opacity = Clamp((m.x - op_x) / op_w, 0.0f, 1.0f);
      return;
    }
  }

  // Row 6: Zoom slider
  float r6 = row_y(6);
  float zsl_x  = s_popup_pos.x + BOX_PAD + 45;
  float zsl_w  = BOX_W - BOX_PAD * 2 - 45 - 45;
  if (CheckCollisionPointRec(m, (Rectangle){ zsl_x, r6, zsl_w, ROW_H })) {
    s_dragging_zoom = true;
    float t = Clamp((m.x - zsl_x) / zsl_w, 0.0F, 1.0F);
    float s = -1.0F + t * 2.0F;
    g_state->target_zoom = Clamp(slider_to_zoom(s), g_configuration->zoom_min, g_configuration->zoom_max);
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

// ── Rendering ───────────────────────────────────────────────

void toolbox_render(void) {
  if (!g_state->toolbox_open) return;
  load_assets();

  rlDrawRenderBatchActive();
  rlColorMask(true, true, true, false);

  float bx = s_popup_pos.x;
  float by = s_popup_pos.y;
  float bh = box_height();

  DrawRectangle((int)bx, (int)by, BOX_W, (int)bh, (Color){ 20, 20, 24, 245 });
  DrawRectangleLines((int)bx, (int)by, BOX_W, (int)bh, (Color){ 65, 65, 75, 255 });

  ToolType cur = g_state->current_tool;

  // Row 0: [Pen] [Eraser] [HL] [Board]
  float r0 = row_y(0);
  draw_btn(col_x(0), r0, COLS_4_W, ROW_H, "Pen", pen_tex, cur == TOOL_PEN);
  update_tooltip("Pen (1)", (Rectangle){ col_x(0), r0, COLS_4_W, ROW_H });

  draw_btn(col_x(1), r0, COLS_4_W, ROW_H, "Eras", eras_tex, cur == TOOL_ERASER);
  update_tooltip("Eraser (2)", (Rectangle){ col_x(1), r0, COLS_4_W, ROW_H });

  draw_btn(col_x(2), r0, COLS_4_W, ROW_H, "HL", hl_tex, cur == TOOL_HIGHLIGHTER);
  update_tooltip("Highlighter (Shift+1)", (Rectangle){ col_x(2), r0, COLS_4_W, ROW_H });

  draw_btn(col_x(3), r0, COLS_4_W, ROW_H, "Brd", board_tex, g_state->black_board_enabled);
  update_tooltip("Blackboard (B)", (Rectangle){ col_x(3), r0, COLS_4_W, ROW_H });

  // Row 1: [Line] [Arrow] [Triangle] [Rectangle]
  float r1 = row_y(1);
  draw_btn(col_x(0), r1, COLS_4_W, ROW_H, "/", line_tex, cur == TOOL_LINE);
  update_tooltip("Straight Line (3) [Shift: 45 deg snap]", (Rectangle){ col_x(0), r1, COLS_4_W, ROW_H });

  draw_btn(col_x(1), r1, COLS_4_W, ROW_H, "->", arrow_tex, cur == TOOL_ARROW);
  update_tooltip("Arrow (4) [Shift: 45 deg snap]", (Rectangle){ col_x(1), r1, COLS_4_W, ROW_H });

  draw_btn(col_x(2), r1, COLS_4_W, ROW_H, "/\\", tri_tex, cur == TOOL_TRIANGLE);
  update_tooltip("Rotatable Triangle (5) [Drag apex, Shift: 45 deg snap]", (Rectangle){ col_x(2), r1, COLS_4_W, ROW_H });

  draw_btn(col_x(3), r1, COLS_4_W, ROW_H, "[]", rect_tex, cur == TOOL_RECTANGLE);
  update_tooltip("Rectangle (6) [Shift: 1:1 square]", (Rectangle){ col_x(3), r1, COLS_4_W, ROW_H });

  // Row 2: [Circle] [Table] [Step Badge] [Text]
  float r2 = row_y(2);
  draw_btn(col_x(0), r2, COLS_4_W, ROW_H, "()", circ_tex, cur == TOOL_CIRCLE);
  update_tooltip("Circle (7)", (Rectangle){ col_x(0), r2, COLS_4_W, ROW_H });

  draw_btn(col_x(1), r2, COLS_4_W, ROW_H, "#", table_tex, cur == TOOL_TABLE);
  update_tooltip("Table Tool (0) [Arrows resize]", (Rectangle){ col_x(1), r2, COLS_4_W, ROW_H });

  draw_btn(col_x(2), r2, COLS_4_W, ROW_H, "(1)", badge_tex, cur == TOOL_STEP_BADGE);
  update_tooltip("Step Badge (8) [- to pop]", (Rectangle){ col_x(2), r2, COLS_4_W, ROW_H });

  draw_btn(col_x(3), r2, COLS_4_W, ROW_H, "Txt", text_tex, cur == TOOL_TEXT);
  update_tooltip("Text Tool (9) [Vector Cairo font]", (Rectangle){ col_x(3), r2, COLS_4_W, ROW_H });

  // ── Contextual Rows 3, 4, 5 ───────────────────────────────
  float r3 = row_y(3);
  float r4 = row_y(4);
  float r5 = row_y(5);

  if (cur == TOOL_PEN || cur == TOOL_HIGHLIGHTER || cur == TOOL_ERASER) {
    // Row 3: Pen / Brush Size Slider
    float sz = current_pen_size();
    char szbuf[16];
    snprintf(szbuf, sizeof(szbuf), "%.1f", sz);
    float s_min = (cur == TOOL_PEN) ? 0.5f : ((cur == TOOL_ERASER) ? 5.0f : 10.0f);
    float s_max = (cur == TOOL_PEN) ? 10.0f : 60.0f;
    float t = (sz - s_min) / (s_max - s_min);
    draw_slider(s_popup_pos.x + BOX_PAD, r3, BOX_W - BOX_PAD * 2, ROW_H, "Size", szbuf, t);
    update_tooltip("Brush / Eraser Size (+ / -)", (Rectangle){ s_popup_pos.x + BOX_PAD, r3, BOX_W - BOX_PAD * 2, ROW_H });

    // Row 4: Swatches with active pen indicator
    if (cur != TOOL_ERASER) {
      float cc_w   = COLOR_SZ;
      float sw_sz  = 26;
      float sw_gap = 12;
      float cc_tot = cc_w + sw_gap + sw_sz + sw_gap + cc_w;
      float cc_x   = s_popup_pos.x + (BOX_W - cc_tot) / 2;
      float c2_x   = cc_x + cc_w + sw_gap + sw_sz + sw_gap;
      float swap_x = cc_x + cc_w + sw_gap;

      draw_swatch(cc_x, r4, cc_w, ROW_H, g_state->color1, g_state->active_swatch == 0, g_state->active_swatch == 0);
      update_tooltip("Color 1 (click to pick with yad)", (Rectangle){ cc_x, r4, cc_w, ROW_H });

      draw_btn(swap_x, r4, sw_sz, ROW_H, "<>", (Texture2D){ 0 }, false);
      update_tooltip("Swap Colors (X)", (Rectangle){ swap_x, r4, sw_sz, ROW_H });

      draw_swatch(c2_x, r4, cc_w, ROW_H, g_state->color2, g_state->active_swatch == 1, g_state->active_swatch == 1);
      update_tooltip("Color 2 (click to pick with yad)", (Rectangle){ c2_x, r4, cc_w, ROW_H });
    }

    // Row 5: Hint / Quick Info
    txt(s_popup_pos.x + BOX_PAD + 15, r5 + 8, "Left: Pan  |  Right: Draw", (Color){ 140, 140, 140, 255 });
  } else if (cur == TOOL_TABLE) {
    // Row 3: Rows and Cols counters [ - R + ]  [ - C + ]
    float btn_w = 28;
    float rx0 = s_popup_pos.x + BOX_PAD;
    draw_btn(rx0, r3, btn_w, ROW_H, "-", (Texture2D){ 0 }, false);
    char rbuf[16];
    snprintf(rbuf, sizeof(rbuf), "R:%d", g_state->table_rows);
    txt(rx0 + 32, r3 + 7, rbuf, WHITE);
    draw_btn(rx0 + 64, r3, btn_w, ROW_H, "+", (Texture2D){ 0 }, false);

    float cx0 = rx0 + 135;
    draw_btn(cx0, r3, btn_w, ROW_H, "-", (Texture2D){ 0 }, false);
    char cbuf[16];
    snprintf(cbuf, sizeof(cbuf), "C:%d", g_state->table_cols);
    txt(cx0 + 32, r3 + 7, cbuf, WHITE);
    draw_btn(cx0 + 64, r3, btn_w, ROW_H, "+", (Texture2D){ 0 }, false);
    update_tooltip("Adjust Rows & Columns (or use Arrow keys while dragging)", (Rectangle){ rx0, r3, BOX_W - BOX_PAD * 2, ROW_H });

    // Row 4: Border thickness slider
    char bbuf[16];
    snprintf(bbuf, sizeof(bbuf), "%.1f", g_state->shape_thickness);
    float bt = (g_state->shape_thickness - 1.0f) / 29.0f;
    draw_slider(s_popup_pos.x + BOX_PAD, r4, BOX_W - BOX_PAD * 2, ROW_H, "Border", bbuf, bt);

    // Row 5: Border Color + Fill Color + Fill Toggle + Opacity
    float bx = s_popup_pos.x + BOX_PAD;
    draw_swatch(bx, r5, 42, ROW_H, g_state->shape_border_color, false, false);
    txt(bx + 5, r5 + 8, "Line", (Color){ 240, 240, 240, 255 });
    update_tooltip("Border Color (yad picker)", (Rectangle){ bx, r5, 42, ROW_H });

    draw_swatch(bx + 48, r5, 42, ROW_H, g_state->fill_color, false, false);
    txt(bx + 53, r5 + 8, "Fill", (Color){ 240, 240, 240, 255 });
    update_tooltip("Fill Color (yad picker)", (Rectangle){ bx + 48, r5, 42, ROW_H });

    draw_btn(bx + 96, r5, 42, ROW_H, g_state->shape_filled ? "On" : "Off", (Texture2D){ 0 }, g_state->shape_filled);
    update_tooltip("Toggle Background Fill (Ctrl+F)", (Rectangle){ bx + 96, r5, 42, ROW_H });

    float op_x = bx + 144;
    float op_w = BOX_W - BOX_PAD * 2 - 144;
    char opbuf[16];
    snprintf(opbuf, sizeof(opbuf), "%d%%", (int)(g_state->shape_fill_opacity * 100.0f));
    draw_slider(op_x, r5, op_w, ROW_H, "A:", opbuf, g_state->shape_fill_opacity);
    update_tooltip("Fill Opacity Slider", (Rectangle){ op_x, r5, op_w, ROW_H });
  } else if (cur == TOOL_STEP_BADGE) {
    // Row 3: Counter [#%d] [Reset] [Pop -] [Fill*]
    float b_w = (BOX_W - BOX_PAD * 2 - GAP_4 * 3) / 4;
    char nbuf[16];
    snprintf(nbuf, sizeof(nbuf), "#%d", g_state->step_badge_counter);
    draw_btn(col_x(0), r3, b_w, ROW_H, nbuf, (Texture2D){ 0 }, false);
    update_tooltip("Current Step Counter", (Rectangle){ col_x(0), r3, b_w, ROW_H });

    draw_btn(col_x(1), r3, b_w, ROW_H, "Reset", (Texture2D){ 0 }, false);
    update_tooltip("Reset Badge Counter to 1", (Rectangle){ col_x(1), r3, b_w, ROW_H });

    draw_btn(col_x(2), r3, b_w, ROW_H, "Pop -", (Texture2D){ 0 }, false);
    update_tooltip("Pop Last Badge (-)", (Rectangle){ col_x(2), r3, b_w, ROW_H });

    draw_btn(col_x(3), r3, b_w, ROW_H, g_state->shape_filled ? "Fill*" : "Fill", (Texture2D){ 0 }, g_state->shape_filled);
    update_tooltip("Toggle Filled / Hollow Badge (Ctrl+F)", (Rectangle){ col_x(3), r3, b_w, ROW_H });

    // Row 4: Badge circle border thickness slider
    char bbuf[16];
    snprintf(bbuf, sizeof(bbuf), "%.1f", g_state->badge_border_thickness);
    float bt = (g_state->badge_border_thickness - 1.0f) / 14.0f;
    draw_slider(s_popup_pos.x + BOX_PAD, r4, BOX_W - BOX_PAD * 2, ROW_H, "Ring Thk", bbuf, bt);
    update_tooltip("Badge Circle Border Thickness", (Rectangle){ s_popup_pos.x + BOX_PAD, r4, BOX_W - BOX_PAD * 2, ROW_H });

    // Row 5: Border Color (yad) + Fill Color (yad) + Fill Opacity
    float bx = s_popup_pos.x + BOX_PAD;
    draw_swatch(bx, r5, 48, ROW_H, g_state->shape_border_color, false, false);
    txt(bx + 6, r5 + 8, "Ring", WHITE);
    update_tooltip("Badge Border Ring Color (yad picker)", (Rectangle){ bx, r5, 48, ROW_H });

    draw_swatch(bx + 54, r5, 48, ROW_H, g_state->fill_color, false, false);
    txt(bx + 60, r5 + 8, "Fill", WHITE);
    update_tooltip("Badge Fill Color (yad picker)", (Rectangle){ bx + 54, r5, 48, ROW_H });

    float op_x = bx + 110;
    float op_w = BOX_W - BOX_PAD * 2 - 110;
    char opbuf[16];
    snprintf(opbuf, sizeof(opbuf), "%d%%", (int)(g_state->shape_fill_opacity * 100.0f));
    draw_slider(op_x, r5, op_w, ROW_H, "A:", opbuf, g_state->shape_fill_opacity);
    update_tooltip("Badge Fill Opacity Slider", (Rectangle){ op_x, r5, op_w, ROW_H });
  } else if (cur == TOOL_TEXT) {
    // Row 3: Font Size slider + [B] [I]
    float bi_w = 26;
    float bi_x1 = s_popup_pos.x + BOX_W - BOX_PAD - bi_w * 2 - 4;
    float bi_x2 = bi_x1 + bi_w + 4;
    draw_btn(bi_x1, r3, bi_w, ROW_H, "B", (Texture2D){ 0 }, g_state->text_bold);
    update_tooltip("Bold Font (Ctrl+B)", (Rectangle){ bi_x1, r3, bi_w, ROW_H });

    draw_btn(bi_x2, r3, bi_w, ROW_H, "I", (Texture2D){ 0 }, g_state->text_italic);
    update_tooltip("Italic Font (Ctrl+I)", (Rectangle){ bi_x2, r3, bi_w, ROW_H });

    char fsbuf[16];
    snprintf(fsbuf, sizeof(fsbuf), "%d", (int)g_state->text_font_size);
    float t = (g_state->text_font_size - 12.0f) / 60.0f;
    float sl_w = bi_x1 - (s_popup_pos.x + BOX_PAD) - 6;
    draw_slider(s_popup_pos.x + BOX_PAD, r3, sl_w, ROW_H, "Font", fsbuf, t);
    update_tooltip("Font Size Slider", (Rectangle){ s_popup_pos.x + BOX_PAD, r3, sl_w, ROW_H });

    // Row 4: Text Color (yad) + Box Fill Color (yad) + Box Fill Toggle
    float bx = s_popup_pos.x + BOX_PAD;
    draw_swatch(bx, r4, 60, ROW_H, g_state->shape_border_color, false, false);
    txt(bx + 8, r4 + 8, "Text", WHITE);
    update_tooltip("Text Color (yad picker)", (Rectangle){ bx, r4, 60, ROW_H });

    draw_swatch(bx + 66, r4, 60, ROW_H, g_state->fill_color, false, false);
    txt(bx + 74, r4 + 8, "Box", WHITE);
    update_tooltip("Text Box Fill Color (yad picker)", (Rectangle){ bx + 66, r4, 60, ROW_H });

    draw_btn(bx + 132, r4, BOX_W - BOX_PAD * 2 - 132, ROW_H, g_state->shape_filled ? "Box: On" : "Box: Off", (Texture2D){ 0 }, g_state->shape_filled);
    update_tooltip("Toggle Background Box (Ctrl+F)", (Rectangle){ bx + 132, r4, BOX_W - BOX_PAD * 2 - 132, ROW_H });

    // Row 5: Background Opacity Slider
    char opbuf[16];
    snprintf(opbuf, sizeof(opbuf), "%d%%", (int)(g_state->shape_fill_opacity * 100.0f));
    draw_slider(s_popup_pos.x + BOX_PAD, r5, BOX_W - BOX_PAD * 2, ROW_H, "Box Opacity", opbuf, g_state->shape_fill_opacity);
    update_tooltip("Background Box Opacity Slider", (Rectangle){ s_popup_pos.x + BOX_PAD, r5, BOX_W - BOX_PAD * 2, ROW_H });
  } else {
    // Default Shapes: Line, Arrow, Triangle, Rect, Circle
    // Row 3: [Solid] [Dash] [Dots] [Fill*]
    draw_btn(col_x(0), r3, COLS_4_W, ROW_H, "Solid", (Texture2D){ 0 }, g_state->shape_stroke_style == STYLE_SOLID);
    update_tooltip("Solid Border", (Rectangle){ col_x(0), r3, COLS_4_W, ROW_H });

    draw_btn(col_x(1), r3, COLS_4_W, ROW_H, "Dash", (Texture2D){ 0 }, g_state->shape_stroke_style == STYLE_DASHED);
    update_tooltip("Dashed Border ([ / ] dash spacing)", (Rectangle){ col_x(1), r3, COLS_4_W, ROW_H });

    draw_btn(col_x(2), r3, COLS_4_W, ROW_H, "Dots", (Texture2D){ 0 }, g_state->shape_stroke_style == STYLE_DOTTED);
    update_tooltip("Dotted Border", (Rectangle){ col_x(2), r3, COLS_4_W, ROW_H });

    draw_btn(col_x(3), r3, COLS_4_W, ROW_H, g_state->shape_filled ? "Fill*" : "Fill", (Texture2D){ 0 }, g_state->shape_filled);
    update_tooltip("Toggle Fill (Ctrl+F)", (Rectangle){ col_x(3), r3, COLS_4_W, ROW_H });

    // Row 4: Border thickness slider
    char bbuf[16];
    snprintf(bbuf, sizeof(bbuf), "%.1f", g_state->shape_thickness);
    float bt = (g_state->shape_thickness - 1.0f) / 29.0f;
    draw_slider(s_popup_pos.x + BOX_PAD, r4, BOX_W - BOX_PAD * 2, ROW_H, "Border", bbuf, bt);
    update_tooltip("Shape Border Thickness Slider", (Rectangle){ s_popup_pos.x + BOX_PAD, r4, BOX_W - BOX_PAD * 2, ROW_H });

    // Row 5: Border Color (yad) + Fill Color (yad) + Fill Opacity
    float bx = s_popup_pos.x + BOX_PAD;
    draw_swatch(bx, r5, 48, ROW_H, g_state->shape_border_color, false, false);
    txt(bx + 6, r5 + 8, "Line", WHITE);
    update_tooltip("Border Color (yad picker)", (Rectangle){ bx, r5, 48, ROW_H });

    draw_swatch(bx + 54, r5, 48, ROW_H, g_state->fill_color, false, false);
    txt(bx + 60, r5 + 8, "Fill", WHITE);
    update_tooltip("Fill Color (yad picker)", (Rectangle){ bx + 54, r5, 48, ROW_H });

    float op_x = bx + 110;
    float op_w = BOX_W - BOX_PAD * 2 - 110;
    char opbuf[16];
    snprintf(opbuf, sizeof(opbuf), "%d%%", (int)(g_state->shape_fill_opacity * 100.0f));
    draw_slider(op_x, r5, op_w, ROW_H, "A:", opbuf, g_state->shape_fill_opacity);
    update_tooltip("Fill Opacity Slider (no wallpaper bleed)", (Rectangle){ op_x, r5, op_w, ROW_H });
  }

  // Row 6: Zoom slider
  float r6 = row_y(6);
  char zbuf[16];
  snprintf(zbuf, sizeof(zbuf), "%.1fx", g_state->zoom);
  float zt = Clamp((zoom_to_slider(g_state->zoom) + 1.0F) / 2.0F, 0.0F, 1.0F);
  draw_slider(s_popup_pos.x + BOX_PAD, r6, BOX_W - BOX_PAD * 2, ROW_H, "Zoom", zbuf, zt);
  update_tooltip("Canvas Zoom (wheel / drag)", (Rectangle){ s_popup_pos.x + BOX_PAD, r6, BOX_W - BOX_PAD * 2, ROW_H });

  // Row 7: [Clear] [Fit]
  float r7 = row_y(7);
  float half_w = (BOX_W - BOX_PAD * 2 - GAP_4) / 2;
  draw_btn(s_popup_pos.x + BOX_PAD, r7, half_w, ROW_H, "Clear", trash_tex, false);
  update_tooltip("Clear current annotations", (Rectangle){ s_popup_pos.x + BOX_PAD, r7, half_w, ROW_H });

  draw_btn(s_popup_pos.x + BOX_PAD + half_w + GAP_4, r7, half_w, ROW_H, "Fit", fit_tex, false);
  update_tooltip("Fit to screen (A)", (Rectangle){ s_popup_pos.x + BOX_PAD + half_w + GAP_4, r7, half_w, ROW_H });

  draw_tooltip_if_hovering();

  rlDrawRenderBatchActive();
  rlColorMask(true, true, true, true);
}

// ── Keymaps Popup ───────────────────────────────────────────

void keymaps_render(void) {
  if (!g_state->keymaps_open) return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  float pw = 520.0f;
  float ph = 440.0f;
  float px = (sw - pw) / 2.0f;
  float py = (sh - ph) / 2.0f - 10.0f;

  DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){ 20, 20, 24, 240 });
  DrawRectangleLines((int)px, (int)py, (int)pw, (int)ph, (Color){ 65, 65, 75, 255 });

  int fs = 18;
  int ly = (int)py + 16;
  int gap = 24;
  int lx1 = (int)px + 24;
  int lx2 = (int)px + 270;

  DrawText("Roomer Keybindings", lx1, ly, 22, (Color){ 220, 220, 220, 255 });
  ly += gap + 10;

  DrawText("1       Pen",                   lx1, ly, fs, WHITE);
  DrawText("Shift+1 Highlighter",           lx1, ly + gap, fs, WHITE);
  DrawText("2       Eraser",                lx1, ly + gap * 2, fs, WHITE);
  DrawText("3       Straight Line",         lx1, ly + gap * 3, fs, WHITE);
  DrawText("4       Arrow",                 lx1, ly + gap * 4, fs, WHITE);
  DrawText("5       Triangle (Rotatable)",  lx1, ly + gap * 5, fs, WHITE);
  DrawText("6       Rectangle",             lx1, ly + gap * 6, fs, WHITE);
  DrawText("7       Circle",                lx1, ly + gap * 7, fs, WHITE);
  DrawText("8       Step Badge",            lx1, ly + gap * 8, fs, WHITE);
  DrawText("9       Text Tool (Vector)",    lx1, ly + gap * 9, fs, WHITE);
  DrawText("0       Table Tool",            lx1, ly + gap * 10, fs, WHITE);

  DrawText("Shift     45 deg / 1:1 Snap",   lx2, ly, fs, WHITE);
  DrawText("Ctrl+F    Toggle Fill",         lx2, ly + gap, fs, WHITE);
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
