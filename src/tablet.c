#include "roomer.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>

TabletState g_tablet = { 0 };

static int   s_fd            = -1;
static int   s_pressure_max  = 0;
static bool  s_prev_pressed  = false;

// ── helpers ─────────────────────────────────────────────────

static bool has_abs_pressure(int fd) {
  unsigned long bits[ABS_CNT / (sizeof(unsigned long) * 8) + 1] = { 0 };
  if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bits)), bits) < 0) return false;
  return (bits[ABS_PRESSURE / (sizeof(unsigned long) * 8)] >> (ABS_PRESSURE % (sizeof(unsigned long) * 8))) & 1UL;
}

static bool has_key(int fd, int code) {
  unsigned long bits[KEY_CNT / (sizeof(unsigned long) * 8) + 1] = { 0 };
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) return false;
  return (bits[code / (sizeof(unsigned long) * 8)] >> (code % (sizeof(unsigned long) * 8))) & 1UL;
}

static int get_abs_max(int fd, int axis) {
  struct input_absinfo info;
  if (ioctl(fd, EVIOCGABS(axis), &info) < 0) return 0;
  return info.maximum;
}

// ── public ──────────────────────────────────────────────────

void tablet_init(void) {
  for (int i = 0; i < 64; i++) {
    char path[64];
    int  n = snprintf(path, sizeof(path), "/dev/input/event%d", i);
    if (n < 0 || n >= (int)sizeof(path)) break;

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) continue;

    bool ok = has_abs_pressure(fd) && has_key(fd, BTN_TOOL_PEN);
    if (ok) {
      s_pressure_max = get_abs_max(fd, ABS_PRESSURE);
      s_fd           = fd;
      g_tablet.present = true;
      TraceLog(LOG_INFO, "Tablet: found device %s (pressure max %d)", path, s_pressure_max);
      return;
    }
    close(fd);
  }
  TraceLog(LOG_INFO, "Tablet: no device found");
}

void tablet_poll(void) {
  if (s_fd < 0) return;

  g_tablet.pen_just_pressed = false;

  struct input_event ev;
  int read_errno = 0;

  while (1) {
    ssize_t n = read(s_fd, &ev, sizeof(ev));
    if (n != (ssize_t)sizeof(ev)) {
      if (n < 0) read_errno = errno;
      break;
    }

    switch (ev.type) {
      case EV_KEY:
        switch (ev.code) {
          case BTN_TOUCH:    g_tablet.touching = ev.value; break;
          case 0x141:        g_tablet.button3  = ev.value; break;
          case BTN_STYLUS:   g_tablet.button1  = ev.value; break;
          case BTN_STYLUS2:  g_tablet.button2  = ev.value; break;
          case BTN_TOOL_PEN: g_tablet.active   = ev.value; break;
        }
        break;
      case EV_ABS:
        if (ev.code == ABS_PRESSURE && s_pressure_max > 0) {
          g_tablet.pressure = (float)ev.value / (float)s_pressure_max;
          if (g_tablet.pressure > 1.0F) g_tablet.pressure = 1.0F;
        }
        break;
    }
  }

  if (read_errno != 0 && read_errno != EAGAIN && read_errno != EWOULDBLOCK) {
    close(s_fd);
    s_fd = -1;
    memset(&g_tablet, 0, sizeof(g_tablet));
    return;
  }

  bool pressed = g_tablet.touching || g_tablet.pressure > 0.01f;
  if (pressed && !s_prev_pressed) g_tablet.pen_just_pressed = true;
  s_prev_pressed = pressed;
}

void tablet_cleanup(void) {
  if (s_fd >= 0) {
    close(s_fd);
    s_fd = -1;
  }
  memset(&g_tablet, 0, sizeof(g_tablet));
}
