/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice assistants.
*/
#include <panic.h>
#include <stdlib.h>
#include "voice_assistant_container.h"
#include "voice_assistant_state.h"

static voice_assistant_handle_t *active_va = NULL;

/*! \brief Container that holds the handle to created voice assistants */
voice_assistant_handle_t* voice_assistant_list[MAX_NO_VA_SUPPORTED];

/*! \brief return the Voice Assistant handle */
voice_assistant_handle_t* VoiceAssistants_GetHandleFromProvider(voice_assistant_provider_t provider_name)
{
    /* Find corresponding handle in the container */
    for(uint8 va_index=0;va_index<MAX_NO_VA_SUPPORTED;va_index++)
    {
        if(NULL != voice_assistant_list[va_index])
        {
            voice_assistant_handle_t* va_handle = voice_assistant_list[va_index];
            if(va_handle->voice_assistant && va_handle->voice_assistant->GetProvider)
            {   
                if(va_handle->voice_assistant->GetProvider() == provider_name)
                {
                    return va_handle;
                }
            }
        }
    }
    return NULL;
}

/*! \brief Initialise the Voice Assistants */
void VoiceAssistants_Init(void)
{
  int i;

  /* Initialise the Voice assistant container */
  for(i=0;i<MAX_NO_VA_SUPPORTED;i++)
  {
    voice_assistant_list[i] = NULL;
  }
}

/*! \brief Unregister the Voice Assistant */
void VoiceAssistants_UnRegister(voice_assistant_handle_t* va_handle)
{
    if(va_handle)
    {
       int i;
    
       for(i=0;i<MAX_NO_VA_SUPPORTED;i++)
       {
           if(va_handle == voice_assistant_list[i])
           {
               free((void*)voice_assistant_list[i]);
               voice_assistant_list[i] = NULL;
               break;
           }
        }
    }
}

/*! \brief Function called by VA's to register the callback routines */
voice_assistant_handle_t* VoiceAssistants_Register(voice_assistant_if* va_table)
{
  int va_index;
  voice_assistant_handle_t* va_handle = NULL;
  

  /* Find a free slot in the container */
  for(va_index=0;va_index<MAX_NO_VA_SUPPORTED;va_index++)
  {
     if(!voice_assistant_list[va_index])
        break;
  }

  if(va_index < MAX_NO_VA_SUPPORTED)
  {
     /* Allocate memory for Handle */
     va_handle = (voice_assistant_handle_t*)PanicUnlessMalloc(sizeof(voice_assistant_handle_t));

     /* Copy the function pointers routine */
     va_handle->voice_assistant = va_table;

     /* Copy the handle into the voice assistant container */
     voice_assistant_list[va_index] = va_handle;

     if(!active_va)
     {
         VoiceAssistant_SetActiveVa(va_handle);
     }
     else
     {
         voice_assistant_provider_t active_va_provider = active_va->voice_assistant->GetProvider();
         voice_assistant_provider_t new_va_provider = va_handle->voice_assistant->GetProvider();
        
         /* Provider id is used to determine a voice assistants priority upon registration */
         if(active_va_provider > new_va_provider)
            VoiceAssistant_SetActiveVa(va_handle);
     }
  }

  /* return the voice assistant handle */
  return va_handle;
}

/*! \brief Function called by VA's to inform any state change */
void VoiceAssistants_SetVaState(voice_assistant_handle_t* va_handle, voice_assistant_state_t state)
{
   if(va_handle)
   {
       /* Update the Voice Assistant State */
       va_handle->voice_assistant_state = state; 
       VoiceAssistant_HandleState(va_handle,state);
   }
}

voice_assistant_handle_t* VoiceAssistant_GetActiveVa(void)
{
    return active_va;
}

void VoiceAssistant_SetActiveVa(voice_assistant_handle_t *va_handle)
{
    active_va = va_handle;
}

/*! \brief Function to handle voice assistant events */
void VoiceAssistants_UiEventHandler(voice_assistant_handle_t* va_handle, ui_input_t event_id)
{
   if(va_handle)
   {
        if(va_handle->voice_assistant->EventHandler)
            va_handle->voice_assistant->EventHandler(event_id);
   }
}

/*! \brief Function to handle voice assistant suspend */
void VoiceAssistants_Suspend(voice_assistant_handle_t* va_handle)
{
   if(va_handle)
   {
        if(va_handle->voice_assistant->Suspend)
            va_handle->voice_assistant->Suspend();
   }
}

