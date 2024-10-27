// Hacked from https://git.sr.ht/~emersion/wlr-randr/tree/master/item/main.c
//
// Useful literature:
// https://wayland.app/protocols/
// https://wayland-book.com/registry/binding.html
// https://git.sr.ht/~emersion/wlr-randr/tree/master/item/main.c

#include "wl_ctrl.h"

// Automatically generated from:
// https://gitlab.freedesktop.org/wlroots/wlr-protocols (see makefile)
#include "wl_protos/wlr-output-management-unstable-v1.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-client.h>

struct wl_ctrl;

// In Wayland terms, a head is a display/screen/something that shows pixels
struct wl_head_info {
  struct zwlr_output_head_v1 *wlr_head;
  struct wl_list link;
  char *name, *description, *make, *model, *serial_number;
  bool enabled;
};

static void printf_wl_head_info(struct wl_head_info *i, const char *msg) {
  printf("%s %s \"%s\"\n", msg, i->name, i->description);
}

struct wl_ctrl {
  struct zwlr_output_manager_v1 *output_manager;
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_list heads;
  uint32_t serial;
  bool has_serial;
  bool running;
  bool failed;
  char *interesting_screen;
};

static void
config_handle_succeeded(void *data,
                        struct zwlr_output_configuration_v1 *config) {
  struct wl_ctrl *state = data;
  zwlr_output_configuration_v1_destroy(config);
  state->running = false;
}

static void config_handle_failed(void *data,
                                 struct zwlr_output_configuration_v1 *config) {
  struct wl_ctrl *state = data;
  zwlr_output_configuration_v1_destroy(config);
  state->running = false;
  state->failed = true;

  fprintf(stderr, "failed to apply configuration\n");
}

static void
config_handle_cancelled(void *data,
                        struct zwlr_output_configuration_v1 *config) {
  struct wl_ctrl *state = data;
  zwlr_output_configuration_v1_destroy(config);
  state->running = false;
  state->failed = true;

  fprintf(stderr, "configuration cancelled, please try again\n");
}

static const struct zwlr_output_configuration_v1_listener config_listener = {
    .succeeded = config_handle_succeeded,
    .failed = config_handle_failed,
    .cancelled = config_handle_cancelled,
};

#define DECL_HEAD_CB_COPY_STR(param)                                           \
  static void head_handle_##param(                                             \
      void *data, struct zwlr_output_head_v1 *_unused __attribute__((unused)), \
      const char *param) {                                                     \
    ((struct wl_head_info *)data)->param = strdup(param);                      \
  }

DECL_HEAD_CB_COPY_STR(name)
DECL_HEAD_CB_COPY_STR(description)

#undef DECL_HEAD_CB_COPY_STR

static void head_handle_enabled(void *data,
                                struct zwlr_output_head_v1 *_unused
                                __attribute__((unused)),
                                int32_t enabled) {
  ((struct wl_head_info *)data)->enabled = !!enabled;
}

static void head_handle_finished(void *data, struct zwlr_output_head_v1 *_unused
                                 __attribute__((unused))) {
  struct wl_head_info *head = data;
  wl_list_remove(&head->link);
  if (zwlr_output_head_v1_get_version(head->wlr_head) >= 3) {
    zwlr_output_head_v1_release(head->wlr_head);
  } else {
    zwlr_output_head_v1_destroy(head->wlr_head);
  }
  free(head->name);
  free(head->description);
  free(head);
}

// wlr_output_management will report a lot of properties about the display we
// don't care about. The signature doesn't match the function wlroots expect, so
// doing anything inside of this function will probably crash.
static void head_handle_ignore() {}

static const struct zwlr_output_head_v1_listener head_listener = {
    // Callback when a head info is "garbage collected"
    .finished = head_handle_finished,

    // Params of a head we care about
    .name = head_handle_name,
    .description = head_handle_description,
    .enabled = head_handle_enabled,

    // Params of a head we don't care about
    .make = head_handle_ignore,
    .model = head_handle_ignore,
    .serial_number = head_handle_ignore,
    .physical_size = head_handle_ignore,
    .mode = head_handle_ignore,
    .current_mode = head_handle_ignore,
    .position = head_handle_ignore,
    .transform = head_handle_ignore,
    .scale = head_handle_ignore,
    .adaptive_sync = head_handle_ignore,
};

