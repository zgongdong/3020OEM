/* ==========================================
    CMock Project - Automatic Mock Generation for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */
#include <stdlib.h>
#include <string.h>
#include "unity.h"

#include "cmock_internals.h"
/* public constants to be used by mocks */
const char* CMockStringOutOfMemory = "CMock has run out of memory. Please allocate more.";
const char* CMockStringCalledMore  = "Called more times than expected.";
const char* CMockStringCalledLess  = "Called less times than expected.";
const char* CMockStringCalledEarly = "Called earlier than expected.";
const char* CMockStringCalledLate  = "Called later than expected.";
const char* CMockStringCallOrder   = "Called out of order.";
const char* CMockStringIgnPreExp   = "IgnoreArg called before Expect.";
const char* CMockStringPtrPreExp   = "ReturnThruPtr called before Expect.";
const char* CMockStringPtrIsNULL   = "Pointer is NULL.";
const char* CMockStringExpNULL     = "Expected NULL.";
const char* CMockStringMismatch    = "Function called with unexpected argument value.";

/* private variables */
#ifdef CMOCK_MEM_DYNAMIC
static unsigned char*         CMock_Guts_Buffer = NULL;
static char*                  cmock_msg_buff = NULL;
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_BufferSize = CMOCK_MEM_ALIGN_SIZE;
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_FreePtr;
#else
static unsigned char          CMock_Guts_Buffer[CMOCK_MEM_SIZE + CMOCK_MEM_ALIGN_SIZE];
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_BufferSize = CMOCK_MEM_SIZE + CMOCK_MEM_ALIGN_SIZE;
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_FreePtr;
static char                   CMock_msg_buff[CMOCK_PRINT_BUFFER_SIZE];
#endif

