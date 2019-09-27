/*!
\copyright  Copyright (c) 2018-2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       task_list.h
\brief      Interface to simple list of VM tasks.

            Task lists may be allocated dynamically or statically.
            For example:
            // Dynamic allocation/initialisation:
            task_list_t *list = TaskList_Create();
            task_list_t *list_with_data = TaskList_WithDataCreate();

            // Static allocation/initialisation:
            task_list_t list;
            task_list_with_data_t list_with_data;
            task_list_t *list_with_data_base;
            TaskList_Initialise(&list);
            TaskList_WithDataInitialise(&list_with_data);
            list_with_data_base = TaskList_GetBaseTaskList(list_with_data);
            // task_list API can be accessed using either &list or list_with_data_base

            The structures task_list_t and task_list_with_data_t are exposed
            to allow the user to allocate task lists dynamically or statically.
            However, the task_list module reserves the right to change its
            implementation in future releases. Therefore, do _not_ access
            members of task_list_t/task_list_with_data_t directly, use the
            accessor APIs instead.

            If a statically allocated task_list_with_data_t is used, the task list
            APIs may be used by passing the address of the base member of the
            task_list_with_data_t structure. The helper macro TaskList_GetBaseTaskList
            _should_ be used to retrieve the base task list.
*/

#ifndef TASK_LIST_H
#define TASK_LIST_H

#include <message.h>

/*! \brief Types of task_list_t.
 */
typedef enum
{
    /*! Standard list of Tasks. */
    TASKLIST_TYPE_STANDARD,

    /*! task_list_t with associated data. */
    TASKLIST_TYPE_WITH_DATA
} task_list_type_t;

/*! 64-bit task_list_data_t type */
typedef unsigned long long tl_uint64;

/*! \brief Definition of 'data' that can be stored in a TaskListWithData.
 */
typedef union
{
    /*! Unsigned 8-bit data. */
    uint8 u8;

    /*! Unsigned 16-bit data. */
    uint16 u16;

    /*! Unsigned 32-bit data. */
    uint32 u32;

    /*! Pointer. */
    void* ptr;

    /*! Unsigned 64-bit data. */
    tl_uint64 u64;

    /*! Signed 8-bit data. */
    int8 s8;

    /*! Signed 16-bit data. */
    int16 s16;

    /*! Signed 32-bit data. */
    int32 s32;

    /*! Unsigned 8-bit array */
    uint8 arr_u8[8];

    /*! Signed 8-bit array */
    int8 arr_s8[8];
} task_list_data_t;

/*! \brief List of VM Tasks.
 */
typedef struct
{
    /*! List of tasks. */
    Task* tasks;

    /*! Number of tasks in #tasks. */
    unsigned size_list : 8;

    /*! Standard task_list_t or one that can support data. */
    unsigned list_type : 1;


    /*! If set, this task list was statically allocated and initialised with
     *  #TaskList_Initialise, it is not valid to call #TaskList_Destroy */
    unsigned no_destroy : 1;
} task_list_t;

/*! \brief Task list with data */
typedef struct
{
    /*! Base task_list */
    task_list_t base;

    /*! List of data items. */
    task_list_data_t* data;

} task_list_with_data_t;

/*! \brief A function type to handle one iteration of #TaskList_IterateWithDataRawFunction.
    \param task The current task.
    \param data The current task's data.
    \param arg An abstract object passed by the caller to #TaskList_IterateWithDataRawFunction.
    \return TRUE if the iteration should continue, FALSE if the iteration should stop.
    \note The data pointer is guaranteed valid only for the current call. The task_list_t module
    reserves the right to invalidate this pointer (e.g. through reallocation)
    in any subsequent task_list_t function call.
 */
typedef bool (*TaskListIterateWithDataRawHandler)(Task task, task_list_data_t *data, void *arg);

/*! \brief Initialise the TaskList library.
*/
void TaskList_Init(void);

/*! \brief Obtain pointer to the base task_list_t from a task_list_with_data_t.
    \return The base task_list_t.

    Always use this function to get the base task_list_t, do not access the base
    member directly.
*/
static inline task_list_t *TaskList_GetBaseTaskList(task_list_with_data_t *list)
{
    return &list->base;
}

/*! \brief Create a task_list_t.

    Must only be used for dynamically allocated task lists, i.e. task_list_t*
    For statically allocated task lists, use #TaskList_Initialise.

    \return task_list_t* Pointer to new task_list_t.
 */
task_list_t* TaskList_Create(void);

/*! \brief Initialise a task_list_t.
    \param list The list to initialise.

    Must only be used for statically allocated task lists, i.e. task_list_t.
    For dynamically allocated task lists, use #TaskList_Create.
*/
void TaskList_Initialise(task_list_t* list);

