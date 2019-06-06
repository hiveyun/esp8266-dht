// Copyright 2017 Bose Corporation.
// This software is released under the 3-Clause BSD License.
// The license can be viewed at https://github.com/smudgelang/smudge/blob/master/LICENSE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "queue.h"
#include "dht_fsm.h"
#include "dht_fsm_ext.h"

typedef enum {
    BUTTON,
    MQTT,
    NET,
    DHT,
    LED,
    SMARTCONFIG,
    SENSOR,
} sm_id_t;

typedef struct {
    sm_id_t sm;
    union {
        button_Event_Wrapper button;
        mqtt_Event_Wrapper mqtt;
        net_Event_Wrapper net;
        dht_Event_Wrapper dht;
        led_Event_Wrapper led;
        smartconfig_Event_Wrapper smartconfig;
        sensor_Event_Wrapper sensor;
    } wrapper;
} system_message_t;

static queue_t *q;

void flushEventQueue(void) {
    // This function could be running in a parallel thread
    // concurrently with the rest of the system. The important thing
    // is that it pops messages off the queue and sends them to
    // xxx_Handle_Message.
    bool success;
    system_message_t *msg;

    while(size(q) > 0) {
        success = dequeue(q, (void **)&msg);
        if (!success) {
            fprintf(stderr, "Failed to dequeue element.\n");
            exit(-1);
        }
        switch(msg->sm) {
        case BUTTON:
            // This actually sends the event into the state machine.
            button_Handle_Message(msg->wrapper.button);

            // This frees the event payload that's within the wrapper by
            // calling SMUDGE_free on it.
            button_Free_Message(msg->wrapper.button);
            break;
        case MQTT:
            mqtt_Handle_Message(msg->wrapper.mqtt);
            mqtt_Free_Message(msg->wrapper.mqtt);
            break;
        case NET:
            net_Handle_Message(msg->wrapper.net);
            net_Free_Message(msg->wrapper.net);
            break;
        case DHT:
            dht_Handle_Message(msg->wrapper.dht);
            dht_Free_Message(msg->wrapper.dht);
            break;
        case LED:
            led_Handle_Message(msg->wrapper.led);
            led_Free_Message(msg->wrapper.led);
            break;
        case SMARTCONFIG:
            smartconfig_Handle_Message(msg->wrapper.smartconfig);
            smartconfig_Free_Message(msg->wrapper.smartconfig);
            break;
        case SENSOR:
            sensor_Handle_Message(msg->wrapper.sensor);
            sensor_Free_Message(msg->wrapper.sensor);
            break;
        }
        // We still need to free the copy of the wrapper itself, since
        // it was malloc'd in turnstile_Send_Message.
        free(msg);
    }
}

void button_Send_Message(button_Event_Wrapper e) {
    bool success;
    system_message_t *msg;
    button_Event_Wrapper *wrapper;

    // The event wrapper is passed in on the stack, so we have to
    // allocate some memory that we can put in the message queue.
    msg = malloc(sizeof(system_message_t));
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate message memory.\n");
        exit(-1);
    }
    msg->sm = BUTTON;
    wrapper = &msg->wrapper.button;
    memcpy(wrapper, &e, sizeof(e));

    // Put the event on the queue, to be popped off later and handled
    // in order.
    success = enqueue(q, msg);
    if (!success) {
        fprintf(stderr, "Failed to enqueue message.\n");
        exit(-1);
    }
}

void mqtt_Send_Message(mqtt_Event_Wrapper e) {
    bool success;
    system_message_t *msg;
    mqtt_Event_Wrapper *wrapper;

    // The event wrapper is passed in on the stack, so we have to
    // allocate some memory that we can put in the message queue.
    msg = malloc(sizeof(system_message_t));
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate message memory.\n");
        exit(-1);
    }
    msg->sm = MQTT;
    wrapper = &msg->wrapper.mqtt;
    memcpy(wrapper, &e, sizeof(e));

    // Put the event on the queue, to be popped off later and handled
    // in order.
    success = enqueue(q, msg);
    if (!success) {
        fprintf(stderr, "Failed to enqueue message.\n");
        exit(-1);
    }
}