static void output_manager_handle_head(void *data,
                                       struct zwlr_output_manager_v1 *_unused
                                       __attribute__((unused)),
                                       struct zwlr_output_head_v1 *wlr_head) {
  struct wl_ctrl *state = data;
  struct wl_head_info *head = calloc(1, sizeof(*head));
  head->wlr_head = wlr_head;
  wl_list_insert(state->heads.prev, &head->link);
  zwlr_output_head_v1_add_listener(wlr_head, &head_listener, head);
}

static void output_manager_handle_done(void *data,
                                       struct zwlr_output_manager_v1 *_unused
                                       __attribute__((unused)),
                                       uint32_t serial) {
  struct wl_ctrl *state = data;
  state->serial = serial;
  state->has_serial = true;
}

static void
output_manager_handle_finished(void *_unused __attribute__((unused)),
                               struct zwlr_output_manager_v1 *_unused2
                               __attribute__((unused))) {
  // This space is intentionally left blank
}

static const struct zwlr_output_manager_v1_listener output_manager_listener = {
    .head = output_manager_handle_head,
    .done = output_manager_handle_done,
    .finished = output_manager_handle_finished,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
  struct wl_ctrl *state = data;

  if (strcmp(interface, zwlr_output_manager_v1_interface.name) == 0) {
    uint32_t version_to_bind = version <= 4 ? version : 4;
    state->output_manager = wl_registry_bind(
        registry, name, &zwlr_output_manager_v1_interface, version_to_bind);
    zwlr_output_manager_v1_add_listener(state->output_manager,
                                        &output_manager_listener, state);
  }
}

