/* ==========================================
    CMock Project - Automatic Mock Generation for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#ifndef CMOCK_FRAMEWORK_H
#define CMOCK_FRAMEWORK_H

/* These are constants that the generated mocks have access to */
extern const char* CMockStringOutOfMemory;
extern const char* CMockStringCalledMore;
extern const char* CMockStringCalledLess;
extern const char* CMockStringCalledEarly;
extern const char* CMockStringCalledLate;
extern const char* CMockStringCallOrder;
extern const char* CMockStringIgnPreExp;
extern const char* CMockStringPtrPreExp;
extern const char* CMockStringPtrIsNULL;
extern const char* CMockStringExpNULL;
extern const char* CMockStringMismatch;

/* should be big enough to index full range of CMOCK_MEM_MAX */
#ifndef CMOCK_MEM_INDEX_TYPE
#define CMOCK_MEM_INDEX_TYPE  unsigned int
#endif

#define CMOCK_GUTS_NONE   (0)

#define CMOCK_ARG_MODE    CMOCK_MEM_INDEX_TYPE
#define CMOCK_ARG_ALL     0
#define CMOCK_ARG_NONE    ((CMOCK_MEM_INDEX_TYPE)(~0U))

/*-------------------------------------------------------*/
/* Memory API                                            */
/*-------------------------------------------------------*/
CMOCK_MEM_INDEX_TYPE  CMock_Guts_MemNew(CMOCK_MEM_INDEX_TYPE size);
CMOCK_MEM_INDEX_TYPE  CMock_Guts_MemChain(CMOCK_MEM_INDEX_TYPE root_index, CMOCK_MEM_INDEX_TYPE obj_index);
CMOCK_MEM_INDEX_TYPE  CMock_Guts_MemNext(CMOCK_MEM_INDEX_TYPE previous_item_index);
CMOCK_MEM_INDEX_TYPE  CMock_Guts_MemEndOfChain(CMOCK_MEM_INDEX_TYPE root_index);

void*                 CMock_Guts_GetAddressFor(CMOCK_MEM_INDEX_TYPE index);

CMOCK_MEM_INDEX_TYPE  CMock_Guts_MemBytesFree(void);
CMOCK_MEM_INDEX_TYPE  CMock_Guts_MemBytesUsed(void);
void                  CMock_Guts_MemFreeAll(void);
void                  CMock_Guts_MemFreeFinal(void);

typedef enum {
    GREATER_THAN_EXPECTED = 1,
    CALLED_EARLIER,
    CALLED_LATER,
    OUT_OF_ORDER,
    ARG_WRONG_VALUE,
    LESS_THAN_EXPECTED,
    IGNORE_BEFORE_EXPECT
} CMOCKMessagesType;

char *CMock_Msg_Gen(const char *func, CMOCKMessagesType msg_type, int arg);

#endif /* CMOCK_FRAMEWORK */