/*! \brief Remove all tasks from a list.
    \param list The list from which to remove all tasks.
*/    
void TaskList_RemoveAllTasks(task_list_t* list);

/*! \brief Create a task_list_t that can also store associated data.

    \return task_list_t* Pointer to new task_list_t.
 */
task_list_t* TaskList_WithDataCreate(void);

/*! \brief Initialise a task_list_t with data.
    \param list The list to initialise.
*/
void TaskList_WithDataInitialise(task_list_with_data_t* list);

/*! \brief Destroy a task_list_t.

    \param[in] list Pointer to a Tasklist.

    \return bool TRUE task_list_t destroyed successfully.
                 FALSE task_list_t destroy failure.

    Must only be used with dynamically allocated task lists that
    have been instantiated via #TaskList_Create.
 */
void TaskList_Destroy(task_list_t* list);

/*! \brief Add a task to a list.

    \param[in] list     Pointer to a Tasklist.
    \param[in] add_task Task to add to the list.

    \return FALSE if the task is already on the list, otherwise TRUE.
 */
bool TaskList_AddTask(task_list_t* list, Task add_task);

/*! \brief Add a task and data to a list.

    \param[in] list     Pointer to a Tasklist.
    \param[in] add_task Task to add to the list.
    \param[in] data     Data to store with the task on the list.

    \return FALSE if the task is already on the list, otherwise TRUE.
 */
bool TaskList_AddTaskWithData(task_list_t* list, Task add_task, const task_list_data_t* data);

/*! \brief Determine if a task is on a list.

    \param[in] list Pointer to a Tasklist.
    \param[in] search_task Task to search for on list.

    \return bool TRUE search_task is on list, FALSE search_task is not on the list.
 */
bool TaskList_IsTaskOnList(task_list_t* list, Task search_task);

/*! \brief Remove a task from a list.

    \param[in] list     Pointer to a Tasklist.
    \param[in] del_task Task to remove from the list.

    \return FALSE if the task was not on the list, otherwise TRUE.
 */
bool TaskList_RemoveTask(task_list_t* list, Task del_task);

/*! \brief Return number of tasks in list.

    \param[in] list Pointer to a Tasklist.

    \return uint16 Number of Tasks in the list. Zero if list is set to NULL.
 */
uint16 TaskList_Size(task_list_t* list);

/*! \brief Iterate through all tasks in a list.

    Pass NULL to next_task to start iterating at first task in the list.
    On each subsequent call next_task should be the task previously returned
    and TaskList_Iterate will return the next task in the list.

    \param[in]     list      Pointer to a Tasklist.
    \param[in,out] next_task Pointer to task from which to iterate.

    \return bool TRUE next_task is returning a Task.
                 FALSE end of list, next_task is not returning a Task.
 */
bool TaskList_Iterate(task_list_t* list, Task* next_task);

/*! \brief Iterate through all tasks in a list returning a copy of data as well.

    Pass NULL to next_task to start iterating at first task in the list.
    On each subsequent call next_task should be the task previously returned
    and TaskList_Iterate will return the next task in the list.

    \param[in]     list      Pointer to a Tasklist.
    \param[in,out] next_task Pointer to task from which to iterate.
    \param[out]    data      Pointer into which data associated with the task is copied.

    \return bool TRUE next_task is returning a Task.
                 FALSE end of list, next_task is not returning a Task.
 */
bool TaskList_IterateWithData(task_list_t* list, Task* next_task, task_list_data_t* data);

/*! \brief Iterate through all tasks in a list returning raw data stored in the
    task_list_t.

    Useful for efficiently modifying the data for all items in a #task_list_t.

    Pass NULL to next_task to start iterating at first task in the list.
    On each subsequent call next_task should be the task previously returned
    and TaskList_Iterate will return the next task in the list.

    \param[in]     list      Pointer to a Tasklist.
    \param[in,out] next_task Pointer to task from which to iterate.
    \param[out]    raw_data  Address of pointer to the task's raw data stored by tasklist.

    \return bool TRUE next_task is returning a Task.
                 FALSE end of list, next_task is not returning a Task.

    \note The lifetime of the raw_data pointer is limited. The task_list_t module
    reserves the right to invalidate this pointer (e.g. through reallocation)
    in any subsequent task_list_t function call.
 */
bool TaskList_IterateWithDataRaw(task_list_t* list, Task* next_task, task_list_data_t** raw_data);