static void registry_handle_global_remove(void *_unused1
                                          __attribute__((unused)),
                                          struct wl_registry *_unused2
                                          __attribute__((unused)),
                                          uint32_t _unused3
                                          __attribute__((unused))) {
  // This space is intentionally left blank
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

#define RPI_DEFAULT_WL_DISPLAY_NAME "wayland-1"

struct wl_ctrl *wl_ctrl_init(const char *interesting_screen) {
  struct wl_ctrl *state = malloc(sizeof(struct wl_ctrl));
  if (!state) {
    fprintf(stderr, "bad alloc wl_ctrl_init\n");
    return NULL;
  }

  bzero(state, sizeof(struct wl_ctrl));
  state->running = true;

  if (interesting_screen) {
    state->interesting_screen = strdup(interesting_screen);
    if (!state->interesting_screen) {
      fprintf(stderr, "bad alloc wl_ctrl_init\n");
      free(state);
      return NULL;
    }
  }

  wl_list_init(&state->heads);

  state->display = wl_display_connect(RPI_DEFAULT_WL_DISPLAY_NAME);
  if (state->display == NULL) {
    fprintf(stderr, "failed to connect to display\n");
    wl_ctrl_free(state);
    return NULL;
  }

  state->registry = wl_display_get_registry(state->display);
  wl_registry_add_listener(state->registry, &registry_listener, state);

  if (wl_display_roundtrip(state->display) < 0) {
    fprintf(stderr, "wl_display_roundtrip failed\n");
    wl_ctrl_free(state);
    return NULL;
  }

  if (state->output_manager == NULL) {
    fprintf(stderr, "compositor doesn't support "
                    "wlr-output-management-unstable-v1\n");
    wl_ctrl_free(state);
    return NULL;
  }

  while (!state->has_serial) {
    if (wl_display_dispatch(state->display) < 0) {
      fprintf(stderr, "wl_display_dispatch failed\n");
      wl_ctrl_free(state);
      return NULL;
    }
  }

  bool found_screen = false;
  if (!state->interesting_screen) {
    found_screen = true;
  } else {
    struct wl_head_info *head;
    wl_list_for_each(head, &state->heads, link) {
      if (strcmp(state->interesting_screen, head->name) == 0) {
        found_screen = true;
      }
    }
  }

  if (!found_screen) {
    fprintf(stderr, "Requested non existent display %s.\n",
            state->interesting_screen);
    wl_print_display_names(state);
    wl_ctrl_free(state);
    return NULL;
  }

  return state;
}

void wl_ctrl_free(struct wl_ctrl *state) {
  if (!state) {
    return;
  }

  struct wl_head_info *head, *tmp_head;
  wl_list_for_each_safe(head, tmp_head, &state->heads, link) {
    zwlr_output_head_v1_destroy(head->wlr_head);
    free(head->name);
    free(head->description);
    free(head->make);
    free(head->model);
    free(head->serial_number);
    free(head);
  }

  if (state->output_manager) {
    zwlr_output_manager_v1_destroy(state->output_manager);
  }

  if (state->registry) {
    wl_registry_destroy(state->registry);
  }

  if (state->display) {
    wl_display_disconnect(state->display);
  }

  free(state);
}

static void wl_ctrl_display_onoff(struct wl_ctrl *state, bool shouldTurnOn) {
  struct zwlr_output_configuration_v1 *config =
      zwlr_output_manager_v1_create_configuration(state->output_manager,
                                                  state->serial);
  zwlr_output_configuration_v1_add_listener(config, &config_listener, state);

  struct wl_head_info *head;
  wl_list_for_each(head, &state->heads, link) {
    // TODO wl_list_for_each seems to break unless all operations are applied to
    // all heads
    if (false && state->interesting_screen &&
        strcmp(state->interesting_screen, head->name) != 0) {
      printf_wl_head_info(head, "Ingore");
    } else if (head->enabled && shouldTurnOn) {
      printf_wl_head_info(head, "Display already on");
    } else if (head->enabled && !shouldTurnOn) {
      printf_wl_head_info(head, "Will shutdown");
      zwlr_output_configuration_v1_disable_head(config, head->wlr_head);
      head->enabled = false;
    } else if (!head->enabled && shouldTurnOn) {
      printf_wl_head_info(head, "Will power on");
      struct zwlr_output_configuration_head_v1 *config_head =
          zwlr_output_configuration_v1_enable_head(config, head->wlr_head);
      zwlr_output_configuration_head_v1_destroy(config_head);
      head->enabled = true;
    } else if (!head->enabled && !shouldTurnOn) {
      printf_wl_head_info(head, "Display already off");
    }
  }

  state->running = true;
  zwlr_output_configuration_v1_apply(config);
  while (state->running && wl_display_dispatch(state->display) != -1) {
    // This space intentionally left blank
  }
}

void wl_ctrl_display_off(struct wl_ctrl *state) {
  wl_ctrl_display_onoff(state, false);
}

void wl_ctrl_display_on(struct wl_ctrl *state) {
  wl_ctrl_display_onoff(state, true);
}

void wl_print_display_names(struct wl_ctrl *state) {
  struct wl_head_info *head;
  printf("Available displays:\n");
  wl_list_for_each(head, &state->heads, link) {
    printf_wl_head_info(head, " - ");
  }
}

enum WlCtrlDisplayState
wl_ctrl_display_query_state(struct wl_ctrl *display_state) {
  enum WlCtrlDisplayState state = WL_CTRL_DISPLAYS_UNKNOWN;
  struct wl_head_info *head;
  wl_list_for_each(head, &display_state->heads, link) {
    if (!display_state->interesting_screen ||
        strcmp(display_state->interesting_screen, head->name) != 0) {
      const enum WlCtrlDisplayState head_state =
          head->enabled ? WL_CTRL_DISPLAYS_ON : WL_CTRL_DISPLAYS_OFF;
      if (state == WL_CTRL_DISPLAYS_UNKNOWN) {
        state = head_state;
      } else if (state != head_state ||
                 state == WL_CTRL_DISPLAYS_INCONSISTENT) {
        state = WL_CTRL_DISPLAYS_INCONSISTENT;
      }
    }
  };
  return state;
}