/*-------------------------------------------------------*/
/* CMock_Msg_Gen                                         */
/*-------------------------------------------------------*/
char *CMock_Msg_Gen(const char *func, CMOCKMessagesType msg_type, int arg)
{   
    UNITY_TEST_ASSERT(strlen(func) + 20 + 4 <= CMOCK_PRINT_BUFFER_SIZE, __LINE__, __FILE__, "CMOCK print buffer not large enough for function name");
    UNITY_TEST_ASSERT_NOT_NULL(cmock_msg_buff, __LINE__, __FILE__, "CMOCK Not Initialised! Initialise at least one Mocked Module and register at least one expectation!");
    memset(cmock_msg_buff, 0x00, CMOCK_PRINT_BUFFER_SIZE);
    switch(msg_type)
    {
    case GREATER_THAN_EXPECTED:
        sprintf(cmock_msg_buff, "%s called > expected", func);
        return cmock_msg_buff;

    case CALLED_EARLIER:
        sprintf(cmock_msg_buff, "%s called earlier", func);
        return cmock_msg_buff;

    case CALLED_LATER:
        sprintf(cmock_msg_buff, "%s called later", func);
        return cmock_msg_buff;

    case OUT_OF_ORDER:
        sprintf(cmock_msg_buff, "%s called Out of order", func);
        return cmock_msg_buff;

    case ARG_WRONG_VALUE:
        sprintf(cmock_msg_buff, "%s arg %d wrong value", func, arg);
        return cmock_msg_buff;

    case LESS_THAN_EXPECTED:
        sprintf(cmock_msg_buff, "%s called < expected", func);
        return cmock_msg_buff;

    case IGNORE_BEFORE_EXPECT:
        sprintf(cmock_msg_buff, "IgnoreArg param %d before Expect %s}", arg, func);
        return cmock_msg_buff;

    default:
        TEST_FAIL_MESSAGE("Unknown CMOCKMessagesType!")
        break;
    };
    return cmock_msg_buff;
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemNew                                     */
/*-------------------------------------------------------*/
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemNew(CMOCK_MEM_INDEX_TYPE size)
{
  CMOCK_MEM_INDEX_TYPE index;

  /* verify arguments valid (we must be allocating space for at least 1 byte, and the existing chain must be in memory somewhere) */
  if (size < 1)
    return CMOCK_GUTS_NONE;

  /* verify we have enough room */
  size = size + CMOCK_MEM_INDEX_SIZE;
  if (size & CMOCK_MEM_ALIGN_MASK)
    size = (size + CMOCK_MEM_ALIGN_MASK) & ~CMOCK_MEM_ALIGN_MASK;
  if ((CMock_Guts_BufferSize - CMock_Guts_FreePtr) < size)
  {
#ifdef CMOCK_MEM_DYNAMIC
#ifdef HOSTED_TEST_ENVIRONMENT
    CMock_Guts_BufferSize += CMOCK_MEM_SIZE + size;
    CMock_Guts_Buffer = realloc(CMock_Guts_Buffer, (size_t)CMock_Guts_BufferSize);
    if(cmock_msg_buff == NULL)
      cmock_msg_buff = malloc((size_t)CMOCK_PRINT_BUFFER_SIZE);
    if (CMock_Guts_Buffer == NULL || cmock_msg_buff == NULL)
#else
    if(CMock_Guts_Buffer == NULL && cmock_msg_buff == NULL)
    {
      CMock_Guts_BufferSize = CMOCK_MEM_SIZE;
      CMock_Guts_Buffer = PanicUnlessMalloc((size_t)CMock_Guts_BufferSize);
      cmock_msg_buff = PanicUnlessMalloc((size_t)CMOCK_PRINT_BUFFER_SIZE);
      if (CMock_Guts_Buffer == NULL || cmock_msg_buff == NULL)
        return CMOCK_GUTS_NONE;
    }
    else
#endif
#endif /* yes that if will continue to the return below if TRUE */
      return CMOCK_GUTS_NONE;
  }

  /* determine where we're putting this new block, and init its pointer to be the end of the line */
  index = CMock_Guts_FreePtr + CMOCK_MEM_INDEX_SIZE;
  *(CMOCK_MEM_INDEX_TYPE*)(&CMock_Guts_Buffer[CMock_Guts_FreePtr]) = CMOCK_GUTS_NONE;
  CMock_Guts_FreePtr += size;

  return index;
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemChain                                   */
/*-------------------------------------------------------*/
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemChain(CMOCK_MEM_INDEX_TYPE root_index, CMOCK_MEM_INDEX_TYPE obj_index)
{
  CMOCK_MEM_INDEX_TYPE index;
  void* root;
  void* obj;
  void* next;

  if (root_index == CMOCK_GUTS_NONE)
  {
    /* if there is no root currently, we return this object as the root of the chain */
    return obj_index;
  }
  else
  {
    /* reject illegal nodes */
    if ((root_index < CMOCK_MEM_ALIGN_SIZE) || (root_index >= CMock_Guts_FreePtr))
    {
      return CMOCK_GUTS_NONE;
    }
    if ((obj_index < CMOCK_MEM_ALIGN_SIZE) || (obj_index >= CMock_Guts_FreePtr))
    {
      return CMOCK_GUTS_NONE;
    }

    root = (void*)(&CMock_Guts_Buffer[root_index]);
    obj  = (void*)(&CMock_Guts_Buffer[obj_index]);

    /* find the end of the existing chain and add us */
    next = root;
    do {
      index = *(CMOCK_MEM_INDEX_TYPE*)((CMOCK_MEM_PTR_AS_INT)next - CMOCK_MEM_INDEX_SIZE);
      if (index >= CMock_Guts_FreePtr)
        return CMOCK_GUTS_NONE;
      if (index > 0)
        next = (void*)(&CMock_Guts_Buffer[index]);
    } while (index > 0);
    *(CMOCK_MEM_INDEX_TYPE*)((CMOCK_MEM_PTR_AS_INT)next - CMOCK_MEM_INDEX_SIZE) = (CMOCK_MEM_INDEX_TYPE)((CMOCK_MEM_PTR_AS_INT)obj - (CMOCK_MEM_PTR_AS_INT)CMock_Guts_Buffer);
    return root_index;
  }
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemNext                                    */
/*-------------------------------------------------------*/
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemNext(CMOCK_MEM_INDEX_TYPE previous_item_index)
{
  CMOCK_MEM_INDEX_TYPE index;
  void* previous_item;

  /* There is nothing "next" if the pointer isn't from our buffer */
  if ((previous_item_index < CMOCK_MEM_ALIGN_SIZE) || (previous_item_index  >= CMock_Guts_FreePtr))
    return CMOCK_GUTS_NONE;
  previous_item = (void*)(&CMock_Guts_Buffer[previous_item_index]);

  /* if the pointer is good, then use it to look up the next index               */
  /* (we know the first element always goes in zero, so NEXT must always be > 1) */
  index = *(CMOCK_MEM_INDEX_TYPE*)((CMOCK_MEM_PTR_AS_INT)previous_item - CMOCK_MEM_INDEX_SIZE);
  if ((index > 1) && (index < CMock_Guts_FreePtr))
    return index;
  else
    return CMOCK_GUTS_NONE;
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemEndOfChain                              */
/*-------------------------------------------------------*/
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemEndOfChain(CMOCK_MEM_INDEX_TYPE root_index)
{
  CMOCK_MEM_INDEX_TYPE index = root_index;
  CMOCK_MEM_INDEX_TYPE next_index;

  for (next_index = root_index;
       next_index != CMOCK_GUTS_NONE;
       next_index = CMock_Guts_MemNext(index))
  {
    index = next_index;
  }

  return index;
}

/*-------------------------------------------------------*/
/* CMock_GetAddressFor                                   */
/*-------------------------------------------------------*/
void* CMock_Guts_GetAddressFor(CMOCK_MEM_INDEX_TYPE index)
{
  if ((index >= CMOCK_MEM_ALIGN_SIZE) && (index < CMock_Guts_FreePtr))
  {
    return (void*)(&CMock_Guts_Buffer[index]);
  }
  else
  {
    return NULL;
  }
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemBytesFree                               */
/*-------------------------------------------------------*/
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemBytesFree(void)
{
  return CMock_Guts_BufferSize - CMock_Guts_FreePtr;
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemBytesUsed                               */
/*-------------------------------------------------------*/
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemBytesUsed(void)
{
  return CMock_Guts_FreePtr - CMOCK_MEM_ALIGN_SIZE;
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemFreeAll                                 */
/*-------------------------------------------------------*/
void CMock_Guts_MemFreeAll(void)
{
  CMock_Guts_FreePtr = CMOCK_MEM_ALIGN_SIZE; /* skip the very beginning */
}

/*-------------------------------------------------------*/
/* CMock_Guts_MemFreeFinal                               */
/*-------------------------------------------------------*/
void CMock_Guts_MemFreeFinal(void)
{
  CMock_Guts_FreePtr = CMOCK_MEM_ALIGN_SIZE;
  CMock_Guts_BufferSize = CMOCK_MEM_ALIGN_SIZE;
#ifdef CMOCK_MEM_DYNAMIC
  if (CMock_Guts_Buffer)
  {
    free(CMock_Guts_Buffer);
    free(cmock_msg_buff);
    cmock_msg_buff = NULL;
    CMock_Guts_Buffer = NULL;
  }
#endif
}

