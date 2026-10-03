#include "roomer.h"
#include "daemon.h"

// clang-format off
const char* g_flashlight_frag_shader_source =
  "#version 330 core\n"
  "in vec2 fragTexCoord;\n"
  "in vec4 fragColor;\n"
  "out vec4 finalColor;\n"
  "uniform sampler2D texture0;\n"
  "uniform vec2 center;\n"
  "uniform float radius;\n"
  "uniform float darkness;\n"
  "void main(void)\n"
  "{\n"
  "    vec4 texel = texture(texture0, fragTexCoord);\n"
  "    vec4 color = texel * fragColor;\n"
  "    vec2 delta = gl_FragCoord.xy - center;\n"
  "    if (dot(delta, delta) > radius * radius) {\n"
  "        color.rgb *= darkness;\n"
  "    }\n"
  "    finalColor = color;\n"
  "}\n";
// clang-format on

void app_render_frame(Texture2D img_texture, Shader flashlight_shader, int loc_center, int loc_radius, int loc_darkness, int loc_texture) {
  handle_inputs();

  float dt       = GetFrameTime();
  float z_speed  = 1.0F - expf(-15.0F * dt);
  g_state->zoom  = Lerp(g_state->zoom, g_state->target_zoom, z_speed);
  g_state->pan.x = Lerp(g_state->pan.x, g_state->target_pan.x, z_speed);
  g_state->pan.y = Lerp(g_state->pan.y, g_state->target_pan.y, z_speed);

  // Smooth scroll: always keep flashlight_radius chasing target_flashlight_radius
  float r_smooth             = 1.0F - expf(-15.0F * dt);
  g_state->flashlight_radius = Lerp(g_state->flashlight_radius, g_state->target_flashlight_radius, r_smooth);

  // Flashlight on/off animation
  {
    float r_fast = 1.0F - expf(-15.0F * dt);
    float r_slow = 1.0F - expf(-20.0F * dt);
    float a_slow = 1.0F - expf(-10.0F * dt);

    float big = g_state->flashlight_radius + 250.0F;

    if (!g_state->flashlight_prev_enabled && g_state->flashlight_enabled) {
      g_state->flashlight_display_radius = big;
      g_state->flashlight_darkness       = 1.0F;
      g_state->flashlight_rendering      = true;
    }

    if (g_state->flashlight_rendering) {
      if (g_state->flashlight_enabled) {
        float speed                        = g_state->flashlight_display_radius > g_state->flashlight_radius ? r_fast : r_slow;
        g_state->flashlight_display_radius = Lerp(g_state->flashlight_display_radius, g_state->flashlight_radius, speed);
        g_state->flashlight_darkness       = Lerp(g_state->flashlight_darkness, 0.1F, a_slow);
      } else {
        g_state->flashlight_display_radius = Lerp(g_state->flashlight_display_radius, big, r_slow);
        g_state->flashlight_darkness       = Lerp(g_state->flashlight_darkness, 1.0F, a_slow);
        if (g_state->flashlight_display_radius >= big - 2.0F && g_state->flashlight_darkness >= 0.95F) {
          g_state->flashlight_display_radius = g_state->flashlight_radius;
          g_state->flashlight_darkness       = 0.1F;
          g_state->flashlight_rendering      = false;
        }
      }
    }

    g_state->flashlight_prev_enabled = g_state->flashlight_enabled;
  }

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  DrawLayer active_layer = g_state->black_board_enabled ? LAYER_BLACKBOARD : LAYER_IMAGE;

  static float   s_last_zoom = 0;
  static Vector2 s_last_pan  = { 0 };
  static bool    s_last_bb   = false;
  bool view_changed = (g_state->zoom != s_last_zoom) ||
                      (g_state->pan.x != s_last_pan.x) ||
                      (g_state->pan.y != s_last_pan.y) ||
                      (g_state->black_board_enabled != s_last_bb);

  s_last_zoom = g_state->zoom;
  s_last_pan  = g_state->pan;
  s_last_bb   = g_state->black_board_enabled;

  draw_render_highlighter(active_layer, view_changed);

  BeginDrawing();
  Color bg = g_configuration->background_color;
  if (!g_configuration->transparent_background) bg.a = 255;
  ClearBackground(bg);

  if (g_state->flashlight_rendering) {
    Vector2 mouse_pos     = GetMousePosition();
    float   u_center[2]   = { mouse_pos.x, (float)sh - mouse_pos.y };
    float   u_radius[1]   = { g_state->flashlight_display_radius };
    float   u_darkness[1] = { g_state->flashlight_darkness };
    int     u_texture[1]  = { 0 };
    SetShaderValue(flashlight_shader, loc_center, u_center, SHADER_UNIFORM_VEC2);
    SetShaderValue(flashlight_shader, loc_radius, u_radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(flashlight_shader, loc_darkness, u_darkness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(flashlight_shader, loc_texture, u_texture, SHADER_UNIFORM_INT);

    BeginShaderMode(flashlight_shader);
  }

  if (g_state->black_board_enabled) {
    DrawRectangle(0, 0, sw, sh, BLACK);
    draw_dot_grid(sw, sh, g_state->pan, g_state->zoom);
    draw_layer_normal(LAYER_BLACKBOARD);
  } else {
    DrawTextureEx(img_texture, g_state->pan, 0.0f, g_state->zoom, WHITE);
    draw_layer_normal(LAYER_IMAGE);
  }

  if (g_state->flashlight_rendering) EndShaderMode();

  draw_composite_highlighter();

  toolbox_render();
  keymaps_render();
  draw_size_indicator();
  EndDrawing();
}

int main(int argc, char** argv) {
  process_commandline_arguments(argc, argv);

  if (g_configuration->quit_daemon) {
    daemon_handle_quit_flag();
    return 0;
  }

  if (g_configuration->toggle_daemon) {
    daemon_handle_toggle_flag();
    return 0;
  }

  if (g_configuration->daemon_mode) {
    return daemon_server_run();
  }

  if (!g_configuration->no_daemon && daemon_client_send_image()) {
    return 0;
  }

  SetTraceLogLevel(LOG_INFO);

  bool  was_file;
  Image img = load_image_from_stdin(&was_file);
  if (memcmp(&img, &(Image){ 0 }, sizeof(Image)) == 0) return EXIT_FAILURE;

  if (was_file) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(g_configuration->window_width, g_configuration->window_height, g_configuration->window_title_imagemode);
  } else {
    SetConfigFlags(FLAG_BORDERLESS_WINDOWED_MODE | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST | FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE);

    int window_width     = (int)roundf((float)img.width / g_configuration->monitor_scaling);
    int window_height    = (int)roundf((float)img.height / g_configuration->monitor_scaling);
    g_state->zoom        = 1 / g_configuration->monitor_scaling;
    g_initial_state.zoom = g_state->zoom;

    InitWindow(window_width, window_height, g_configuration->window_title_roomermode);
  }

  g_state->image_w = img.width;
  g_state->image_h = img.height;

  tablet_init();

  Texture2D img_texture = LoadTextureFromImage(img);
  SetTextureFilter(img_texture, TEXTURE_FILTER_POINT);
  UnloadImage(img);

  Shader flashlight_shader = LoadShaderFromMemory(NULL, g_flashlight_frag_shader_source);
  int    loc_texture       = GetShaderLocation(flashlight_shader, "texture0");
  int    loc_center        = GetShaderLocation(flashlight_shader, "center");
  int    loc_radius        = GetShaderLocation(flashlight_shader, "radius");
  int    loc_darkness      = GetShaderLocation(flashlight_shader, "darkness");

  SetTargetFPS(120);
  while (!WindowShouldClose() && !g_state->should_quit) {
    app_render_frame(img_texture, flashlight_shader, loc_center, loc_radius, loc_darkness, loc_texture);
  }

  draw_clear_all();
  draw_cleanup();
  tablet_cleanup();
  UnloadShader(flashlight_shader);
  UnloadTexture(img_texture);
  CloseWindow();
  return 0;
}
