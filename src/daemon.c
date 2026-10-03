#include "daemon.h"

#include <GLFW/glfw3.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <stdint.h>

#define ROOMER_MAGIC 0x524F4F4D

typedef enum {
  ROOMER_CMD_IMAGE  = 1,
  ROOMER_CMD_QUIT   = 2,
  ROOMER_CMD_PING   = 3,
  ROOMER_CMD_TOGGLE = 4,
} DaemonCmd;

typedef struct {
  uint32_t magic;
  uint32_t cmd;
  float    monitor_scaling;
  uint32_t bg_color;
  uint8_t  transparent;
  uint8_t  _pad[3];
  uint32_t data_len;
} __attribute__((packed)) DaemonHeader;

static volatile sig_atomic_t s_daemon_running = 1;

static void handle_signal(int sig) {
  (void)sig;
  s_daemon_running = 0;
}

static const char* get_socket_path(char* buf, size_t size) {
  const char* xdg = getenv("XDG_RUNTIME_DIR");
  if (xdg && xdg[0]) {
    snprintf(buf, size, "%s/roomer.sock", xdg);
  } else {
    snprintf(buf, size, "/tmp/roomer-%d.sock", (int)getuid());
  }
  return buf;
}

static int daemon_connect(void) {
  char path[256];
  get_socket_path(path, sizeof(path));

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return -1;

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

static ssize_t read_full(int fd, void* buf, size_t len) {
  size_t total = 0;
  char*  p     = (char*)buf;
  while (total < len) {
    ssize_t n = read(fd, p + total, len - total);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) break;
    total += (size_t)n;
  }
  return (ssize_t)total;
}

void daemon_handle_quit_flag(void) {
  int fd = daemon_connect();
  if (fd < 0) {
    fprintf(stderr, "Roomer daemon is not running.\n");
    return;
  }

  DaemonHeader hdr = {
    .magic = ROOMER_MAGIC,
    .cmd   = ROOMER_CMD_QUIT,
  };
  (void)write(fd, &hdr, sizeof(hdr));
  close(fd);
  fprintf(stdout, "Roomer daemon stopped.\n");
}

void daemon_handle_toggle_flag(void) {
  int fd = daemon_connect();
  if (fd < 0) {
    fprintf(stderr, "Roomer daemon is not running.\n");
    return;
  }

  DaemonHeader hdr = {
    .magic = ROOMER_MAGIC,
    .cmd   = ROOMER_CMD_TOGGLE,
  };
  (void)write(fd, &hdr, sizeof(hdr));
  char ack = 0;
  (void)read(fd, &ack, 1);
  close(fd);
}

