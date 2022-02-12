#include "remote_control.h"
#include "util/buffer_util.h"
#include "util/log.h"
#include <assert.h>

static int run_remote_control(void *data) {
    struct remote_control *remote_control = data;
    uint8_t *buf = malloc(CONTROL_MSG_MAX_SIZE);
    while (true) {
        sc_socket connection_socket = net_accept(remote_control->socket);
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
            assert(control_msg_deserialize(buf, &msg));
            msg.inject_touch_event.buttons = AMOTION_EVENT_BUTTON_PRIMARY;
            msg.inject_touch_event.pointer_id = POINTER_ID_MOUSE;
            msg.inject_touch_event.position.screen_size =
                remote_control->screen_size;
            msg.inject_touch_event.pressure =
                msg.inject_touch_event.action == AMOTION_EVENT_ACTION_DOWN ? 0
                                                                           : 1;
            assert(controller_push_msg(remote_control->controller, &msg));
        }
    }
    free(buf);
    return 0;
}

bool remote_control_init(struct remote_control *remote_control,
                         const struct remote_control_params *params) {
    remote_control->controller = params->controller;
    remote_control->screen_size.width = params->width;
    remote_control->screen_size.height = params->height;
    remote_control->socket = net_socket();
    LOGD("Remote control listen on %d", params->port);
    if (!net_listen(remote_control->socket, IPV4_LOCALHOST, params->port, 2)) {
        return false;
    }
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
