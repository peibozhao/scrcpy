#include "remote_control.h"
#include "util/buffer_util.h"
#include "util/log.h"
#include <assert.h>
#include <netinet/in.h>

#define DOWNCAST(SINK) container_of(SINK, struct remote_control, frame_sink)

static int run_remote_control(void *data) {
    struct remote_control *remote_control = data;
    uint8_t *buf = malloc(CONTROL_MSG_MAX_SIZE);
    while (true) {
        sc_socket connection_socket = net_accept(remote_control->socket);
        LOGI("Remote control accept connect");
        while (true) {
            ssize_t recv_len = 0;
            recv_len = net_recv_all(connection_socket, buf, 4);
            if (recv_len <= 0) {
                net_close(connection_socket);
                LOGI("Remote control receive message length %ld", recv_len);
                break;
            }
            uint16_t msg_len = buffer_read32be(buf);
            recv_len = net_recv_all(connection_socket, buf, msg_len);
            if (recv_len <= 0) {
                net_close(connection_socket);
                LOGI("Remote control receive message length %ld", recv_len);
                break;
            }

            struct control_msg msg;
            control_msg_deserialize(buf, &msg);
            msg.inject_touch_event.buttons = AMOTION_EVENT_BUTTON_PRIMARY;
            msg.inject_touch_event.pointer_id = POINTER_ID_MOUSE;
            msg.inject_touch_event.position.screen_size =
                remote_control->screen_size;
            msg.inject_touch_event.pressure =
                msg.inject_touch_event.action == AMOTION_EVENT_ACTION_DOWN ? 0
                                                                           : 1;
            controller_push_msg(remote_control->controller, &msg);
        }
    }
    free(buf);
    return 0;
}

static bool remote_frame_sink_open(struct sc_frame_sink *sink) {
    (void)sink;
    return true;
}

static bool remote_frame_sink_push(struct sc_frame_sink *sink, const AVFrame *frame) {
    struct remote_control *remote_control = DOWNCAST(sink);
    remote_control->screen_size.width = frame->width;
    remote_control->screen_size.height = frame->height;
    return true;
}

static void remote_frame_sink_close(struct sc_frame_sink *sink) {
    (void)sink;
}

bool remote_control_init(struct remote_control *remote_control,
                         const struct remote_control_params *params) {
    remote_control->controller = params->controller;
    remote_control->screen_size.width = params->width;
    remote_control->screen_size.height = params->height;
    remote_control->socket = net_socket();
    LOGD("Remote control listen on %d", params->port);
    if (!net_listen(remote_control->socket, INADDR_ANY, params->port, 2)) {
        return false;
    }
    static const struct sc_frame_sink_ops ops = {
        .open = remote_frame_sink_open,
        .push = remote_frame_sink_push,
        .close = remote_frame_sink_close,
    };
    remote_control->frame_sink.ops = &ops;
    return true;
}

bool remote_control_start(struct remote_control *remote_control) {
    LOGD("Remote control thread running");
    if (!sc_thread_create(&remote_control->thread, run_remote_control,
                          "remote_control", remote_control)) {
        return false;
    }
    return true;
}