bool daemon_client_send_image(void) {
  if (isatty(STDIN_FILENO)) return false;

  int fd = daemon_connect();
  if (fd < 0) return false;

  size_t capacity = 1024 * 1024;
  size_t length   = 0;
  unsigned char* buffer = malloc(capacity);
  if (!buffer) { close(fd); return false; }

  char tmp[8192];
  ssize_t n;
  while ((n = read(STDIN_FILENO, tmp, sizeof(tmp))) > 0) {
    if (length + (size_t)n > capacity) {
      size_t new_cap = (capacity * 2 > length + (size_t)n) ? capacity * 2 : length + (size_t)n + 65536;
      unsigned char* p = realloc(buffer, new_cap);
      if (!p) { free(buffer); close(fd); return false; }
      buffer   = p;
      capacity = new_cap;
    }
    memcpy(buffer + length, tmp, n);
    length += (size_t)n;
  }

  if (length == 0) {
    free(buffer);
    close(fd);
    return false;
  }

  uint32_t bg_val = ((uint32_t)g_configuration->background_color.r << 24) |
                    ((uint32_t)g_configuration->background_color.g << 16) |
                    ((uint32_t)g_configuration->background_color.b << 8)  |
                    ((uint32_t)g_configuration->background_color.a);

  DaemonHeader hdr = {
    .magic           = ROOMER_MAGIC,
    .cmd             = ROOMER_CMD_IMAGE,
    .monitor_scaling = g_configuration->monitor_scaling,
    .bg_color        = bg_val,
    .transparent     = g_configuration->transparent_background ? 1 : 0,
    .data_len        = (uint32_t)length,
  };

  if (write(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
    free(buffer);
    close(fd);
    return false;
  }

  size_t sent = 0;
  while (sent < length) {
    ssize_t w = write(fd, buffer + sent, length - sent);
    if (w < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (w == 0) break;
    sent += (size_t)w;
  }

  free(buffer);

  // Wait for 1-byte acknowledgment from daemon
  char ack = 0;
  (void)read(fd, &ack, 1);
  close(fd);

  return (sent == length);
}

int daemon_server_run(void) {
  char path[256];
  get_socket_path(path, sizeof(path));

  int test_fd = daemon_connect();
  if (test_fd >= 0) {
    close(test_fd);
    fprintf(stderr, "Roomer daemon is already running.\n");
    return 1;
  }
  unlink(path);

  int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    return 1;
  }

  int flags = fcntl(listen_fd, F_GETFL, 0);
  if (flags >= 0) fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(listen_fd);
    return 1;
  }

  if (listen(listen_fd, 5) < 0) {
    perror("listen");
    close(listen_fd);
    unlink(path);
    return 1;
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_BORDERLESS_WINDOWED_MODE | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST | FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIDDEN);

  InitWindow(1920, 1080, g_configuration->window_title_roomermode);
  SetExitKey(KEY_NULL);
  SetWindowState(FLAG_WINDOW_HIDDEN);

  tablet_init();

  // Initial placeholder 1x1 blank texture
  Image dummy_img = GenImageColor(1, 1, BLANK);
  Texture2D img_texture = LoadTextureFromImage(dummy_img);
  SetTextureFilter(img_texture, TEXTURE_FILTER_POINT);
  UnloadImage(dummy_img);

  Shader flashlight_shader = LoadShaderFromMemory(NULL, g_flashlight_frag_shader_source);
  int    loc_texture       = GetShaderLocation(flashlight_shader, "texture0");
  int    loc_center        = GetShaderLocation(flashlight_shader, "center");
  int    loc_radius        = GetShaderLocation(flashlight_shader, "radius");
  int    loc_darkness      = GetShaderLocation(flashlight_shader, "darkness");

  bool is_visible = false;
  SetTargetFPS(120);

  fprintf(stdout, "Roomer daemon started (listening on %s)\n", path);

  while (s_daemon_running) {
    struct pollfd pfd = { .fd = listen_fd, .events = POLLIN };
    int timeout_ms = is_visible ? 0 : 50;
    int p = poll(&pfd, 1, timeout_ms);

    if (p > 0 && (pfd.revents & POLLIN)) {
      int client_fd = accept(listen_fd, NULL, NULL);
      if (client_fd >= 0) {
        DaemonHeader hdr;
        if (read_full(client_fd, &hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr) && hdr.magic == ROOMER_MAGIC) {
          if (hdr.cmd == ROOMER_CMD_QUIT) {
            s_daemon_running = 0;
            close(client_fd);
            break;
          }
          if (hdr.cmd == ROOMER_CMD_TOGGLE) {
            if (is_visible) {
              SetWindowState(FLAG_WINDOW_HIDDEN);
              is_visible = false;
              reset_all_variables(false);
            } else {
              ClearWindowState(FLAG_WINDOW_HIDDEN);
              SetWindowFocused();
              is_visible = true;
            }
            char ack = 1;
            (void)write(client_fd, &ack, 1);
            close(client_fd);
            continue;
          }
          if (hdr.cmd == ROOMER_CMD_IMAGE && hdr.data_len > 0) {
            unsigned char* img_buf = malloc(hdr.data_len);
            if (img_buf && read_full(client_fd, img_buf, hdr.data_len) == (ssize_t)hdr.data_len) {
              const char* ext = detect_image_extension(img_buf, hdr.data_len);
              if (ext) {
                Image new_img = LoadImageFromMemory(ext, img_buf, (int)hdr.data_len);
                if (new_img.data != NULL) {
                  UnloadTexture(img_texture);
                  img_texture = LoadTextureFromImage(new_img);
                  SetTextureFilter(img_texture, TEXTURE_FILTER_POINT);

                  g_state->image_w = new_img.width;
                  g_state->image_h = new_img.height;
                  UnloadImage(new_img);

                  if (hdr.monitor_scaling > 0.0f) {
                    g_configuration->monitor_scaling = hdr.monitor_scaling;
                  }
                  g_configuration->transparent_background = (hdr.transparent != 0);

                  int win_w = (int)roundf((float)g_state->image_w / g_configuration->monitor_scaling);
                  int win_h = (int)roundf((float)g_state->image_h / g_configuration->monitor_scaling);
                  SetWindowSize(win_w, win_h);

                  g_state->zoom        = 1.0f / g_configuration->monitor_scaling;
                  g_state->target_zoom = g_state->zoom;
                  g_state->pan         = (Vector2){ 0, 0 };
                  g_state->target_pan  = (Vector2){ 0, 0 };
                  g_state->black_board_enabled = false;
                  draw_clear_all();

                  ClearWindowState(FLAG_WINDOW_HIDDEN);
                  SetWindowFocused();
                  is_visible = true;

                  char ack = 1;
                  (void)write(client_fd, &ack, 1);
                }
              }
            }
            free(img_buf);
          }
        }
        close(client_fd);
      }
    }

    if (!is_visible) {
      PollInputEvents();
      if (WindowShouldClose()) {
        GLFWwindow* win = glfwGetCurrentContext();
        if (win) glfwSetWindowShouldClose(win, GLFW_FALSE);
      }
      continue;
    }

    if (WindowShouldClose() || g_state->should_quit) {
      g_state->should_quit = false;
      GLFWwindow* win = glfwGetCurrentContext();
      if (win) glfwSetWindowShouldClose(win, GLFW_FALSE);
      SetWindowState(FLAG_WINDOW_HIDDEN);
      draw_clear_all();
      is_visible = false;
      continue;
    }

    app_render_frame(img_texture, flashlight_shader, loc_center, loc_radius, loc_darkness, loc_texture);
  }

  draw_cleanup();
  tablet_cleanup();
  UnloadShader(flashlight_shader);
  UnloadTexture(img_texture);
  CloseWindow();
  unlink(path);
  close(listen_fd);

  return 0;
}
