#pragma once

#include <stdbool.h>

typedef struct {
  bool  present;
  float pressure;
  bool  touching;
  bool  button1;
  bool  button2;
  bool  button3;
  bool  pen_just_pressed;
  bool  active;
} TabletState;

extern TabletState g_tablet;

void tablet_init(void);
void tablet_poll(void);
void tablet_cleanup(void);
