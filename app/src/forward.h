#ifndef FORWARD_H
#define FORWARD_H

#include "trait/frame_sink.h"
#include "util/net.h"
#include "util/thread.h"

#define FORWARD_MAX_CONNECTION 2

struct forward {
    struct sc_frame_sink frame_sink;
    sc_socket socket;
    sc_socket connection[FORWARD_MAX_CONNECTION];
    sc_mutex mutex;
    unsigned connection_count;
    sc_thread thread;
};

struct forward_params {
    uint16_t port;
};

bool forward_init(struct forward *forward, const struct forward_params *params);

bool forward_start(struct forward *forward);



#endif
