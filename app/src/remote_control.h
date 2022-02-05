#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include <stdbool.h>
#include "controller.h"
#include "input_manager.h"
#include "util/net.h"
#include "util/thread.h"

#define REMOTE_MAX_CONNECTION 1

struct remote_control {
    struct input_manager *input_manager;
    struct controller *controller;
    sc_socket socket;
    sc_thread thread;
};

struct remote_control_params {
    struct input_manager *input_manager;
    struct controller *controller;
    uint16_t port;
};

bool remote_control_init(struct remote_control *remote_control,
                         const struct remote_control_params *params);

bool remote_control_start(struct remote_control *remote_control);

#endif
