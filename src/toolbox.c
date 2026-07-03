#include "roomer.h"
#include "pen_png.h"
#include "eraser_png.h"
#include "trash_png.h"
#include "fit_png.h"
#include "font_ttf.h"

#define BAR_H       64
#define BTN_H       48
#define FONT_SZ     32
#define SWATCH_SZ   36
#define ICON_SZ     36
#define MARGIN      18
#define GAP_SM      8
#define GAP_MD      14

static Color color_presets[8] = {
  RED, GREEN, BLUE, YELLOW, (Color){ 0, 255, 255, 255 }, MAGENTA, WHITE, BLACK
};

// ── widget widths ──────────────────────────────────────────
#define PEN_W      52
#define ERASER_W   64
#define PEN_ERA_GAP 8

#define SEP_W      22

#define SIZE_LABEL_W 60
#define SIZE_VAL_W   56

#define MINUS_W     36
#define PLUS_W      36
#define MINUS_PLUS_GAP 4
#define MP_LEAD     16

#define SWATCH_GAP  8

#define CLEAR_W     54
#define CLEAR_FIT_GAP 8
#define FIT_W       56

// ── helpers ─────────────────────────────────────────────────

static float btn_y(void) { return (BAR_H - BTN_H) / 2; }

static float text_center_y(void) { return btn_y() + (BTN_H - FONT_SZ) / 2; }

static Rectangle bar_rect(void) {
  return (Rectangle){ 0, 0, GetScreenWidth(), BAR_H };
}

static float swatch_y(void) { return btn_y() + (BTN_H - SWATCH_SZ) / 2; }

// ── lazy-loaded assets ──────────────────────────────────────

static Texture2D pen_tex   = { 0 };
static Texture2D eras_tex  = { 0 };
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

static void draw_icon_or_text(float cx, float cy, float w, Texture2D tex, const char* fallback, Color bg) {
  DrawRectangle(cx, cy, w, BTN_H, bg);
  if (tex.id > 0) {
    float ix = cx + (w - ICON_SZ) / 2;
    float iy = cy + (BTN_H - ICON_SZ) / 2;
    DrawTexture(tex, (int)ix, (int)iy, WHITE);
  } else {
    txt(cx + 4, text_center_y(), fallback, WHITE);
  }
}

// ── total content width (for centering) ────────────────────

static float content_width(void) {
  float w = (float)PEN_W + PEN_ERA_GAP + ERASER_W;
  w += SEP_W;
  w += SIZE_LABEL_W + SIZE_VAL_W + MP_LEAD;
  w += MINUS_W + MINUS_PLUS_GAP + PLUS_W + GAP_MD;
  for (int i = 0; i < 8; i++) w += (float)SWATCH_SZ + (i < 7 ? SWATCH_GAP : 0);
  w += GAP_MD + SEP_W + GAP_MD;
  w += CLEAR_W + CLEAR_FIT_GAP + FIT_W;
  return w;
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
  if (s_tip.hovering && s_tip.label == label) {
    // still hovering same button
  } else {
    s_tip.hovering    = true;
    s_tip.hover_start = GetTime();
    s_tip.label       = label;
    s_tip.rect        = btn_rect;
  }
}

static void draw_tooltip_if_hovering(void) {
  if (!s_tip.hovering) return;
  if (GetTime() - s_tip.hover_start < 0.5) return;

  Font f = tool_font;
  Vector2 ms = MeasureTextEx(f, s_tip.label, FONT_SZ, 1);
  float tw = ms.x + 12;
  float th = ms.y + 6;
  float tx = s_tip.rect.x + (s_tip.rect.width - tw) / 2;
  float ty = BAR_H + 4;

  DrawRectangle((int)tx, (int)ty, (int)tw, (int)th, (Color){ 30, 30, 30, 230 });
  DrawRectangleLines((int)tx, (int)ty, (int)tw, (int)th, (Color){ 120, 120, 120, 255 });
  DrawTextEx(f, s_tip.label, (Vector2){ tx + 6, ty + 3 }, FONT_SZ, 1, WHITE);
}

// ── public API ─────────────────────────────────────────────

