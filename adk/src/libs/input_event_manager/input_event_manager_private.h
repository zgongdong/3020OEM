/*
Copyright (c) 2018  Qualcomm Technologies International, Ltd.
*/

#ifndef INPUT_EVENT_MANAGER_PRIVATE_H_
#define INPUT_EVENT_MANAGER_PRIVATE_H_

/* control debug generation */
#ifdef INPUT_EVENT_MANAGER_DEBUG_LIB
#include <stdio.h>
#define IEM_DEBUG(x)  printf x
#else
#define IEM_DEBUG(x)
#endif

#define SINGLE_CLICK_TIMEOUT 500

typedef struct InputEventClient
{
    struct InputEventClient *next;
    Task task;
    unsigned pio:7;
    unsigned state:1;
} InputEventClient_t;

typedef struct InputPressTracker
{
    input_event_bits_t short_press;
    input_event_bits_t long_press;
} InputPressTracker_t;

typedef struct
{
    TaskData task;
    Task client;

    InputEventClient_t *client_list;

    const InputEventConfig_t *config;
    const InputActionMessage_t *action_table;
    uint8 num_action_messages;
    
    /* Separate trackers for the two press types so they can be configured
       independent of one another */
    InputPressTracker_t single_click_tracker;
    InputPressTracker_t double_click_tracker;

    /* The input event bits as last read or indicated */
    input_event_bits_t input_event_bits;

    /* PAM state stored for timers */
    const InputActionMessage_t *repeat;
    const InputActionMessage_t *held_release;

    /* PIO deep sleep wake up state PS Key setting */
    uint32 pio_state[IEM_NUM_BANKS];
} InputEventState_t;

/* Task messages */
enum
{
    IEM_INTERNAL_HELD_TIMER,
    IEM_INTERNAL_REPEAT_TIMER,
    IEM_INTERNAL_HELD_RELEASE_TIMER,
    IEM_INTERNAL_SINGLE_CLICK_TIMER,
    IEM_INTERNAL_DOUBLE_CLICK_TIMER
};

#endif    /* INPUT_EVENT_MANAGER_PRIVATE_H_ */
