/*
Keep existing tick list, pend list and ready list for original tick tasks, idle tasks, ISR tasks etc.

Use AVL tree for task recursion's "dispatch list", aimed to keep track of tasks to dispatch.
Use Binary Heap for task scheduler's "recursive ready list", aimed to function as a separate ready list for keeping track of deadlines and adjusting task priority according to EDF rules
*/

/* Priority ranges between OS_CFG_REC_TASK_PRIO to OS_CFG_STAT_TASK_PRIO are RESERVED for recursive tasks */

#include <os.h>/* 
Recursion datastruct (task list): AVL tree, recursive tasks will be 
stored in AVL tree and be dispatched periodically
* designed to be read more often than write
* no provision to remove recursive tasks once created (due to malloc)
* pinging next task to queue will be implemented in tick task
* TODO: currently doesn't support multiple recursive tasks with same period!

Scheduler datastruct (ready list): Binary heap, recursive tasks ready to run will be 
stored in binary heap and scheduler will choose EDF job to run
* recursive tasks will have their entry removed from binary heap. if they are pre-empted, they will be added back
* recursive tasks run in their own task context. They can del themselves safely after they are done


*/
void printAVLTreeRec11(OS_REC_TASK_AVLTREE_NODE* n, int num)
{
  OS_REC_TASK_AVLTREE_NODE* tmp = n;
  OS_REC_TASK_AVLTREE_NODE* empty = (OS_REC_TASK_AVLTREE_NODE*)0;
printf("#%d:%d ", num, n->taskNode->nextRelease);
  if(tmp->left != empty)
  {
    printf("L ");
    printAVLTreeRec11(tmp->left, num+1);
  }
  if(tmp->right != empty)
  {
    printf("R ");
    printAVLTreeRec11(tmp->right, num+1);
  }
}
void printAVLTree11()
{
  printf("--- AVL tree ---\n");
  // pre-order traversal
  printAVLTreeRec11(OSRecTaskAvltreeListRoot, 0);
  printf("\n");
}
// C program to insert a node in AVL tree, modified from 
// https://www.geeksforgeeks.org/avl-tree-set-1-insertion/

// A AVL / binary tree element

// ---------------------- AVL TREE HELPER FUNCTIONS ----------------------
// A utility function to get maximum of two integers 
int max(int a, int b); 

// A utility function to get the height of the tree 
int getNodeHeight(OS_REC_TASK_AVLTREE_NODE *N) 
{ 
	if (N == (OS_REC_TASK_AVLTREE_NODE*)0) 
		return 0; 
	return N->height; 
} 

// A utility function to get maximum of two integers 
int max(int a, int b) 
{ 
	return (a > b)? a : b; 
} 

// A utility function to right rotate subtree rooted with y 
// See the diagram given above. 
OS_REC_TASK_AVLTREE_NODE *rightRotate(OS_REC_TASK_AVLTREE_NODE *y) 
{ 
	OS_REC_TASK_AVLTREE_NODE *x = y->left; 
	OS_REC_TASK_AVLTREE_NODE *T2 = x->right; 

	// Perform rotation 
	x->right = y; 
	y->left = T2; 

	// Update heights 
	y->height = max(getNodeHeight(y->left), getNodeHeight(y->right))+1; 
	x->height = max(getNodeHeight(x->left), getNodeHeight(x->right))+1; 

	// Return new root 
	return x; 
} 

// A utility function to left rotate subtree rooted with x 
// See the diagram given above. 
OS_REC_TASK_AVLTREE_NODE *leftRotate(OS_REC_TASK_AVLTREE_NODE *x) 
{ 
	OS_REC_TASK_AVLTREE_NODE *y = x->right; 
	OS_REC_TASK_AVLTREE_NODE *T2 = y->left; 

	// Perform rotation 
	y->left = x; 
	x->right = T2; 

	// Update heights 
	x->height = max(getNodeHeight(x->left), getNodeHeight(x->right))+1; 
	y->height = max(getNodeHeight(y->left), getNodeHeight(y->right))+1; 

	// Return new root 
	return y; 
} 