/*! \brief Iterate through all tasks in a list calling handler each iteration.

    \param[in] list     Pointer to a Tasklist.
    \param[in] handler  Function to handle each iteration. If the handler returns
            FALSE, the iteration will stop.
    \param[in] arg      Abstract object to pass to handler every iteration.

    \return TRUE if the iteration completed, FALSE if the iteration stopped because
            a call to handler returned FALSE.
*/
bool TaskList_IterateWithDataRawFunction(task_list_t *list, TaskListIterateWithDataRawHandler handler, void *arg);

/*! \brief Create a duplicate task list.

    \param[in] list Pointer to a Tasklist.

    \return task_list_t * Pointer to duplicate task list.
 */
task_list_t *TaskList_Duplicate(task_list_t* list);

/*! \brief Send a message (with message body) to all tasks in the task list.

    \param[in] list  Pointer to a task_list_t.
    \param id The message ID to send to the task_list_t.
    \param[in] data Pointer to the message content.
    \param size_data The sizeof the message content.
*/
void TaskList_MessageSendWithSize(task_list_t *list, MessageId id, void *data, size_t size_data);

/*! \brief Send a message (with message body and delay) to all tasks in the task list.

    \param[in] list  Pointer to a task_list_t.
    \param id The message ID to send to the task_list_t.
    \param[in] data Pointer to the message content.
    \param size_data The sizeof the message content.
    \param delay The delay in ms before the message will be sent.
*/
void TaskList_MessageSendLaterWithSize(task_list_t *list, MessageId id, void *data, size_t size_data, uint32 delay);

/*! \brief Send a message (with data) to all tasks in the task list.

    \param[in] list Pointer to a task_list_t.
    \param id The message ID to send to the task_list_t.
    \param[in] message Pointer to the message content.

    \note Assumes id is of a form such that appending a _T to the id creates the
    message structure type string.
*/
#define TaskList_MessageSend(list, id, message) \
    TaskList_MessageSendWithSize(list, id, message, sizeof(id##_T))

/*! \brief Send a message (with data and delay) to all tasks in the task list.

    \param[in] list Pointer to a task_list_t.
    \param id The message ID to send to the task_list_t.
    \param[in] message Pointer to the message content.
    \param delay The delay in ms before the message will be sent.

    \note Assumes id is of a form such that appending a _T to the id creates the
    message structure type string.
*/
#define TaskList_MessageSendLater(list, id, message, delay) \
    TaskList_MessageSendLaterWithSize(list, id, message, delay sizeof(id##_T), delay)

/*! \brief Send a message without content to all tasks in the task list.

    \param[in] list Pointer to a task_list_t.
    \param id The message ID to send to the task_list_t.
*/
#define TaskList_MessageSendId(list, id) \
    TaskList_MessageSendWithSize(list, id, NULL, 0)


/*! \brief Send a message without content to all tasks in the task list.

    \param[in] list Pointer to a task_list_t.
    \param id The message ID to send to the task_list_t.
    \param delay The delay in ms before the message will be sent.
*/
#define TaskList_MessageSendLaterId(list, id, delay) \
    TaskList_MessageSendLaterWithSize(list, id, NULL, 0, delay)

/*! \brief Get a copy of the data stored in the list for a given task.

    \param[in]  list        Pointer to a Tasklist.
    \param[in]  search_task Task to search for on list.
    \param[out] data        Pointer to copy the associated task data.

    \return bool TRUE search_task is on the list and data copied.
                 FALSE search_task is not on the list.
*/
bool TaskList_GetDataForTask(task_list_t* list, Task search_task, task_list_data_t* data);

/*! \brief Get the address of the data stored in the list for a given task.

    \param[in]  list        Pointer to a Tasklist.
    \param[in]  search_task Task to search for on list.
    \param[out] raw_data    Address of pointer to the associated task data.

    \return bool TRUE search_task is on the list and raw_data address set.
                 FALSE search_task is not on the list.

    \note The lifetime of the raw_data pointer is limited. The task_list_t module
    reserves the right to invalidate this pointer (e.g. through reallocation)
    in any subsequent task_list_t function call.
*/
bool TaskList_GetDataForTaskRaw(task_list_t* list, Task search_task, task_list_data_t** raw_data);

/*! \brief Set the data stored in the list for a given task.

    \param[in] list        Pointer to a Tasklist.
    \param[in] search_task Task to search for on list.
    \param[in] data        Pointer to new associated task data.

    \return bool TRUE search_task is on the list and data changed.
                 FALSE search_task is not on the list.
*/
bool TaskList_SetDataForTask(task_list_t* list, Task search_task, const task_list_data_t* data);

/*! \brief Determine if the list is one that supports data.

    \param[in] list Pointer to a Tasklist.

    \return bool TRUE list supports data, FALSE list does not support data.
*/
bool TaskList_IsTaskListWithData(task_list_t* list);

#endif /* TASK_LIST_H */


