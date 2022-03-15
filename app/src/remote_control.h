#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include <stdbool.h>
#include "controller.h"
#include "input_manager.h"
#include "util/net.h"
#include "util/thread.h"
#include "trait/frame_sink.h"

#define REMOTE_MAX_CONNECTION 1

struct remote_control {
    struct sc_size screen_size;
    struct controller *controller;
    sc_socket socket;
    sc_thread thread;
    struct sc_frame_sink frame_sink;
};

struct remote_control_params {
    uint16_t width;
    uint16_t height;
    struct controller *controller;
    uint16_t port;
};

bool remote_control_init(struct remote_control *remote_control,
                         const struct remote_control_params *params);

bool remote_control_start(struct remote_control *remote_control);

#endif
