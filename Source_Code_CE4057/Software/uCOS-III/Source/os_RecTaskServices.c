/*
Keep existing tick list, pend list and ready list for original tick tasks, idle tasks, ISR tasks etc.

Use AVL tree for task recursion's "dispatch list", aimed to keep track of tasks to dispatch.
Use Binary Heap for task scheduler's "recursive ready list", aimed to function as a separate ready list for keeping track of deadlines and adjusting task priority according to EDF rules
*/

/* Priority ranges between OS_CFG_REC_TASK_PRIO to OS_CFG_STAT_TASK_PRIO are RESERVED for recursive tasks */

#include "os_RecTaskDataStructs.c"
void  OS_RecTask (void *p_arg)
{
// NO recursive task may have its priority higher than this task!
  OS_ERR err;
  OS_REC_TASK_NODE* incomingNode;
  OS_MSG_SIZE incomingNodeSize;
  CPU_TS ts;
  
  while(DEF_ON)
  {
    // insert that runnable task into binary heap
    // the running task should remain at the bin heap root if not pre-empted.
    // take earliest deadline task from binary heap, run it. rmb OPT |= OS_OPT_TASK_RECURSIVE and RecursiveTaskNode = its node
     incomingNode = (OS_REC_TASK_NODE*)OSTaskQPend(0, OS_OPT_PEND_BLOCKING, &incomingNodeSize, &ts, &err);
     if(OSRecCheckNodeReady(incomingNode) == DEF_FALSE)
     {
       // this task wants to run!
       OSRecRdyListInsert(incomingNode, &err); // this sets the taskPrio automatically
       OSTaskCreate((OS_TCB     *)incomingNode->tcb,
                 (CPU_CHAR   *)((void *)"uC/OS-III Recursive Task"),
                 (OS_TASK_PTR )incomingNode->taskPtr,
                 (void       *)0,
                 (OS_PRIO     )incomingNode->taskPrio, // this is settled when heapifying binary heap
                 (CPU_STK    *)OSCfg_RecTaskStkBasePtr,
                 (CPU_STK_SIZE)OSCfg_RecTaskStkLimit,
                 (CPU_STK_SIZE)OSCfg_RecTaskStkSize,
                 (OS_MSG_QTY  )0u,
                 (OS_TICK     )0u,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_RECURSIVE),
                 (OS_ERR     *)&err);
     }
     else
     {
       // this task is just being deleted! revert its prio back to nominal
       // Note: TCB should still be intact since deleted task guranteed to have priority lower than this task
       if(incomingNode != OSRecRdyList[0])
       {
          // this should not happen. TODO: handle/display error
       }
       OSRecRdyListExtractNext(&err); // this invalidates the taskPrio automatically
       OSTaskChangePrio(OSRecRdyList[0]->tcb, OSRecRdyList[0]->taskPrio, &err); // refresh new root node's task priority to take removed node's place
       // OSSched should handle the rest
     }
  }
}
void OSRecTaskInit(OS_ERR *p_err)
{
  if (OSCfg_RecTaskPrio >= (OS_CFG_PRIO_MAX - 1u)) {     /* Only one task at the 'Idle Task' priority              */
        *p_err = OS_ERR_PRIO_INVALID;
        return;
   }
  *p_err = OS_ERR_NONE;

  OSRecTaskCount = 0;
  OSRecRdyListCount = 0;
    
  for(CPU_INT16U i=0; i<OS_REC_MAX_TASKS; i++)
  {
    OSRecRdyList[i] = (OS_REC_TASK_NODE*)0;
  }
  initNode(&OSRecTaskList, (void*)0, 0, 0);

  OSTaskCreate((OS_TCB     *)&OSRecTaskTCB,
               (CPU_CHAR   *)((void *)"uC/OS-III Recursive Task Scheduler"),
               (OS_TASK_PTR )OS_RecTask,
               (void       *)0,
               (OS_PRIO     )OSCfg_RecTaskPrio,
               (CPU_STK    *)OSCfg_RecTaskStkBasePtr,
               (CPU_STK_SIZE)OSCfg_RecTaskStkLimit,
               (CPU_STK_SIZE)OSCfg_RecTaskStkSize,
               (OS_MSG_QTY  )0u,
               (OS_TICK     )0u,
               (void       *)0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR     *)p_err);
}
// ---------------------- AVL TREE PUBLIC FUNCTIONS ----------------------
// Recursive function to insert a key in the subtree rooted 
// with node and returns the new root of the subtree. 
void OSRecTaskCreate(OS_TASK_PTR taskPtr, OS_TICK releasePeriod, OS_TICK deadline, OS_ERR* err)
{
        if (OSRecTaskCount == OS_REC_MAX_TASKS)
        {
          *err = OS_ERR_Q_FULL;
          return;
        }
        *err = OS_ERR_NONE;
	if (OSRecTaskList.height == 0) 
        {
          OSRecTaskList.taskPrio = 0;
          OSRecTaskList.taskPtr = taskPtr;
          OSRecTaskList.height = 1;          
          OSRecTaskList.releasePeriod = releasePeriod; 
          OSRecTaskList.deadline = deadline;
          OSRecTaskList.nextRelease = (OS_TICK)0;
          OSRecTaskList.nextDeadline = releasePeriod + deadline; // this release time + deadline
        }
        else
        {
          insertRecursive(&OSRecTaskList, taskPtr, releasePeriod, deadline);
        }
}
// ---------------------- BIN HEAP PUBLIC FUNCTIONS ----------------------
// Inserts a new ready node
void OSRecRdyListInsert(OS_REC_TASK_NODE *n, OS_ERR *err) 
{ 
	if (OSRecRdyListCount == OS_REC_MAX_TASKS) 
	{ 
          *err = OS_ERR_Q_FULL;
          return; 
	} 
        *err = OS_ERR_NONE;
	// First insert the new key at the end 
	OSRecRdyListCount++; 
	int i = OSRecRdyListCount - 1; 
	OSRecRdyList[i] = n; 
        n->taskPrio = OSRecRdyListCount + OSCfg_RecTaskPrio + 1; // set prio accordingly to be used later
        
        
	// Fix the min heap property if it is violated 
	while (i != 0 && getBinHeapKey(OSRecRdyList[parent(i)]) > getBinHeapKey(OSRecRdyList[i])) 
	{ 
          swap(OSRecRdyList[i], OSRecRdyList[parent(i)]); 
          i = parent(i); 
	} 
} 
// Method to remove minimum element (or root) from min heap 
OS_REC_TASK_NODE* OSRecRdyListExtractNext(OS_ERR *err) 
{ 
	if (OSRecRdyListCount <= 0) 
	{
          *err = OS_ERR_Q_EMPTY;
          return (OS_REC_TASK_NODE*)0; 
        }
        *err = OS_ERR_NONE;
        
        OSRecRdyList[0]->taskPrio = OS_CFG_PRIO_CEILING; // invalidate prio
	if (OSRecRdyListCount == 1) 
	{ 
		OSRecRdyListCount--; 
		return OSRecRdyList[0]; 
	} 

	// Store the minimum value, and remove it from heap 
	OS_REC_TASK_NODE* root = OSRecRdyList[0]; 
	OSRecRdyList[0] = OSRecRdyList[OSRecRdyListCount-1]; 
	OSRecRdyListCount--; 
	MinHeapify(0); 

	return root; 
} 
CPU_BOOLEAN OSRecCheckNodeReady(OS_REC_TASK_NODE* n)
{
  return n->taskPrio != OS_CFG_PRIO_CEILING;
}