// Get Balance factor of node N 
int getBalance(OS_REC_TASK_AVLTREE_NODE *N) 
{ 
	if (N == (OS_REC_TASK_AVLTREE_NODE*)0) 
		return 0; 
	return getNodeHeight(N->left) - getNodeHeight(N->right); 
} 
/* Helper function that allocates a new node with the given key and 
	NULL left and right pointers. */
OS_REC_TASK_NODE* initNode(OS_TCB* taskTCB, OS_REC_TASK_NODE* node, OS_TASK_PTR taskPtr, CPU_STK* stkPtr, CPU_CHAR* taskName, OS_TICK releasePeriod, OS_TICK deadline)
{
        node->taskPrio = OS_CFG_PRIO_CEILING;
        node->tcb = taskTCB;
        node->tcb->RecursiveTaskNode = node;
        node->taskPtr = taskPtr;
        node->stkBasePtr = stkPtr;
        strcpy(node->taskName, taskName);
        
        node->next = (OS_REC_TASK_NODE*)0;
        
	node->releasePeriod = releasePeriod; 
        node->deadline = deadline;
        node->nextRelease = releasePeriod; // start out on 1st release time
        node->nextDeadline = 0;
        return node;
}
void initAvlNode(OS_REC_TASK_AVLTREE_NODE* node)
{
    node->left = (OS_REC_TASK_AVLTREE_NODE*)0;
    node->right = (OS_REC_TASK_AVLTREE_NODE*)0;
    node->height = 0;
    node->numTaskNodes = 0;
    node->taskNode = (OS_REC_TASK_NODE*)0;
}
OS_REC_TASK_AVLTREE_NODE* allocateAvlNode(OS_REC_TASK_NODE* node) // you must specify an initial node for a new avl node to be allocated to you
{
  if(OSRecTaskAvltreeListNumElements >= OS_REC_MAX_TASKS)
    return (OS_REC_TASK_AVLTREE_NODE*)0;
  
  OS_REC_TASK_AVLTREE_NODE* newAvlNode = &OSRecTaskAvltreeList[OSRecTaskAvltreeListNumElements];
  initAvlNode(newAvlNode);
  newAvlNode->taskNode = node;
  newAvlNode->numTaskNodes = 1;
  OSRecTaskAvltreeListNumElements++;
  return newAvlNode;
}
void copyAvlNode(OS_REC_TASK_AVLTREE_NODE* node,OS_REC_TASK_AVLTREE_NODE* src) 
{
  node->left = src->left;
  node->right = src->right;
  node->height = src->height;
  node->numTaskNodes = src->numTaskNodes;
  node->taskNode = src->taskNode;
}
void freeAvlNode(OS_REC_TASK_AVLTREE_NODE* node)
{
  OS_REC_TASK_AVLTREE_NODE* lastNode = &OSRecTaskAvltreeList[OSRecTaskAvltreeListNumElements-1];
  copyAvlNode(node, lastNode);
  initAvlNode(lastNode); // restore last avl node state to 0
  OSRecTaskAvltreeListNumElements--;
}
OS_REC_TASK_AVLTREE_NODE* insertRecursive(OS_REC_TASK_AVLTREE_NODE* avlNode, OS_REC_TASK_NODE* node, OS_TICK key) 
{ 
	/* 1. Perform the normal BST insertion */
	if (avlNode->numTaskNodes == 0)
        {
          return allocateAvlNode(node);
        }
	if (key < avlNode->taskNode->nextRelease) 
          avlNode->left = insertRecursive(avlNode->left, node, key); 
	else if (key > avlNode->taskNode->nextRelease) 
          avlNode->right = insertRecursive(avlNode->right, node, key); 
	else 
        {
          // TODO: Equal keys are not allowed in BST. Put at the end of Linked List inside AVLnode instead
          OS_REC_TASK_NODE* tmp = avlNode->taskNode;
          while(tmp->next != (OS_REC_TASK_NODE*)0)
          {
            tmp = tmp->next;
          }
          tmp->next = node;
          avlNode->numTaskNodes += 1;
              return avlNode; 
        }

	/* 2. Update height of this ancestor node */
	avlNode->height = 1 + max(getNodeHeight(avlNode->left), getNodeHeight(avlNode->right)); 

	/* 3. Get the balance factor of this ancestor 
		node to check whether this node became 
		unbalanced */
	int balance = getBalance(avlNode); 

	// If this node becomes unbalanced, then 
	// there are 4 cases 

	// Left Left Case 
	if (balance > 1 && key < avlNode->left->taskNode->nextRelease) 
		return rightRotate(avlNode); 

	// Right Right Case 
	if (balance < -1 && key > avlNode->right->taskNode->nextRelease) 
		return leftRotate(avlNode); 

	// Left Right Case 
	if (balance > 1 && key > avlNode->left->taskNode->nextRelease) 
	{ 
		avlNode->left = leftRotate(avlNode->left); 
		return rightRotate(avlNode); 
	} 

	// Right Left Case 
	if (balance < -1 && key < avlNode->right->taskNode->nextRelease) 
	{ 
		avlNode->right = rightRotate(avlNode->right); 
		return leftRotate(avlNode); 
	} 

	/* return the (unchanged) node pointer */
	return avlNode; 
}
// TODO: DEL recursive
/* Given a non-empty binary search tree, return the 
   node with minimum key value found in that tree. 
   Note that the entire tree does not need to be 
   searched. */
