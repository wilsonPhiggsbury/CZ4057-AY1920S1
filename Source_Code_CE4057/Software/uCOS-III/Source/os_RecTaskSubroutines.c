/* 
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
// C program to insert a node in AVL tree, modified from 
// https://www.geeksforgeeks.org/avl-tree-set-1-insertion/
#include<stdio.h> 
#include<stdlib.h> 
#include <os.h>
// --- BIN HEAP ---
#define CPU_INT16U_MAX (CPU_INT16U)-1
#define CPU_INT32U_MAX (CPU_INT32U)-1

// A AVL / binary tree element

// ---------------------- AVL TREE HELPER FUNCTIONS ----------------------
// A utility function to get maximum of two integers 
int max(int a, int b); 

// A utility function to get the height of the tree 
int getNodeHeight(OS_REC_TASK_NODE *N) 
{ 
	if (N == NULL) 
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
OS_REC_TASK_NODE *rightRotate(OS_REC_TASK_NODE *y) 
{ 
	OS_REC_TASK_NODE *x = y->left; 
	OS_REC_TASK_NODE *T2 = x->right; 

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
OS_REC_TASK_NODE *leftRotate(OS_REC_TASK_NODE *x) 
{ 
	OS_REC_TASK_NODE *y = x->right; 
	OS_REC_TASK_NODE *T2 = y->left; 

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
int getBalance(OS_REC_TASK_NODE *N) 
{ 
	if (N == NULL) 
		return 0; 
	return getNodeHeight(N->left) - getNodeHeight(N->right); 
} 
/* Helper function that allocates a new node with the given key and 
	NULL left and right pointers. */
OS_REC_TASK_NODE* initNode(OS_REC_TASK_NODE* node, OS_TASK_PTR taskPtr, OS_TICK releasePeriod, OS_TICK deadline)
{
	node->left = NULL; 
	node->right = NULL; 
	node->height = 1; // initialized as leaf node
        node->taskPrio = OS_CFG_PRIO_CEILING;
        node->tcb = malloc(sizeof(OS_TCB));
        node->tcb->RecursiveTaskNode = node;
        node->taskPtr = taskPtr;
        
	node->releasePeriod = releasePeriod; 
        node->deadline = deadline;
        node->nextRelease = (OS_TICK)0;
        node->nextDeadline = node->releasePeriod + deadline; // this release time + deadline
        return node;
}
OS_REC_TASK_NODE* insertRecursive(OS_REC_TASK_NODE* node, OS_TASK_PTR taskPtr, OS_TICK releasePeriod, OS_TICK deadline) 
{ 
	/* 1. Perform the normal BST insertion */
  
	if (node == NULL) 
        {
          OS_REC_TASK_NODE* n = (OS_REC_TASK_NODE*) malloc(sizeof(OS_REC_TASK_NODE)); 
          initNode(n, taskPtr, releasePeriod, deadline); 
          return n;
        } 
	if (releasePeriod < node->releasePeriod) 
		node->left = insertRecursive(node->left, taskPtr, releasePeriod, deadline); 
	else if (releasePeriod > node->releasePeriod) 
		node->right = insertRecursive(node->right, taskPtr, releasePeriod, deadline); 
	else // TODO: Equal keys are not allowed in BST 
		return node; 

	/* 2. Update height of this ancestor node */
	node->height = 1 + max(getNodeHeight(node->left), getNodeHeight(node->right)); 

	/* 3. Get the balance factor of this ancestor 
		node to check whether this node became 
		unbalanced */
	int balance = getBalance(node); 

	// If this node becomes unbalanced, then 
	// there are 4 cases 

	// Left Left Case 
	if (balance > 1 && releasePeriod < node->left->releasePeriod) 
		return rightRotate(node); 

	// Right Right Case 
	if (balance < -1 && releasePeriod > node->right->releasePeriod) 
		return leftRotate(node); 

	// Left Right Case 
	if (balance > 1 && releasePeriod > node->left->releasePeriod) 
	{ 
		node->left = leftRotate(node->left); 
		return rightRotate(node); 
	} 

	// Right Left Case 
	if (balance < -1 && releasePeriod < node->right->releasePeriod) 
	{ 
		node->right = rightRotate(node->right); 
		return leftRotate(node); 
	} 

	/* return the (unchanged) node pointer */
	return node; 
} 

// ---------------------- BIN HEAP HELPER FUNCTIONS ----------------------
// A utility function to swap two elements 
void swap(OS_REC_TASK_NODE *x, OS_REC_TASK_NODE *y) 
{ 
    OS_REC_TASK_NODE temp = *x; 
    *x = *y; 
    *y = temp; 
    // swap the priority as well to be used later
    OS_PRIO temp2 = x->taskPrio;
    x->taskPrio = y->taskPrio;
    y->taskPrio = temp2;
} 
int parent(int i) { return (i-1)/2; } 

// to get index of left child of node at index i 
int left(int i) { return (2*i + 1); } 

// to get index of right child of node at index i 
int right(int i) { return (2*i + 2); } 

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
            swap(OSRecRdyList[i], OSRecRdyList[smallest]); 
            MinHeapify(smallest); 
    } 
} 