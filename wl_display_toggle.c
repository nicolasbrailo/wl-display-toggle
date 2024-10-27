#include "wl_ctrl.h"

#include <stdio.h>
#include <string.h>

enum RequestedAction {
  INVALID_RQ_ACTION,
  TURN_ON_ACTION,
  TURN_OFF_ACTION,
};

int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("Usage: %s [[--on|on|1] | [--off|off|0]]\n", argv[0]);
    return 0;
  }

  enum RequestedAction act = INVALID_RQ_ACTION;
  if ((strcmp(argv[1], "--on") == 0) || (strcmp(argv[1], "on") == 0) ||
      (strcmp(argv[1], "1")) == 0) {
    act = TURN_ON_ACTION;
  } else if ((strcmp(argv[1], "--off") == 0) || (strcmp(argv[1], "off") == 0) ||
             (strcmp(argv[1], "0") == 0)) {
    act = TURN_OFF_ACTION;
  }

  if (act == INVALID_RQ_ACTION) {
    printf("%s is not a valid state, expected [--on|on|1] or [--off|off|0]\n",
           argv[1]);
    return 1;
  }

  struct wl_ctrl *display = wl_ctrl_init(NULL);
  if (!display) {
    return 1;
  }

  switch (wl_ctrl_display_query_state(display)) {
  case WL_CTRL_DISPLAYS_UNKNOWN: // fallthrough
  case WL_CTRL_DISPLAYS_INCONSISTENT:
    printf("Can't find display state\n");
    break;
  case WL_CTRL_DISPLAYS_ON:
    if (act == TURN_ON_ACTION) {
      printf("Display is already on, nothing to do\n");
      return 0;
    }
    break;
  case WL_CTRL_DISPLAYS_OFF:
    if (act == TURN_OFF_ACTION) {
      printf("Display is already off, nothing to do\n");
      return 0;
    }
    break;
  }

  if (act == TURN_ON_ACTION) {
    printf("Turn display on.\n");
    wl_ctrl_display_on(display);
  } else {
    printf("Turn display off.\n");
    wl_ctrl_display_off(display);
  }

  wl_ctrl_free(display);

  return 0;
}