OS_REC_TASK_AVLTREE_NODE* minValueNode(OS_REC_TASK_AVLTREE_NODE* node) 
{ 
    OS_REC_TASK_AVLTREE_NODE* current = node; 
    /* loop down to find the leftmost leaf */
    while (current->left != (OS_REC_TASK_AVLTREE_NODE*)0) 
        current = current->left; 
    return current; 
} 
  
// Recursive function to delete a node with given key 
// from subtree with given root. It returns root of 
// the modified subtree. 
OS_REC_TASK_AVLTREE_NODE* deleteRecursive(OS_REC_TASK_AVLTREE_NODE* avlNode, OS_TICK key) 
{ 
//  OS_REC_TASK_AVLTREE_NODE
//  OSRecTaskAvltreeList
//    OSRecTaskAvltreeListNumElements

    // STEP 1: PERFORM STANDARD BST DELETE 
  
    if (avlNode->numTaskNodes == 0) 
        return (OS_REC_TASK_AVLTREE_NODE*)0; 
  
    // If the key to be deleted is smaller than the 
    // root's key, then it lies in left subtree 
    if ( key < avlNode->taskNode->nextRelease ) 
        avlNode->left = deleteRecursive(avlNode->left, key); 
  
    // If the key to be deleted is greater than the 
    // root's key, then it lies in right subtree 
    else if( key > avlNode->taskNode->nextRelease ) 
        avlNode->right = deleteRecursive(avlNode->right, key); 
  
    // if key is same as root's key, then This is 
    // the node to be deleted 
    else
    { 
      // we are removing this whole avl node
      // we always want to remove the whole avl node instead of single entries in taskNode linked list.
      // this is because we release the whole linked list of tasks together, and re-insert them into other different avl nodes based on their next release time
      
      
        // node with only one child or no child 
        if( (avlNode->left == (OS_REC_TASK_AVLTREE_NODE*)0) || (avlNode->right == (OS_REC_TASK_AVLTREE_NODE*)0) ) 
        {
           OS_REC_TASK_AVLTREE_NODE* temp = avlNode->left != (OS_REC_TASK_AVLTREE_NODE*)0 ? avlNode->left : 
                                             avlNode->right; 
  
            // No child case, just del
            if (temp == (OS_REC_TASK_AVLTREE_NODE*)0) 
            { 
              temp = avlNode;
              freeAvlNode(temp);
              return 0;
            } 
            else // One child case 
            {
              copyAvlNode(avlNode, temp);// Copy the contents of the non-empty child 
              freeAvlNode(temp);
              return avlNode;
            }
        } 
        else
        { 
            // node with two children: Get the inorder 
            // successor (smallest in the right subtree) 
            OS_REC_TASK_AVLTREE_NODE* temp = minValueNode(avlNode->right); 
  
            // Copy the inorder successor's data to this node 
            copyAvlNode(avlNode, temp);
  
            // Delete the inorder successor 
            avlNode->right = deleteRecursive(avlNode->right, key); 
        } 
    } 
  
    // If the tree had only one node then return 
    if (avlNode == (OS_REC_TASK_AVLTREE_NODE*)0) 
      return avlNode; 
  
    // STEP 2: UPDATE HEIGHT OF THE CURRENT NODE 
    avlNode->height = 1 + max(getNodeHeight(avlNode->left), 
                           getNodeHeight(avlNode->right)); 
  
    // STEP 3: GET THE BALANCE FACTOR OF THIS NODE (to 
    // check whether this node became unbalanced) 
    int balance = getBalance(avlNode); 
  
    // If this node becomes unbalanced, then there are 4 cases 
  
    // Left Left Case 
    if (balance > 1 && getBalance(avlNode->left) >= 0) 
        return rightRotate(avlNode); 
  
    // Left Right Case 
    if (balance > 1 && getBalance(avlNode->left) < 0) 
    { 
        avlNode->left =  leftRotate(avlNode->left); 
        return rightRotate(avlNode); 
    } 
  
    // Right Right Case 
    if (balance < -1 && getBalance(avlNode->right) <= 0) 
        return leftRotate(avlNode); 
  
    // Right Left Case 
    if (balance < -1 && getBalance(avlNode->right) > 0) 
    { 
        avlNode->right = rightRotate(avlNode->right); 
        return leftRotate(avlNode); 
    } 
  
    return avlNode; 
} 