void toolbox_toggle(void) {
  g_state->toolbox_open = !g_state->toolbox_open;
}

bool toolbox_is_open(void) {
  return g_state->toolbox_open;
}

bool toolbox_is_mouse_over(void) {
  if (!g_state->toolbox_open) return false;
  return CheckCollisionPointRec(GetMousePosition(), bar_rect());
}

void toolbox_handle_input(void) {
  if (!g_state->toolbox_open) return;
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_tablet.pen_just_pressed) return;

  Vector2 m = GetMousePosition();
  if (!CheckCollisionPointRec(m, bar_rect())) return;

  float sw = (float)GetScreenWidth();
  float cx = (sw - content_width()) / 2;
  float by = btn_y();

  // Pen
  if (CheckCollisionPointRec(m, (Rectangle){ cx, by, PEN_W, BTN_H })) { g_state->current_tool = TOOL_PEN; return; }
  cx += PEN_W + PEN_ERA_GAP;

  // Eraser
  if (CheckCollisionPointRec(m, (Rectangle){ cx, by, ERASER_W, BTN_H })) { g_state->current_tool = TOOL_ERASER; return; }
  cx += ERASER_W;

  // Sep
  cx += SEP_W;

  // Size label / value (non-interactive)
  cx += SIZE_LABEL_W + SIZE_VAL_W + MP_LEAD;

  float cur = (g_state->current_tool == TOOL_PEN) ? g_state->tool_pen_size : g_state->tool_eraser_size;
  float hi  = (g_state->current_tool == TOOL_PEN) ? 30.0F : 60.0F;
  float st  = (g_state->current_tool == TOOL_PEN) ? 0.5F : 1.0F;

  // Minus
  if (CheckCollisionPointRec(m, (Rectangle){ cx, by, MINUS_W, BTN_H })) {
    float v = cur - st; if (v >= 1.0F) { if (g_state->current_tool == TOOL_PEN) g_state->tool_pen_size = v; else g_state->tool_eraser_size = v; } return;
  }
  cx += MINUS_W + MINUS_PLUS_GAP;

  // Plus
  if (CheckCollisionPointRec(m, (Rectangle){ cx, by, PLUS_W, BTN_H })) {
    float v = cur + st; if (v <= hi) { if (g_state->current_tool == TOOL_PEN) g_state->tool_pen_size = v; else g_state->tool_eraser_size = v; } return;
  }
  cx += PLUS_W + GAP_MD;

  // Color swatches
  float sc = swatch_y();
  for (int i = 0; i < 8; i++) {
    if (CheckCollisionPointRec(m, (Rectangle){ cx, sc, SWATCH_SZ, SWATCH_SZ })) { g_configuration->draw_color = color_presets[i]; return; }
    cx += SWATCH_SZ + SWATCH_GAP;
  }

  // Sep
  cx += GAP_MD + SEP_W + GAP_MD;

  // Clear All
  if (CheckCollisionPointRec(m, (Rectangle){ cx, by, CLEAR_W, BTN_H })) { lines_clear(); return; }
  cx += CLEAR_W + CLEAR_FIT_GAP;

  if (CheckCollisionPointRec(m, (Rectangle){ cx, by, FIT_W, BTN_H })) {
    if (g_state->image_w > 0 && g_state->image_h > 0) {
      float fw = (float)GetScreenWidth();
      float fh = (float)GetScreenHeight();
      g_state->target_zoom = fminf(fw / (float)g_state->image_w, fh / (float)g_state->image_h);
      g_state->target_pan.x = (fw - (float)g_state->image_w * g_state->target_zoom) / 2;
      g_state->target_pan.y = (fh - (float)g_state->image_h * g_state->target_zoom) / 2;
    }
    return;
  }
  cx += FIT_W;
}

