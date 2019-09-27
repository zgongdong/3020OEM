/*!
\copyright  Copyright (c) 2018-2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       task_list.c
\brief      Implementation of a simple list of VM tasks.
*/

#include "task_list.h"

#include <stdlib.h>
#include <string.h>
#include <hydra_macros.h>

#include <panic.h>

/******************************************************************************
 * Internal functions
 ******************************************************************************/
/*! \brief Find the index in the task list array for a given task.

    \param[in]  list        Pointer to a Tasklist.
    \param[in]  search_task Task to search for on list.
    \param[out] index       Index at which search_task is found.

    \return bool TRUE search_task found and index returned.
                 FALSE search_task not found, index not valid.
 */
static bool taskList_FindTaskIndex(task_list_t* list, Task search_task, uint16* index)
{
    bool task_index_found = FALSE;
    uint16 iter = 0;

    for (iter = 0; iter < list->size_list; iter++)
    {
        if (list->tasks[iter] == search_task)
        {
            *index = iter;
            task_index_found = TRUE;
        }
    }

    return task_index_found;
}

/*! \brief Convert task_list_t to task_list_with_data_t.
    \param list Pointer to the list to convert.
    \return The task_list_with_data_t.
*/
static inline task_list_with_data_t *taskList_ToListWithData(task_list_t *list)
{
    return STRUCT_FROM_MEMBER(task_list_with_data_t, base, list);
}

/*! \brief Get pointer to task_list_data_t array from a task list.
    \param list Pointer to the list (must be task_list_with_data_t).
    \return Pointer to the array of task_list_data_t.
*/
static inline task_list_data_t *taskList_GetListData(task_list_t *list)
{
    return taskList_ToListWithData(list)->data;
}

/*! \brief Helper function that iterates through a list but also returns the index.

    \param[in]      list        Pointer to a Tasklist.
    \param[in,out]  next_task   IN Task from which to continue iterating though list.
                                OUT Next task in the list after next_task passed in.
    \param[out]     index       Index in the list at which OUT next_task was returned.

    \return bool TRUE iteration successful, another next_task and index provided.
                 FALSE empty list or end of list reached. next_task and index not
                       valid.
 */
static bool iterateIndex(task_list_t* list, Task* next_task, uint16* index)
{
    bool iteration_successful = FALSE;
    uint16 tmp_index = 0;

    PanicNull(list);
    PanicNull(next_task);
    PanicNull(index);

    /* list not empty */
    if (list->size_list)
    {
        /* next_task == NULL to start at tmp_index 0 */
        if (*next_task == 0)
        {
            *next_task = list->tasks[tmp_index];
            *index = tmp_index;
            iteration_successful =  TRUE;
        }
        else
        {
            /* move to next task */
            if (taskList_FindTaskIndex(list, *next_task, &tmp_index))
            {
                if (tmp_index + 1 < list->size_list)
                {
                    *next_task = list->tasks[tmp_index+1];
                    *index = tmp_index+1;
                    iteration_successful =  TRUE;
                }
            }
            else
            {
                /* end of the list */
                *next_task = 0;
            }
        }
    }

    return iteration_successful;
}

/******************************************************************************
 * External API functions
 ******************************************************************************/
/*! \brief Create a task_list_t.
 */
task_list_t* TaskList_Create(void)
{
    task_list_t* new_list = malloc(sizeof(*new_list));

    TaskList_Initialise(new_list);
    new_list->no_destroy = FALSE;

    return new_list;
}

void TaskList_Initialise(task_list_t* list)
{
    PanicNull(list);
    memset(list, 0, sizeof(*list));
    list->list_type = TASKLIST_TYPE_STANDARD;
    list->no_destroy = TRUE;
}

/*! \brief Create a task_list_t that can also store associated data.
 */
task_list_t* TaskList_WithDataCreate(void)
{
    task_list_with_data_t *new_list = malloc(sizeof(*new_list));

    TaskList_WithDataInitialise(new_list);

    return &new_list->base;
}

void TaskList_WithDataInitialise(task_list_with_data_t* list)
{
    PanicNull(list);
    memset(list, 0, sizeof(*list));
    list->base.list_type = TASKLIST_TYPE_WITH_DATA;
}

/*! \brief Destroy a task_list_t.
 */
void TaskList_Destroy(task_list_t* list)
{
    PanicNull(list);
    if (list->no_destroy)
    {
        Panic();
    }

    free(list->tasks);

    if (list->list_type == TASKLIST_TYPE_WITH_DATA)
    {
        task_list_with_data_t *list_with_data = taskList_ToListWithData(list);
        free(list_with_data->data);
        list_with_data->data = NULL;
        free(list_with_data);
        list_with_data = NULL;
    }
    else
    {
        free(list);
        list = NULL;
    }
}

/*! \brief Add a task to a list.
 */
