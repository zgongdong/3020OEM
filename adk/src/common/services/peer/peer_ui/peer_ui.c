/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_ui.c
\brief      Intercept UI input messages, add the delay and re-inject locally for 
            synchronisation of UI input messages such as prompts.
*/

#include "peer_ui.h"
#include "peer_ui_typedef.h"
#include "peer_ui_marshal_typedef.h"

#include "logging.h"
#include "peer_signalling.h"
#include "system_clock.h"
#include "ui.h"

/* system includes */
#include <stdlib.h>
#include <panic.h>

#define US_TO_MS(us) ((us) / US_PER_MS)

/*! \brief Peer UI task data structure. */
typedef struct
{
    /*! Peer UI task */
    TaskData task;
} peer_ui_task_data_t;

/*! Instance of the peer Ui. */
peer_ui_task_data_t peer_ui;

#define peerUi_GetTaskData()        (&peer_ui)
#define peerUi_GetTask()            (&peer_ui.task)

/*! The default UI interceptor function */
static inject_ui_input ui_func_ptr_to_return = NULL;

/*! \brief Sends the message to secondary earbud. */
static void peerUi_SendUiInputToSecondary(ui_input_t ui_input)
{
    peer_ui_input_t* msg = PanicUnlessMalloc(sizeof(peer_ui_input_t));

    /* get the current system time */
    rtime_t now = SystemClockGetTimerTime();

    /* add the delay of PEER_UI_PROMPT_DELAY_MS to current system time which is sent to secondary, 
    this is the time when we want primary and secondary to handle the UI input*/
    marshal_rtime_t timestamp = rtime_add(now, PEER_UI_PROMPT_DELAY_US);

    msg->ui_input = ui_input;
    msg->timestamp = timestamp;

    DEBUG_LOG("peerUi_SendUiInputToSecondary send ui_input (0x%x) with timestamp %d us", ui_input, timestamp);
    appPeerSigMarshalledMsgChannelTx(peerUi_GetTask(),
                                     PEER_SIG_MSG_CHANNEL_PEER_UI,
                                     msg, MARSHAL_TYPE_peer_ui_input_t);
}

/***********************************
 * Marshalled Message TX CFM and RX
 **********************************/

/*! \brief Handle confirmation of transmission of a marshalled message. */
static void peerUi_HandleMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    peerSigStatus status = cfm->status;
    
    if (peerSigStatusSuccess != status)
    {
        DEBUG_LOG("peerUi_HandleMarshalledMsgChannelTxCfm reports failure status 0x%x(%d)",status,status);
    }
}

/*! \brief Handle incoming marshalled messages.*/
static void peerUi_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    PanicNull(ind);

    DEBUG_LOG("peerUi_HandleMarshalledMsgChannelRxInd channel %u type %u", ind->channel, ind->type);
    switch (ind->type)
    {
        case MARSHAL_TYPE_peer_ui_input_t:
        {
            /* received message at secondary EB */
            peer_ui_input_t* rcvd = (peer_ui_input_t*)ind->msg;

            PanicNull(rcvd);
            ui_input_t ui_input = rcvd->ui_input;
            marshal_rtime_t timestamp = rcvd->timestamp;
           
            /* difference between timestamp (sent by primary by when to handle the UI input) and 
            actual system time when UI input is received by secondary */
            int32 delta;

            /* system time when message received by secondary earbud */
            rtime_t now = SystemClockGetTimerTime();

            /* time left for secondary to handle the UI input */
            delta = rtime_sub(timestamp, now);

            /* Inject UI input to secondary with the time left, so it can handle UI input,
            we only inject UI input if secondary has any time left */    
            if(rtime_gt(delta, 0))
            {
                DEBUG_LOG("peerUi_HandleMarshalledMsgChannelRxInd send ui_input(0x%x) in %d ms", ui_input, US_TO_MS(delta));
                Ui_InjectUiInputWithDelay(ui_input, US_TO_MS(delta));
            }
        }
        break;

        default:
            /* Do not expect any other messages*/
            Panic();
        break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

/*! brief Interceptor call back function called by UI module on reception of UI input messages */
static void peerUi_Interceptor_FuncPtr(ui_input_t ui_input, uint32 delay)
{    
    switch(ui_input)
    {
        case ui_input_prompt_pairing:
        case ui_input_prompt_pairing_successful:
        case ui_input_prompt_pairing_failed:
        case ui_input_prompt_connected:
        case ui_input_prompt_disconnected:
            if(appPeerSigIsConnected())
            { 
                /* peer connection exists so need to delay in handling UI input at 
                primary EB so need to add delay when to handle the ui_input */
                delay = PEER_UI_PROMPT_DELAY_MS;
                   
                /* send ui_input to secondary */
                peerUi_SendUiInputToSecondary(ui_input);
            }
            break;

        default:
            break;
    }
    
    /* pass ui_input back to UI module */
    ui_func_ptr_to_return(ui_input, delay);
}

/*! brief Register the peer_ui interceptor function pointer with UI module 
    to receive all the ui_input messages */
static void peerUi_Register_Interceptor_Func(void)
{ 
    /* original UI function pointer received */
    ui_func_ptr_to_return = Ui_RegisterUiInputsInterceptor(peerUi_Interceptor_FuncPtr);
}


/*! Peer UI Message Handler. */
static void peerUi_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* marshalled messaging */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            peerUi_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            peerUi_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

        default:
            DEBUG_LOG("peerUi_HandleMessage unhandled message id %u", id);
            break;
    }
}

/*! brief Initialise Peer Ui  module */
bool PeerUi_Init(Task init_task)
{
    DEBUG_LOG("PeerUi_Init");
    peer_ui_task_data_t *theTaskData = peerUi_GetTaskData();
    
    theTaskData->task.handler = peerUi_HandleMessage;

    /* Register with peer signalling to use the peer UI msg channel */
    appPeerSigMarshalledMsgChannelTaskRegister(peerUi_GetTask(), 
                                               PEER_SIG_MSG_CHANNEL_PEER_UI,
                                               peer_ui_marshal_type_descriptors,
                                               NUMBER_OF_PEER_UI_MARSHAL_TYPES);

    /* get notification of peer signalling availability to send ui_input messages to peer */
    appPeerSigClientRegister(peerUi_GetTask()); 

    /* register the peer_ui interceptor function pointer with UI module 
    to receive all the ui_inputs messages */
    peerUi_Register_Interceptor_Func(); 

    UNUSED(init_task);  
    return TRUE;   
}
