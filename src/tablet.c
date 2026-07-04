#include "roomer.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

// Global state read by the rest of the application
TabletState g_tablet = { 0 };

static int s_fd            = -1;
static int s_pressure_max  = 0;
static int s_abs_x_max     = 0;
static int s_abs_y_max     = 0;
static bool s_has_distance = false;

// Shadow state for accumulating evdev events until SYN_REPORT
static TabletState s_pending = { 0 };
static double s_last_physical_touch_time = 0.0;

// 50ms debounce forgives hardware dropping the signal briefly 
// without making pen lifts feel laggy.
#define PEN_UP_DEBOUNCE_SEC 0.050 

// --- Helper Functions ---

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static bool has_abs_axis(int fd, int axis) {
    unsigned long bits[ABS_CNT / (sizeof(unsigned long) * 8) + 1] = { 0 };
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bits)), bits) < 0) return false;
    return (bits[axis / (sizeof(unsigned long) * 8)] >> (axis % (sizeof(unsigned long) * 8))) & 1UL;
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

// --- Core API ---

void tablet_init(void) {
    for (int i = 0; i < 64; i++) {
        char path[64];
        int n = snprintf(path, sizeof(path), "/dev/input/event%d", i);
        if (n < 0 || n >= (int)sizeof(path)) break;

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        // Verify it's a graphics tablet
        bool ok = has_abs_axis(fd, ABS_PRESSURE) && has_key(fd, BTN_TOOL_PEN);
        if (ok) {
            s_pressure_max = get_abs_max(fd, ABS_PRESSURE);
            s_abs_x_max    = get_abs_max(fd, ABS_X);
            s_abs_y_max    = get_abs_max(fd, ABS_Y);
            s_has_distance = has_abs_axis(fd, ABS_DISTANCE);
            s_fd           = fd;
            g_tablet.present  = true;
            g_tablet.abs_x_max = s_abs_x_max;
            g_tablet.abs_y_max = s_abs_y_max;
            
            TraceLog(LOG_INFO, "Tablet: found device %s (pressure max %d)", path, s_pressure_max);
            return;
        }
        close(fd);
    }
    TraceLog(LOG_INFO, "Tablet: no device found");
}

void tablet_poll(void) {
    if (s_fd < 0) return;

    // Reset the one-frame trigger
    g_tablet.pen_just_pressed = false;

    struct input_event ev;
    int read_errno = 0;
    bool syn_received = false;

    // 1. Accumulate events into the pending shadow state
    while (1) {
        ssize_t n = read(s_fd, &ev, sizeof(ev));
        if (n != (ssize_t)sizeof(ev)) {
            if (n < 0) read_errno = errno;
            break;
        }

        switch (ev.type) {
            case EV_SYN:
                if (ev.code == SYN_REPORT) {
                    syn_received = true;
                }
                break;
            case EV_KEY:
                switch (ev.code) {
                    case BTN_TOUCH:    s_pending.touching = ev.value; break;
                    case 0x141:        s_pending.button3  = ev.value; break;
                    case BTN_STYLUS:   s_pending.button1  = ev.value; break;
                    case BTN_STYLUS2:  s_pending.button2  = ev.value; break;
                    case BTN_TOOL_PEN: s_pending.active   = ev.value; break;
                }
                break;
            case EV_ABS:
                if (ev.code == ABS_PRESSURE && s_pressure_max > 0) {
                    s_pending.pressure = (float)ev.value / (float)s_pressure_max;
                    if (s_pending.pressure > 1.0F) s_pending.pressure = 1.0F;
                } else if (ev.code == ABS_X) {
                    s_pending.abs_x = ev.value;
                } else if (ev.code == ABS_Y) {
                    s_pending.abs_y = ev.value;
                } else if (ev.code == ABS_DISTANCE) {
                    s_pending.distance = ev.value;
                }
                break;
        }
    }

    // Handle disconnects or fatal read errors
    if (read_errno != 0 && read_errno != EAGAIN && read_errno != EWOULDBLOCK) {
        TraceLog(LOG_WARNING, "Tablet: device disconnected or read error");
        close(s_fd);
        s_fd = -1;
        memset(&g_tablet, 0, sizeof(g_tablet));
        memset(&s_pending, 0, sizeof(s_pending));
        return;
    }

    // 2. Only update application state if a full hardware frame was received
    if (syn_received) {
        double now = get_time_sec();
        
        // Combine sensors to catch flaky hardware reports.
        // distance == 0 means pen touches surface, but only use it if device reports it.
        bool physical_touch = s_pending.touching || 
                              s_pending.pressure > 0.01f || 
                              (s_has_distance && s_pending.distance == 0 && s_pending.active);

        if (s_pending.active) {
            if (physical_touch) {
                // Pen is down, refresh the latch timer.
                s_last_physical_touch_time = now;
                s_pending.logical_pen_down = true;
            } else {
                // Hardware says pen is up. Has it been up long enough to trust it?
                if (now - s_last_physical_touch_time > PEN_UP_DEBOUNCE_SEC) {
                    s_pending.logical_pen_down = false;
                }
            }
        } else {
            // Pen left proximity entirely, force release immediately.
            s_pending.logical_pen_down = false;
        }

        // Detect fresh press for the application layer
        if (s_pending.logical_pen_down && !g_tablet.logical_pen_down) {
            g_tablet.pen_just_pressed = true;
        }

        // Commit pending state to the global application state
        g_tablet.active   = s_pending.active;
        g_tablet.touching = s_pending.touching;
        g_tablet.pressure = s_pending.pressure;
        g_tablet.abs_x    = s_pending.abs_x;
        g_tablet.abs_y    = s_pending.abs_y;
        g_tablet.distance = s_pending.distance;
        g_tablet.logical_pen_down = s_pending.logical_pen_down;
        g_tablet.button1  = s_pending.button1;
        g_tablet.button2  = s_pending.button2;
        g_tablet.button3  = s_pending.button3;
    }
}

void tablet_cleanup(void) {
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
    memset(&g_tablet, 0, sizeof(g_tablet));
    memset(&s_pending, 0, sizeof(s_pending));
}