void toolbox_render(void) {
  if (!g_state->toolbox_open) return;
  load_assets();

  float sw = (float)GetScreenWidth();

  Rectangle bar = bar_rect();
  DrawRectangle(bar.x, bar.y, bar.width, bar.height, (Color){ 20, 20, 20, 215 });
  DrawLine(bar.x, bar.y + bar.height, bar.x + bar.width, bar.y + bar.height, (Color){ 60, 60, 60, 255 });

  float cx = (sw - content_width()) / 2;
  float by = btn_y();
  float ty = text_center_y();
  Color  active_bg = (Color){ 50, 115, 210, 255 };
  Color  inactive_bg = (Color){ 40, 40, 40, 220 };

  // ── Pen ─────────────────────────────────────────────────────
  bool pen = g_state->current_tool == TOOL_PEN;
  draw_icon_or_text(cx, by, PEN_W, pen_tex, "Pen", pen ? active_bg : inactive_bg);
  update_tooltip("Pen (right-click)", (Rectangle){ cx, by, PEN_W, BTN_H });
  cx += PEN_W + PEN_ERA_GAP;

  // ── Eraser ───────────────────────────────────────────────────
  bool era = g_state->current_tool == TOOL_ERASER;
  draw_icon_or_text(cx, by, ERASER_W, eras_tex, "Erase", era ? active_bg : inactive_bg);
  update_tooltip("Eraser (right-click)", (Rectangle){ cx, by, ERASER_W, BTN_H });
  cx += ERASER_W;

  // ── Separator ──────────────────────────────────────────────
  txt(cx, ty, "|", (Color){ 80, 80, 80, 255 });
  cx += SEP_W;

  // ── Size label ─────────────────────────────────────────────
  float cur = (g_state->current_tool == TOOL_PEN) ? g_state->tool_pen_size : g_state->tool_eraser_size;
  txt(cx, ty, "Size", (Color){ 180, 180, 180, 255 });
  cx += SIZE_LABEL_W;

  // ── Size value ─────────────────────────────────────────────
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", cur);
  txt(cx, ty, buf, WHITE);
  cx += SIZE_VAL_W + MP_LEAD;

  // ── Minus ──────────────────────────────────────────────────
  DrawRectangle(cx, by, MINUS_W, BTN_H, inactive_bg);
  txt(cx + 12, ty, "-", WHITE);
  update_tooltip("Decrease size", (Rectangle){ cx, by, MINUS_W, BTN_H });
  cx += MINUS_W + MINUS_PLUS_GAP;

  // ── Plus ───────────────────────────────────────────────────
  DrawRectangle(cx, by, PLUS_W, BTN_H, inactive_bg);
  txt(cx + 12, ty, "+", WHITE);
  update_tooltip("Increase size", (Rectangle){ cx, by, PLUS_W, BTN_H });
  cx += PLUS_W + GAP_MD;

  // ── Color swatches ─────────────────────────────────────────
  float sc = swatch_y();
  for (int i = 0; i < 8; i++) {
    Rectangle sw = { cx, sc, SWATCH_SZ, SWATCH_SZ };
    Color    c  = color_presets[i];
    DrawRectangleRec(sw, c);
    bool active = ColorToInt(c) == ColorToInt(g_configuration->draw_color) && c.a == g_configuration->draw_color.a;
    Color border = active ? WHITE : (Color){ 50, 50, 50, 255 };
    DrawRectangleLines(sw.x, sw.y, sw.width, sw.height, border);
    if (active) DrawRectangleLines(sw.x - 1, sw.y - 1, sw.width + 2, sw.height + 2, WHITE);
    update_tooltip("Color", sw);
    cx += SWATCH_SZ + SWATCH_GAP;
  }

  // ── Separator ──────────────────────────────────────────────
  cx += GAP_MD;
  txt(cx, ty, "|", (Color){ 80, 80, 80, 255 });
  cx += SEP_W + GAP_MD;

  // ── Clear All ─────────────────────────────────────────────
  draw_icon_or_text(cx, by, CLEAR_W, trash_tex, "Clear", inactive_bg);
  update_tooltip("Clear all strokes", (Rectangle){ cx, by, CLEAR_W, BTN_H });
  cx += CLEAR_W + CLEAR_FIT_GAP;

  // ── Fit to screen ─────────────────────────────────────────
  draw_icon_or_text(cx, by, FIT_W, fit_tex, "Fit", inactive_bg);
  update_tooltip("Fit image to screen", (Rectangle){ cx, by, FIT_W, BTN_H });
  cx += FIT_W;

  draw_tooltip_if_hovering();
}
