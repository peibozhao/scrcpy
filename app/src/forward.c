#include "forward.h"
#include "util/buffer_util.h"
#include "util/log.h"
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <netinet/in.h>
#include <stdio.h>

#define DOWNCAST(SINK) container_of(SINK, struct forward, frame_sink)

static bool forward_frame_sink_open(struct sc_frame_sink *sink) {
    (void)sink;
    return true;
}

static bool forward_frame_sink_push(struct sc_frame_sink *sink,
                                    const AVFrame *frame) {
    struct forward *forward = DOWNCAST(sink);
    uint32_t plane_size[3] = {frame->height * frame->linesize[0],
                              frame->height / 2 * frame->linesize[1],
                              frame->height / 2 * frame->linesize[2]};

    sc_mutex_lock(&forward->mutex);
    for (unsigned connection_idx = 0;
         connection_idx < forward->connection_count;) {
        sc_socket connection = forward->connection[connection_idx];

        uint32_t header_size = sizeof(uint32_t);
        uint32_t buf_size = header_size;
        for (unsigned i = 0; i < sizeof(plane_size) / sizeof(plane_size[0]); ++i) {
            buf_size += plane_size[i];
        }
        uint8_t *buffer = (uint8_t *)malloc(buf_size);
        uint32_t write_idx = 0;
        buffer_write32be(&buffer[write_idx], buf_size - header_size);  // Frame size
        write_idx += 4;

        for (unsigned i = 0; i < sizeof(plane_size) / sizeof(plane_size[0]); ++i) {
            memcpy(&buffer[write_idx], frame->data[i], plane_size[i]);
            write_idx += plane_size[i];
        }
        buffer_write16be(&buffer[header_size], frame->linesize[0]);
        buffer_write16be(&buffer[header_size + 2], frame->height);
        ssize_t send_len = net_send_all(connection, buffer, buf_size);
        free(buffer);
        if (send_len < 0) {
            LOGI("Forward send failed. %d", errno);
            net_close(connection);
            for (unsigned i = connection_idx + 1; i < forward->connection_count;
                 ++i) {
                forward->connection[i - 1] = forward->connection[i];
            }
            forward->connection_count -= 1;
            LOGI("Forward close connect");
            break;
        } else {
            connection_idx += 1;
        }
    }
    sc_mutex_unlock(&forward->mutex);

    return true;
}

static void forward_frame_sink_close(struct sc_frame_sink *sink) {
    struct forward *forward = DOWNCAST(sink);
    sc_mutex_lock(&forward->mutex);
    for (unsigned connetion_idx = 0; connetion_idx < forward->connection_count;
         ++connetion_idx) {
        net_close(forward->connection[connetion_idx]);
    }
    sc_mutex_unlock(&forward->mutex);
}

static int run_forward(void *data) {
    struct forward *forward = data;
    while (true) {
        sc_socket connection_socket = net_accept(forward->socket);
        sc_mutex_lock(&forward->mutex);
        forward->connection[forward->connection_count] = connection_socket;
        forward->connection_count += 1;
        LOGI("Forward accept connect");
        sc_mutex_unlock(&forward->mutex);
    }
    return 0;
}

bool forward_init(struct forward *forward,
                  const struct forward_params *params) {
    static const struct sc_frame_sink_ops ops = {
        .open = forward_frame_sink_open,
        .push = forward_frame_sink_push,
        .close = forward_frame_sink_close,
    };

    forward->frame_sink.ops = &ops;
    forward->socket = net_socket();
    LOGD("Forward listen on %d", params->port);
    if (!net_listen(forward->socket, IPV4_LOCALHOST, params->port, 2)) {
        return false;
    }
    forward->connection_count = 0;
    assert(sc_mutex_init(&forward->mutex));
    return true;
}

bool forward_start(struct forward *forward) {
    LOGD("Forward thread running");
    if (!sc_thread_create(&forward->thread, run_forward, "forward", forward)) {
        return false;
    }

    return true;
}