bool TaskList_AddTask(task_list_t* list, Task add_task)
{
    bool task_added = FALSE;

    PanicNull(list);
    PanicNull(add_task);

    /* if not in the list */
    if (!TaskList_IsTaskOnList(list, add_task))
    {
        /* Resize list */
        list->tasks = realloc(list->tasks, sizeof(Task) * (list->size_list + 1));
        PanicNull(list->tasks);

        /* Add task to list */
        list->tasks[list->size_list] = add_task;
        list->size_list += 1;

        task_added = TRUE;
    }

    return task_added;
}

/*! \brief Add a task and data to a list.
 */
bool TaskList_AddTaskWithData(task_list_t* list, Task add_task, const task_list_data_t* data)
{
    bool task_with_data_added = FALSE;

    if (TaskList_IsTaskListWithData(list) && TaskList_AddTask(list, add_task))
    {
        task_list_with_data_t *list_with_data = taskList_ToListWithData(list);
        task_list_data_t *list_data = taskList_GetListData(list);

        /* create space in 'data' for new data item, size_list already
         * accounts for the +1 after call to TaskList_AddTask */
        list_data = realloc(list_data, sizeof(*list_data) * (list->size_list));
        list_with_data->data = PanicNull(list_data);
        /* but do need to use size_list-1 to access the new last
         * entry in the data array */
        list_data[list->size_list-1] = *data;
        task_with_data_added = TRUE;
    }

    return task_with_data_added;
}

/*! \brief Determine if a task is on a list.
 */
bool TaskList_IsTaskOnList(task_list_t* list, Task search_task)
{
    uint16 tmp;

    PanicNull(list);

    return taskList_FindTaskIndex(list, search_task, &tmp);
}

/*! \brief Remove a task from a list.
 */
bool TaskList_RemoveTask(task_list_t* list, Task del_task)
{
    bool task_removed = FALSE;
    uint16 index = 0;

    PanicNull(list);
    PanicNull(del_task);

    if (taskList_FindTaskIndex(list, del_task, &index))
    {
        uint16 tasks_to_end = list->size_list - index - 1;
        memmove(&list->tasks[index], &list->tasks[index] + 1, sizeof(Task) * tasks_to_end);
        if (list->list_type == TASKLIST_TYPE_WITH_DATA)
        {
            task_list_data_t *list_data = taskList_GetListData(list) + index;
            memmove(list_data, list_data + 1, sizeof(*list_data) * tasks_to_end);
        }
        list->size_list -= 1;

        if (!list->size_list)
        {
            free(list->tasks);
            list->tasks = NULL;
            if (list->list_type == TASKLIST_TYPE_WITH_DATA)
            {
                task_list_with_data_t *task_list_with_data = taskList_ToListWithData(list);
                free(task_list_with_data->data);
                task_list_with_data->data = NULL;
            }
        }
        else
        {
            /* resize list, if not an empty list now, then check realloc successfully
             * returned the memory */
            list->tasks = realloc(list->tasks, sizeof(Task) * list->size_list);
            PanicNull(list->tasks);
            if (list->list_type == TASKLIST_TYPE_WITH_DATA)
            {
                task_list_with_data_t *list_with_data = taskList_ToListWithData(list);
                task_list_data_t *list_data = taskList_GetListData(list);
                list_data = realloc(list_data, sizeof(*list_data) * (list->size_list));
                list_with_data->data = PanicNull(list_data);
            }
        }

        task_removed = TRUE;
    }

    return task_removed;
}

void TaskList_RemoveAllTasks(task_list_t* list)
{
    PanicNull(list);
    
    /* tasklist cannot remove all tasks for lists with data as the data would need
     * to be removed as well, and is owned by the tasklist owner. */
    if (list->list_type == TASKLIST_TYPE_WITH_DATA)
    {
        Panic();
    }

    free(list->tasks);
    list->tasks = NULL;
    list->size_list = 0;
}

/*! \brief Return number of tasks in list.
 */
uint16 TaskList_Size(task_list_t* list)
{
    return list ? list->size_list : 0;
}

/*! \brief Iterate through all tasks in a list.
 */
bool TaskList_Iterate(task_list_t* list, Task* next_task)
{
    uint16 tmp_index = 0;
    return iterateIndex(list, next_task, &tmp_index);
}

/*! \brief Iterate through all tasks in a list returning a copy of data as well.
 */
bool TaskList_IterateWithData(task_list_t* list, Task* next_task, task_list_data_t* data)
{
    bool iteration_successful = FALSE;
    task_list_data_t *raw_data = NULL;

    if (TaskList_IterateWithDataRaw(list, next_task, &raw_data))
    {
        *data = *raw_data;
        iteration_successful = TRUE;
    }

    return iteration_successful;
}

/*! \brief Iterate through all tasks in a list returning raw data stored in the
    task_list_t.
 */