// ---------------------- BIN HEAP HELPER FUNCTIONS ----------------------
int parent(int i) { return (i-1)/2; } 

// to get index of left child of node at index i 
int left(int i) { return (2*i + 1); } 

// to get index of right child of node at index i 
int right(int i) { return (2*i + 2); } 

OS_TICK getAvlTreeKey(OS_REC_TASK_NODE* n) { return n->nextRelease; }
OS_TICK getBinHeapKey(OS_REC_TASK_NODE* n) { return n->nextDeadline; }

// A recursive method to heapify a subtree with the root at given index 
// This method assumes that previous insertions are done properly
void MinHeapify(int i) 
{ 
    int l = left(i); 
    int r = right(i); 
    int smallest = i; 
    if (l < OSRecRdyListCount && getBinHeapKey(OSRecRdyList[l]) < getBinHeapKey(OSRecRdyList[i])) 
            smallest = l; 
    if (r < OSRecRdyListCount && getBinHeapKey(OSRecRdyList[r]) < getBinHeapKey(OSRecRdyList[smallest])) 
            smallest = r; 
    if (smallest != i) 
    { 
      // swap the 2 pointers
      OS_REC_TASK_NODE* tmp = OSRecRdyList[i]; 
      OSRecRdyList[i] = OSRecRdyList[smallest];
      OSRecRdyList[smallest] = tmp;
      MinHeapify(smallest); 
    } 
} 
// ------------------------------------------- PUBLIC FUNCS -------------------------------
void  OS_RecTask (void *p_arg)
{
// NO recursive task may have its priority higher than this task!
  OS_ERR err;
  OS_REC_TASK_NODE* incomingNode;
  OS_MSG_SIZE incomingNodeSize;
  CPU_TS ts;
  OS_TICK prevTick;
  
  while(DEF_ON)
  {
    // insert that runnable task into binary heap
    // the running task should remain at the bin heap root if not pre-empted.
    // take earliest deadline task from binary heap, run it.
    
    // will receive a node either from tick task releasing recursive tasks, or from OSTaskDel from recursive tasks requesting to delete themselves
     incomingNode = (OS_REC_TASK_NODE*)OSTaskQPend(0, OS_OPT_PEND_BLOCKING, &incomingNodeSize, &ts, &err);
 
     prevTick = OSTimeGet(&err);    
     if(OSRecCheckNodeReady(incomingNode) == DEF_FALSE)
     {
       // this task wants to run!
       // save current running task
       OS_REC_TASK_NODE* prevRunningTask = OSRecRdyList[0];
       // set the task deadlines here instead of tick task 
       incomingNode->nextDeadline += incomingNode->deadline;
       OSRecRdyListInsert(incomingNode, &err); // this sets the taskPrio automatically
       // demote priority of previous running task now that it is sorted down somewhere into the binary heap
       if(prevRunningTask != (OS_REC_TASK_NODE*)0 && prevRunningTask->taskPrio != OS_CFG_PRIO_CEILING)
         OSTaskChangePrio(prevRunningTask->tcb, prevRunningTask->taskPrio, &err);
       // the old task gave way. Now make that new task
       // TODO: what if a previous instance of that task has not completed when it was released again?
       
       OSTaskCreate((OS_TCB     *)incomingNode->tcb,
                 (CPU_CHAR   *)((void *)incomingNode->taskName),
                 (OS_TASK_PTR )incomingNode->taskPtr,
                 (void       *)0,
                 (OS_PRIO     )incomingNode->taskPrio,
                 (CPU_STK    *)incomingNode->stkBasePtr,
                 (CPU_STK_SIZE)OSCfg_RecTaskInstanceStkLimit,
                 (CPU_STK_SIZE)OSCfg_RecTaskInstanceStkSize,
                 (OS_MSG_QTY  )0u,
                 (OS_TICK     )0u,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_RECURSIVE),
                 (OS_ERR     *)&err);
      // TODO: use ts to calc time taken, in particular, OSTimeGet(&err) - ts}
      printf("Task %s created %d, bin heap insertion took %d\n", incomingNode->taskName, OSTimeGet(&err), OSTimeGet(&err)-prevTick);
     }
     else
     {
       // this task is just being deleted! revert its prio back to nominal
       // Note: TCB should still be intact since deleted task guranteed to have priority lower than this task
       if(incomingNode != OSRecRdyList[0])
       {
          // this should not happen. TODO: handle/display error
         int i=0;
         while(i < OSRecRdyListCount && incomingNode != OSRecRdyList[i])
           i++;
         //printf("Deleted task not running! It is at index %d out of %d.\n", i, OSRecRdyListCount);
       }
       OS_REC_TASK_NODE* completedTask = OSRecRdyListExtractNext(&err); // this invalidates the taskPrio automatically
       if(OSRecRdyListCount != 0)
         OSTaskChangePrio(OSRecRdyList[0]->tcb, OSRecRdyList[0]->taskPrio, &err); // refresh new root node's task priority to take removed node's place
       // OSSched should handle the rest
       printf("Task %s completed in %d, bin heap deletion took %d\n", completedTask->taskName, OSTimeGet(&err)-completedTask->nextRelease+completedTask->releasePeriod, OSTimeGet(&err)-prevTick);
       printf("Task %s next sched for %d.\n", completedTask->taskName, completedTask->nextRelease);
     }
  }
}
void OS_RecTaskInit(OS_ERR *p_err)
{
  if (OSCfg_RecTaskPrio >= (OS_CFG_PRIO_MAX - 1u)) {     /* Only one task at the 'Idle Task' priority              */
        *p_err = OS_ERR_PRIO_INVALID;
        return;
   }
  *p_err = OS_ERR_NONE;
  
  OSRecTaskCount = 0;
  OSRecRdyListCount = 0;
  OSRecTaskAvltreeListRoot = &OSRecTaskAvltreeList[0];
  for(OS_PRIO i=0; i<OS_REC_MAX_TASKS; i++)
  {
    OSRecRdyList[i] = (OS_REC_TASK_NODE*)0;
    initAvlNode(&OSRecTaskAvltreeList[i]);
    initNode((OS_TCB*)0, &OSRecTaskList[i], (void*)0, (CPU_STK*)0, (CPU_CHAR*)"-", 0, 0);
  }

  OSTaskCreate((OS_TCB     *)&OSRecTaskTCB,
               (CPU_CHAR   *)((void *)"uC/OS-III Recursive Task Scheduler"),
               (OS_TASK_PTR )OS_RecTask,
               (void       *)0,
               (OS_PRIO     )OSCfg_RecTaskPrio,
               (CPU_STK    *)OSCfg_RecTaskStkBasePtr,
               (CPU_STK_SIZE)OSCfg_RecTaskStkLimit,
               (CPU_STK_SIZE)OSCfg_RecTaskStkSize,
               (OS_MSG_QTY  )OS_REC_MAX_TASKS, // the queue must be able to contain all tasks being dispatched by tick task at once
               (OS_TICK     )0u,
               (void       *)0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR     *)p_err);
}
// ---------------------- AVL TREE PUBLIC FUNCTIONS ----------------------
// Recursive function to insert a key in the subtree rooted 
// with node and returns the new root of the subtree. 
// MUST BE CALLED BEFORE OSStart()!!
void OSRecTaskCreate(OS_TCB* taskTCB, OS_TASK_PTR taskPtr, CPU_CHAR* taskName, OS_TICK releasePeriod, OS_TICK deadline, OS_ERR* err)
{
        if (OSRecTaskCount == OS_REC_MAX_TASKS)
        {
          *err = OS_ERR_Q_FULL;
          return;
        }
        *err = OS_ERR_NONE;
        
        CPU_STK* thisStackPtr = OSCfg_RecTaskInstanceStkBasePtr + (OSRecTaskCount * OSCfg_RecTaskInstanceStkSize);
        OSRecTaskCount++;
        
        // make that node
        OS_REC_TASK_NODE *newNode = &OSRecTaskList[OSRecTaskListNumElements];
        initNode(taskTCB, newNode, taskPtr, thisStackPtr, taskName, releasePeriod, deadline);
        OSRecTaskListNumElements++;
        // put it into AVL tree node
        OSRecTaskAvltreeListRoot = insertRecursive(OSRecTaskAvltreeListRoot, newNode, releasePeriod);
        printAVLTree11();
}
void OSRecReleaseListInsert(OS_REC_TASK_NODE *node, OS_ERR *err)
{
  node->next = (OS_REC_TASK_NODE*)0; // make the new node forget its previous neighbour
  OSRecTaskAvltreeListRoot = insertRecursive(OSRecTaskAvltreeListRoot, node, getAvlTreeKey(node));
  if(OSRecTaskAvltreeListRoot != (OS_REC_TASK_AVLTREE_NODE*)0)
    *err = OS_ERR_NONE;
  else
    *err = OS_ERR_Q_FULL;
}
void OSRecReleaseListRemove(OS_REC_TASK_NODE *node, OS_ERR *err)
{
  OSRecTaskAvltreeListRoot = deleteRecursive(OSRecTaskAvltreeListRoot, getAvlTreeKey(node));
  if(OSRecTaskAvltreeListRoot != (OS_REC_TASK_AVLTREE_NODE*)0)
    *err = OS_ERR_NONE;
  else
    *err = OS_ERR_Q_FULL;
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
        n->taskPrio = OSRecRdyListCount + OSCfg_RecTaskPrio; // set prio to be in range (rec task prio, stat task prio)
        
        
	// Fix the min heap property if it is violated 
	while (i != 0 && getBinHeapKey(OSRecRdyList[parent(i)]) > getBinHeapKey(OSRecRdyList[i])) 
	{ 
          // swap the 2 pointers
          OS_REC_TASK_NODE* tmp = OSRecRdyList[i]; 
          OSRecRdyList[i] = OSRecRdyList[parent(i)];
          OSRecRdyList[parent(i)] = tmp;
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
        
        OSRecRdyList[0]->taskPrio = OS_CFG_PRIO_CEILING;
	OSRecRdyListCount--;
	if (OSRecRdyListCount == 0)  // if only one element remaining
	{
          return OSRecRdyList[0]; 
	} 

	// Store the minimum value, and remove it from heap 
	OS_REC_TASK_NODE* root = OSRecRdyList[0]; 
	OSRecRdyList[0] = OSRecRdyList[OSRecRdyListCount]; 
	MinHeapify(0); 

	return root; 
} 
CPU_BOOLEAN OSRecCheckNodeReady(OS_REC_TASK_NODE* n)
{
  return n->taskPrio != OS_CFG_PRIO_CEILING;
}