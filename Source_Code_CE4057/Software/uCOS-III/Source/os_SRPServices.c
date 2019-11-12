// __ SRPBlockedTasks[] (arr of tasknodes) len OS_SRP_MAX_MUTEXES
// __ SRPBlockedMutexes (bitmap)len OS_SRP_MAX_MUTEXES. INIT TO 0s
// __ SRPResourceCeilings[]
// __ SRPSystemCeiling
// __ struct SRPMutex
// __ OS_SRP_MAX_MUTEXES
// __ OSSRPMutexCount
#include <os.h>
/* SRPMutex represented using its index only.
*/
void pushMutexToStack(OS_REC_TASK_NODE* n, int mutexIndex)
{
  if(n->mutexStackPtr > OS_SRP_MAX_MUTEXES)
  {
    printf("Task mutex stack full! Mutex not pushed...\n");
    return;
  }
  n->mutexStack[n->mutexStackPtr] = mutexIndex;
  (n->mutexStackPtr)++;
}
int popMutexFromStack(OS_REC_TASK_NODE* n)
{
  if(n->mutexStackPtr == 0)
  {
    printf("Task mutex stack empty! Mutex not popped...\n");
    return -1;
  }
  (n->mutexStackPtr)--;
  int tmp = n->mutexStack[n->mutexStackPtr];
  n->mutexStack[n->mutexStackPtr] = 0;
  return tmp;
}
int OS_SRPMutexCreate(int resourceCeiling, OS_ERR* err)
{
  if (OSSRPMutexCount == OS_SRP_MAX_MUTEXES)
  {
    *err = OS_ERR_Q_FULL;
    return -1;
  }
  else
    *err = OS_ERR_NONE;
  SRPResourceCeilings[OSSRPMutexCount] = resourceCeiling;
  OSSRPMutexCount++;
  return OSSRPMutexCount-1;
}
// these 2 are called from application code. they merely signal the srp task
void OS_SRPMutexPend(OS_REC_TASK_NODE* n, int mutexIndex)
{
  OS_ERR err;
  CPU_TS32 t = CPU_TS_Get32();
  OS_TICK ts = OSTimeGet(&err);
  OS_SRP_MUTEX_QUEUEOBJ signal;
  signal.taskNode = n;
  signal.mutexIndex = mutexIndex;
  signal.isPend = 1;
  OSTaskQPost(&OSSRPTaskTCB, (void*)&signal, (OS_MSG_SIZE)sizeof(OS_SRP_MUTEX_QUEUEOBJ), OS_OPT_POST_FIFO, &err);
  printf("Task %s time to acquire mutex: %d/%d\n", n->taskName, OSTimeGet(&err)-ts, CPU_TS32_to_uSec(CPU_TS_Get32()-t));
}
void OS_SRPMutexPost(OS_REC_TASK_NODE* n, int mutexIndex)
{
  OS_ERR err;
  CPU_TS32 t = CPU_TS_Get32();
  OS_TICK ts = OSTimeGet(&err);
  OS_SRP_MUTEX_QUEUEOBJ signal;
  signal.taskNode = n;
  signal.mutexIndex = mutexIndex;
  signal.isPend = 0;
  OSTaskQPost(&OSSRPTaskTCB, (void*)&signal, (OS_MSG_SIZE)sizeof(OS_SRP_MUTEX_QUEUEOBJ), OS_OPT_POST_FIFO, &err);
  printf("Task %s time to release mutex: %d/%d\n", n->taskName, OSTimeGet(&err)-ts, CPU_TS32_to_uSec(CPU_TS_Get32()-t));
}
// ------------------------------------
void pendMutex(OS_REC_TASK_NODE* n, int mutexIndex)
{
  OS_ERR err;
  if(n->isHoldingMutex != 0 || (n->preemptionLevel > SRPSystemCeiling && SRPBlockedMutexes[mutexIndex] == 0))
  {
    // allow 
    n->isHoldingMutex++;
    SRPSystemCeiling = SRPSystemCeiling > SRPResourceCeilings[mutexIndex] ? SRPSystemCeiling : SRPResourceCeilings[mutexIndex];
    SRPBlockedMutexes[mutexIndex] = 1;
    pushMutexToStack(n, mutexIndex);
    printf("+++Mutex %d allocated to task %s.\n", mutexIndex, n->taskName);
  }
  else
  {
    printf("!!!Mutex %d not acquired by task %s.\n", mutexIndex, n->taskName);
    // block. insert into block list
    OSTaskSuspend(n->tcb, &err);
    // TODO: time ll insert
    OS_REC_TASK_NODE* a = SRPBlockedTasks[mutexIndex];
    if(a == (OS_REC_TASK_NODE*)0)
    {
      SRPBlockedTasks[mutexIndex] = n;
    }
    else
    {
      while(a->nextBlocked != (OS_REC_TASK_NODE*)0)
      {
        a = a->nextBlocked;
      }
      a->nextBlocked = n;
    }
    // remove from rec ready list
    // TODO ...
  }
}
void postMutex(OS_REC_TASK_NODE* n)
{
  OS_ERR err;
  n->isHoldingMutex--;
  int mutexIndex = popMutexFromStack(n);
  if(mutexIndex == -1)
  {
    printf("!!!Mutex not popped, task %s stack empty!\n", mutexIndex, n->taskName);
    return;
  }
  
  // restore ceiling
  SRPSystemCeiling = 0;
  // go through blocked mutexes to get highest resource ceiling, else default to 0
  SRPBlockedMutexes[mutexIndex] = 0;
  for(int i=0; i<OSSRPMutexCount; i++)
  {
    if(SRPBlockedMutexes[i] == 1 && SRPResourceCeilings[i] > SRPSystemCeiling)
    {
      SRPSystemCeiling = SRPResourceCeilings[i];
    }
  }
  printf("---Mutex %d released by task %s. Sysceil restored to %d.\n", mutexIndex, n->taskName, SRPSystemCeiling);
  // resume the next blocked task waiting on this mutex. detach "nextBlocked LL ptr" of the resumed task's parent node
  // block back the mutex again if this is the case
  // TODO: time ll remove
  OS_REC_TASK_NODE* a = SRPBlockedTasks[mutexIndex];
  if(a == (OS_REC_TASK_NODE*)0)
  {
    
  }
  else
  {
    if(a->nextBlocked == (OS_REC_TASK_NODE*)0)
    {
      OSTaskResume(a->tcb, &err);
      SRPBlockedTasks[mutexIndex] = (OS_REC_TASK_NODE*)0;
      // take the mutex again 
      SRPSystemCeiling = SRPResourceCeilings[mutexIndex];
      SRPBlockedMutexes[mutexIndex] = 1;
      pushMutexToStack(a, mutexIndex);
      printf("+++Mutex %d allocated to task %s.\n", mutexIndex, n->taskName);
    }
    else
    {
      while(a->nextBlocked->nextBlocked != (OS_REC_TASK_NODE*)0)
      {
        a = a->nextBlocked;
      }
      OSTaskResume(a->nextBlocked->tcb, &err);
      OS_REC_TASK_NODE* tmp = a->nextBlocked;
      a->nextBlocked = (OS_REC_TASK_NODE*)0;
      // take the mutex again
      SRPSystemCeiling = SRPResourceCeilings[mutexIndex];
      SRPBlockedMutexes[mutexIndex] = 1;
      pushMutexToStack(tmp, mutexIndex);
      printf("+++Mutex %d allocated to task %s.\n", mutexIndex, n->taskName);
    }
  }
}
void OS_SRPTaskInit(OS_ERR* p_err)
{
  for(int i=0; i<OS_SRP_MAX_MUTEXES; i++)
  {
    SRPBlockedTasks[i] = (OS_REC_TASK_NODE*)0;
    SRPResourceCeilings[i] = 0;
    SRPBlockedMutexes[i] = 0;
  }
  SRPSystemCeiling = 0;
  OSSRPMutexCount = 0;
  
  OSTaskCreate((OS_TCB     *)&OSSRPTaskTCB,
               (CPU_CHAR   *)((void *)"uC/OS-III SRP Task"),
               (OS_TASK_PTR )OS_SRPTask,
               (void       *)0,
               (OS_PRIO     )OSCfg_SRPTaskPrio,
               (CPU_STK    *)OSCfg_SRPTaskStkBasePtr,
               (CPU_STK_SIZE)OSCfg_SRPTaskStkLimit,
               (CPU_STK_SIZE)OSCfg_SRPTaskStkSize,
               (OS_MSG_QTY  )OS_REC_MAX_TASKS, // the queue must only handle 1 service
               (OS_TICK     )0u,
               (void       *)0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR     *)p_err);
}
void OS_SRPTask(void* p_arg)
{
  OS_ERR err;
  OS_SRP_MUTEX_QUEUEOBJ* incomingNode;
  OS_MSG_SIZE incomingNodeSize;
  CPU_TS ts;
  
  // empty the rec rdy list to assign their preemption level!
  int i = OSRecTaskCount;
  OS_REC_TASK_NODE* n = OSRecRdyListExtractNext(&err);
  while(n != (OS_REC_TASK_NODE*)0)
  {
    n->preemptionLevel = i;
    n = OSRecRdyListExtractNext(&err);
    printf("Assigned task %s a preemption level of %d\n", n->taskName, i);
    i--;
  }
  
  while(DEF_ON)
  {
    // will receive a struct containing taskNode, SRPMutex, 
    incomingNode = (OS_SRP_MUTEX_QUEUEOBJ*)OSTaskQPend(0, OS_OPT_PEND_BLOCKING, &incomingNodeSize, &ts, &err);
    if(incomingNode->isPend)
    {
      pendMutex(incomingNode->taskNode, incomingNode->mutexIndex);
    }
    else
    {
      postMutex(incomingNode->taskNode);
    }  
  }
}