bool TaskList_IterateWithDataRaw(task_list_t* list, Task* next_task, task_list_data_t** raw_data)
{
    bool iteration_successful = FALSE;
    uint16 tmp_index = 0;

    if (TaskList_IsTaskListWithData(list) && iterateIndex(list, next_task, &tmp_index))
    {
        *raw_data = taskList_GetListData(list) + tmp_index;
        iteration_successful = TRUE;
    }

    return iteration_successful;
}

/*! \brief Iterate through all tasks in a list calling handler each iteration. */
bool TaskList_IterateWithDataRawFunction(task_list_t *list, TaskListIterateWithDataRawHandler handler, void *arg)
{
    Task next_task = 0;
    uint16 index = 0;
    bool proceed = TRUE;

    PanicFalse(TaskList_IsTaskListWithData(list));

    while (proceed && iterateIndex(list, &next_task, &index))
    {
        task_list_data_t *data = taskList_GetListData(list) + index;
        proceed = handler(next_task, data, arg);
    }

    return proceed;
}

/*! \brief Create a duplicate task list.
 */
task_list_t *TaskList_Duplicate(task_list_t* list)
{
    task_list_t *new_list = NULL;

    PanicNull(list);

    if (list->list_type == TASKLIST_TYPE_STANDARD)
    {
        new_list = TaskList_Create();
    }
    else
    {
        new_list = TaskList_WithDataCreate();
    }

    if (new_list)
    {
        new_list->size_list = list->size_list;
        new_list->tasks = PanicUnlessMalloc(sizeof(Task) * new_list->size_list);
        memcpy(new_list->tasks, list->tasks, sizeof(Task) * new_list->size_list);

        if (new_list->list_type == TASKLIST_TYPE_WITH_DATA)
        {
            task_list_with_data_t *new_list_with_data = taskList_ToListWithData(new_list);
            task_list_with_data_t *old_list_with_data = taskList_ToListWithData(list);
            task_list_data_t *old_data = old_list_with_data->data;
            task_list_data_t *new_data;

            new_list_with_data->data = PanicUnlessMalloc(sizeof(task_list_data_t) * new_list->size_list);
            new_data = new_list_with_data->data;
            memcpy(new_data, old_data, sizeof(task_list_data_t) * new_list->size_list);
        }
    }

    return new_list;
}

/*! \brief Send a message (with message body) to all tasks in the task list.
*/
void TaskList_MessageSendWithSize(task_list_t *list, MessageId id, void *data, size_t size_data)
{
    TaskList_MessageSendLaterWithSize(list, id, data, size_data, D_IMMEDIATE);
}

/*! \brief Send a message (with message body and delay) to all tasks in the task list.
*/
void TaskList_MessageSendLaterWithSize(task_list_t *list, MessageId id, void *data, size_t size_data, uint32 delay)
{
    PanicNull(list);

    if (list->size_list)
    {
        int index;

        for (index = 0; index < list->size_list; index++)
        {
            void *copy = NULL;
            if (size_data && (data != NULL))
            {
                copy = PanicUnlessMalloc(size_data);
                memcpy(copy, data, size_data);
            }

            MessageSendLater(list->tasks[index], id, copy, delay);
        }
    }

    if(size_data && (data != NULL))
    {
        free(data);
    }
}

/*! \brief Get a copy of the data stored in the list for a given task.
*/
bool TaskList_GetDataForTask(task_list_t* list, Task search_task, task_list_data_t* data)
{
    bool data_copied = FALSE;
    task_list_data_t *raw_data = NULL;

    if (TaskList_GetDataForTaskRaw(list, search_task, &raw_data))
    {
        *data = *raw_data;
        data_copied = TRUE;
    }

    return data_copied;
}

/*! \brief Get the address of the data stored in the list for a given task.
*/
bool TaskList_GetDataForTaskRaw(task_list_t* list, Task search_task, task_list_data_t** raw_data)
{
    bool data_copied = FALSE;
    uint16 tmp;

    if (TaskList_IsTaskListWithData(list) && taskList_FindTaskIndex(list, search_task, &tmp))
    {
        *raw_data = taskList_GetListData(list) + tmp;
        data_copied = TRUE;
    }

    return data_copied;
}

/*! \brief Set the data stored in the list for a given task.
*/
bool TaskList_SetDataForTask(task_list_t* list, Task search_task, const task_list_data_t* data)
{
    bool data_set = FALSE;
    uint16 tmp;

    if (TaskList_IsTaskListWithData(list) && taskList_FindTaskIndex(list, search_task, &tmp))
    {
        *(taskList_GetListData(list) + tmp) = *data;
        data_set = TRUE;
    }

    return data_set;
}

/*! \brief Determine if the list is one that supports data.
*/
bool TaskList_IsTaskListWithData(task_list_t* list)
{
    PanicNull(list);
    return list->list_type == TASKLIST_TYPE_WITH_DATA;
}

void TaskList_Init(void)
{

}