void net_Send_Message(net_Event_Wrapper e) {
    bool success;
    system_message_t *msg;
    net_Event_Wrapper *wrapper;

    // The event wrapper is passed in on the stack, so we have to
    // allocate some memory that we can put in the message queue.
    msg = malloc(sizeof(system_message_t));
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate message memory.\n");
        exit(-1);
    }
    msg->sm = NET;
    wrapper = &msg->wrapper.net;
    memcpy(wrapper, &e, sizeof(e));

    // Put the event on the queue, to be popped off later and handled
    // in order.
    success = enqueue(q, msg);
    if (!success) {
        fprintf(stderr, "Failed to enqueue message.\n");
        exit(-1);
    }
}

void dht_Send_Message(dht_Event_Wrapper e) {
    bool success;
    system_message_t *msg;
    dht_Event_Wrapper *wrapper;

    // The event wrapper is passed in on the stack, so we have to
    // allocate some memory that we can put in the message queue.
    msg = malloc(sizeof(system_message_t));
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate message memory.\n");
        exit(-1);
    }
    msg->sm = DHT;
    wrapper = &msg->wrapper.dht;
    memcpy(wrapper, &e, sizeof(e));

    // Put the event on the queue, to be popped off later and handled
    // in order.
    success = enqueue(q, msg);
    if (!success) {
        fprintf(stderr, "Failed to enqueue message.\n");
        exit(-1);
    }
}

void led_Send_Message(led_Event_Wrapper e) {
    bool success;
    system_message_t *msg;
    led_Event_Wrapper *wrapper;

    // The event wrapper is passed in on the stack, so we have to
    // allocate some memory that we can put in the message queue.
    msg = malloc(sizeof(system_message_t));
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate message memory.\n");
        exit(-1);
    }
    msg->sm = LED;
    wrapper = &msg->wrapper.led;
    memcpy(wrapper, &e, sizeof(e));

    // Put the event on the queue, to be popped off later and handled
    // in order.
    success = enqueue(q, msg);
    if (!success) {
        fprintf(stderr, "Failed to enqueue message.\n");
        exit(-1);
    }
}

void smartconfig_Send_Message(smartconfig_Event_Wrapper e) {
    bool success;
    system_message_t *msg;
    smartconfig_Event_Wrapper *wrapper;

    // The event wrapper is passed in on the stack, so we have to
    // allocate some memory that we can put in the message queue.
    msg = malloc(sizeof(system_message_t));
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate message memory.\n");
        exit(-1);
    }
    msg->sm = SMARTCONFIG;
    wrapper = &msg->wrapper.smartconfig;
    memcpy(wrapper, &e, sizeof(e));

    // Put the event on the queue, to be popped off later and handled
    // in order.
    success = enqueue(q, msg);
    if (!success) {
        fprintf(stderr, "Failed to enqueue message.\n");
        exit(-1);
    }
}

void sensor_Send_Message(sensor_Event_Wrapper e) {
    bool success;
    system_message_t *msg;
    sensor_Event_Wrapper *wrapper;

    // The event wrapper is passed in on the stack, so we have to
    // allocate some memory that we can put in the message queue.
    msg = malloc(sizeof(system_message_t));
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate message memory.\n");
        exit(-1);
    }
    msg->sm = SENSOR;
    wrapper = &msg->wrapper.sensor;
    memcpy(wrapper, &e, sizeof(e));

    // Put the event on the queue, to be popped off later and handled
    // in order.
    success = enqueue(q, msg);
    if (!success) {
        fprintf(stderr, "Failed to enqueue message.\n");
        exit(-1);
    }
}

void SMUDGE_debug_print(const char *a1, const char *a2, const char *a3) {
    fprintf(stderr, a1, a2, a3);
}

void SMUDGE_free(const void *a1) {
    free((void *)a1);
}

void SMUDGE_panic(void) {
    exit(-1);
}

void SMUDGE_panic_print(const char *a1, const char *a2, const char *a3) {
    fprintf(stderr, a1, a2, a3);
}

int initEventQueue() {
    q = newq();
    if (q == NULL) {
        fprintf(stderr, "Failed to get a queue.\n");
        return -1;
    }
    return 0;
}
