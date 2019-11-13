/*
*********************************************************************************************************
*                                              EXAMPLE CODE
*
*                          (c) Copyright 2009-2010; Micrium, Inc.; Weston, FL
*
*               All rights reserved.  Protected by international copyright laws.
*               Knowledge of the source code may NOT be used to develop a similar product.
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                            EXAMPLE CODE
*
* Filename      : app.c
* Version       : V1.00
* Programmer(s) : FUZZI
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include <includes.h>
#include <stdio.h>
#include "driverlib/timer.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"

#include "ext/SD_Card.h"

/*
*********************************************************************************************************
*                                             LOCAL DEFINES
*********************************************************************************************************
*/

#define ONESECONDTICK             7000000

#define TASK1PERIOD                   3
#define TASK2PERIOD                   5
#define TASK3PERIOD                   7

#define WORKLOAD1                     1
#define WORKLOAD2                     1
#define WORKLOAD3                     3

#define TIMERDIV                      (BSP_CPUClkFreq() / (CPU_INT32U)OSCfg_TickRate_Hz)

#define MICROSD_EN                    1
#define TIMER_EN                      0

/*
*********************************************************************************************************
*                                            LOCAL VARIABLES
*********************************************************************************************************
*/

static  OS_TCB       AppTaskStartTCB;
static  CPU_STK      AppTaskStartStk[APP_TASK_START_STK_SIZE];

static  OS_TCB       AppTaskOneTCB;
static  CPU_STK      AppTaskOneStk[APP_TASK_ONE_STK_SIZE];

static  OS_TCB       AppTaskTwoTCB;
static  CPU_STK      AppTaskTwoStk[APP_TASK_TWO_STK_SIZE];

static  OS_TCB       AppTaskThreeTCB;
static  CPU_STK      AppTaskThreeStk[APP_TASK_THREE_STK_SIZE];

static  OS_TCB       AppTaskRepeatOneTCB;
static  CPU_STK      AppTaskRepeatOneStk[APP_TASK_ONE_STK_SIZE];

static  OS_TCB       AppTaskRepeatTwoTCB;
static  CPU_STK      AppTaskRepeatTwoStk[APP_TASK_TWO_STK_SIZE];

static  OS_TCB       AppTaskRepeatThreeTCB;
static  CPU_STK      AppTaskRepeatThreeStk[APP_TASK_THREE_STK_SIZE];

OS_Q            DummyQ;
OS_MUTEX        MutexOne, MutexTwo, MutexThree;

CPU_INT32U      iCnt = 0;
CPU_INT08U      Left_tgt;
CPU_INT08U      Right_tgt;

CPU_INT32U      iToken  = 0;
CPU_INT32U      iCounter= 1;
CPU_INT32U      iMove   = 10;

CPU_CHAR        g_cCmdBuf[30] = {'n','e','w',' ','l','o','g','.','t','x','t','\0'};

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

        void        IntWheelSensor                    ();
static  void        AppRobotMotorDriveSensorEnable    ();
static  void        AppRobotMotorDriveSensorDisable   ();

        void        RoboTurn                          (tSide dir, CPU_INT16U seg, CPU_INT16U speed);

static  void        AppTaskStart                 (void  *p_arg);
static  void        AppTaskOne                   (void  *p_arg);
static  void        AppTaskTwo                   (void  *p_arg);
static  void        AppTaskThree                 (void  *p_arg);

static  void        AppTaskRepeatOne             (void  *p_arg);
static  void        AppTaskRepeatTwo             (void  *p_arg);
static  void        AppTaskRepeatThree           (void  *p_arg);

#if(TIMER_EN == 1)
extern void TimerReset(void);
extern unsigned long TimerTick(void);
unsigned long iTick1, iTick2, iTick3,iTick4;
#endif

/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*********************************************************************************************************
*/

int  main (void)
{
    OS_ERR  err;

    BSP_IntDisAll();                                            /* Disable all interrupts.                              */
    OSInit(&err);                                               /* Init uC/OS-III.                                      */

    OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,           /* Create the start task                                */
                 (CPU_CHAR   *)"App Task Start",
                 (OS_TASK_PTR ) AppTaskStart,
                 (void       *) 0,
                 (OS_PRIO     ) APP_TASK_START_PRIO,
                 (CPU_STK    *)&AppTaskStartStk[0],
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE / 10u,
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) (CPU_INT32U) 0, 
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

    OSStart(&err);                                              /* Start multitasking (i.e. give control to uC/OS-III). */
}


/*
*********************************************************************************************************
*                                          STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/

static  void  AppTaskStart (void  *p_arg)
{
    CPU_INT32U  clk_freq;
    CPU_INT32U  ulPHYMR0;
    CPU_INT32U  cnts;
    OS_ERR      err;

   (void)&p_arg;

    BSP_Init();                                                 /* Initialize BSP functions                             */
    CPU_Init();                                                 /* Initialize the uC/CPU services                       */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);                  /* Enable and Reset the Ethernet Controller.            */
    SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);
    ulPHYMR0 = EthernetPHYRead(ETH_BASE, PHY_MR0);              /* Power Down PHY                                       */
    EthernetPHYWrite(ETH_BASE, PHY_MR0, ulPHYMR0 | PHY_MR0_PWRDN);
    SysCtlPeripheralDeepSleepDisable(SYSCTL_PERIPH_ETH);
    clk_freq = BSP_CPUClkFreq();                                /* Determine SysTick reference freq.                    */
    cnts     = clk_freq / (CPU_INT32U)OSCfg_TickRate_Hz;        /* Determine nbr SysTick increments                     */
    OS_CPU_SysTickInit(cnts);                                   /* Init uC/OS periodic time src (SysTick).              */
    CPU_TS_TmrFreqSet(clk_freq);
    
    #if(MICROSD_EN == 1)
    /* Mount the file system, using logical disk 0 */
    f_mount(0, &g_sFatFs);
    /* Create a new log.txt file */
    CmdLineProcess(g_cCmdBuf);
    #endif

    /* Enable Wheel ISR Interrupt */
    AppRobotMotorDriveSensorEnable();
    
    /* Create Dummy Queue */
    OSQCreate((OS_Q *)&DummyQ, (CPU_CHAR *)"Dummy Queue", (OS_MSG_QTY)5, (OS_ERR *)&err);
    
    /* Create Mutexes */
    OSMutexCreate((OS_MUTEX *)&MutexOne, (CPU_CHAR *)2, (OS_ERR *)&err);
    OSMutexCreate((OS_MUTEX *)&MutexTwo, (CPU_CHAR *)3, (OS_ERR *)&err);
    OSMutexCreate((OS_MUTEX *)&MutexThree, (CPU_CHAR *)3, (OS_ERR *)&err);

    /* Initialise the 3 Main Tasks to  Deleted State */
    OSTaskCreate((OS_TCB     *)&AppTaskOneTCB, (CPU_CHAR   *)"App Task One", (OS_TASK_PTR ) AppTaskOne, (void       *) 0, (OS_PRIO     ) APP_TASK_ONE_PRIO, (CPU_STK    *)&AppTaskOneStk[0], (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *)(CPU_INT32U) 1, (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
    OSTaskDel((OS_TCB *)&AppTaskOneTCB, &err);
    OSTaskCreate((OS_TCB     *)&AppTaskTwoTCB, (CPU_CHAR   *)"App Task Two", (OS_TASK_PTR ) AppTaskTwo, (void       *) 0, (OS_PRIO     ) APP_TASK_TWO_PRIO, (CPU_STK    *)&AppTaskTwoStk[0], (CPU_STK_SIZE) APP_TASK_TWO_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_TWO_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) 2, (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
    OSTaskDel((OS_TCB *)&AppTaskTwoTCB, &err);
    OSTaskCreate((OS_TCB     *)&AppTaskThreeTCB, (CPU_CHAR   *)"App Task Three", (OS_TASK_PTR ) AppTaskThree, (void       *) 0, (OS_PRIO     ) APP_TASK_THREE_PRIO, (CPU_STK    *)&AppTaskThreeStk[0], (CPU_STK_SIZE) APP_TASK_THREE_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_THREE_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) 3, (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
    OSTaskDel((OS_TCB *)&AppTaskThreeTCB, &err);

    /* Start!! */
    BSP_DisplayStringDraw("Go",0u, 1u);
    
    /* Tasks that recreate the 3 main tasks */
    OSTaskCreate((OS_TCB     *)&AppTaskRepeatOneTCB, (CPU_CHAR   *)"Repeat Task One", (OS_TASK_PTR ) AppTaskRepeatOne, (void       *) 0, (OS_PRIO     ) APP_TASK_REPEAT_ONE_PRIO, (CPU_STK    *)&AppTaskRepeatOneStk[0], (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) 1, (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
    OSTaskCreate((OS_TCB     *)&AppTaskRepeatTwoTCB, (CPU_CHAR   *)"Repeat Task Two", (OS_TASK_PTR ) AppTaskRepeatTwo, (void       *) 0, (OS_PRIO     ) APP_TASK_REPEAT_TWO_PRIO, (CPU_STK    *)&AppTaskRepeatTwoStk[0], (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) 2, (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
    OSTaskCreate((OS_TCB     *)&AppTaskRepeatThreeTCB, (CPU_CHAR   *)"Repeat Task Three", (OS_TASK_PTR ) AppTaskRepeatThree, (void       *) 0, (OS_PRIO     ) APP_TASK_REPEAT_THREE_PRIO, (CPU_STK    *)&AppTaskRepeatThreeStk[0], (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) 3, (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
     
    /* Delete this task */
    OSTaskDel((OS_TCB *)0, &err);
}


static  void  AppTaskRepeatOne (void  *p_arg)
{
    OS_ERR      err;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    
   (void)&p_arg;
   CPU_INT32U i=0;
   
   
      while (DEF_ON)
      {
        i++;
        if(AppTaskOneTCB.TaskState == OS_TASK_STATE_DEL)
        {
          //BSP_DisplayStringDraw("C1",0u, 0u);
          OSTaskCreate((OS_TCB     *)&AppTaskOneTCB, (CPU_CHAR   *)"App Task One", (OS_TASK_PTR ) AppTaskOne, (void       *) 0, (OS_PRIO     ) APP_TASK_ONE_PRIO, (CPU_STK    *)&AppTaskOneStk[0], (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_ONE_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) (TASK1PERIOD*i), (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
          OSQPend(&DummyQ, (TASK1PERIOD * 1000), OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);
        }
        else
        {
          OSQPend(&DummyQ, 300, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);
          //BSP_DisplayStringDraw("E1",0u, 0u);
          //printf("\nE1");
        }
      }
}

static  void  AppTaskRepeatTwo (void  *p_arg)
{
    OS_ERR      err;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
   (void)&p_arg;
   CPU_INT32U i=0;

      while (DEF_ON)
      {
        i++;
        if(AppTaskTwoTCB.TaskState == OS_TASK_STATE_DEL)
        {
          //BSP_DisplayStringDraw("C2",0u, 0u);
          OSTaskCreate((OS_TCB     *)&AppTaskTwoTCB, (CPU_CHAR   *)"App Task Two", (OS_TASK_PTR ) AppTaskTwo, (void       *) 0, (OS_PRIO     ) APP_TASK_TWO_PRIO, (CPU_STK    *)&AppTaskTwoStk[0], (CPU_STK_SIZE) APP_TASK_TWO_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_TWO_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) (TASK2PERIOD*i), (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
          OSQPend(&DummyQ, (TASK2PERIOD * 1000), OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);
        }
        else
        {
          OSQPend(&DummyQ, 300, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);
          //BSP_DisplayStringDraw("E2",0u, 0u);
          //printf("\nE2");
        }
      }
}

static  void  AppTaskRepeatThree (void  *p_arg)
{
    OS_ERR      err;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
   (void)&p_arg;
   CPU_INT32U i=0;

      while (DEF_ON)
      {
        i++;
        if(AppTaskThreeTCB.TaskState == OS_TASK_STATE_DEL)
        {
          //BSP_DisplayStringDraw("C3",0u, 0u);
          OSTaskCreate((OS_TCB     *)&AppTaskThreeTCB, (CPU_CHAR   *)"App Task Three", (OS_TASK_PTR ) AppTaskThree, (void       *) 0, (OS_PRIO     ) APP_TASK_THREE_PRIO, (CPU_STK    *)&AppTaskThreeStk[0], (CPU_STK_SIZE) APP_TASK_THREE_STK_SIZE / 10u, (CPU_STK_SIZE) APP_TASK_THREE_STK_SIZE, (OS_MSG_QTY  ) 0u, (OS_TICK     ) 0u, (void       *) (CPU_INT32U) (TASK3PERIOD*i), (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), (OS_ERR     *)&err);
          OSQPend(&DummyQ, (TASK3PERIOD * 1000), OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);
        }
        else
        {
          OSQPend(&DummyQ, 300, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);
          //BSP_DisplayStringDraw("E3",0u, 0u);
          //printf("\nE3");
        }
      }
}

static  void  AppTaskOne (void  *p_arg)
{
    OS_ERR      err;
    CPU_INT32U  iSec, k, i, j;
    CPU_TS ts;
   
    iSec = WORKLOAD1;
    
    OSMutexPend((OS_MUTEX *)&MutexThree, (OS_TICK )0, (OS_OPT )OS_OPT_PEND_BLOCKING, (CPU_TS *)&ts, (OS_ERR *)&err);
    
    for(k=0; k<iSec; k++)
    {
      for(i=0; i <ONESECONDTICK; i++)
         j = ((i * 2) + j);
    }
    
    OSMutexPend((OS_MUTEX *)&MutexTwo, (OS_TICK )0, (OS_OPT )OS_OPT_PEND_BLOCKING, (CPU_TS *)&ts, (OS_ERR *)&err);
    if(iMove > 0)
    {
      RoboTurn(LEFT_SIDE, 14, 50);
      iMove--;
    }
    
    OSMutexPost((OS_MUTEX *)&MutexTwo, (OS_OPT )OS_OPT_POST_NONE, (OS_ERR *)&err);
    BSP_DisplayClear();
    BSP_DisplayStringDraw("TASK ONE",0u, 0u);
    OSMutexPost((OS_MUTEX *)&MutexThree, (OS_OPT )OS_OPT_POST_NONE, (OS_ERR *)&err);
        
    //printf("\nT1");
    OSTaskDel((OS_TCB *)0, &err);
}

static  void  AppTaskTwo (void  *p_arg)
{
    OS_ERR      err;
    CPU_INT32U  i, j=7;
    CPU_TS ts;
    
    OSMutexPend((OS_MUTEX *)&MutexThree, (OS_TICK )0, (OS_OPT )OS_OPT_PEND_BLOCKING, (CPU_TS *)&ts, (OS_ERR *)&err);
    OSMutexPend((OS_MUTEX *)&MutexTwo, (OS_TICK )0, (OS_OPT )OS_OPT_PEND_BLOCKING, (CPU_TS *)&ts, (OS_ERR *)&err);
    
    for(i=0; i <(WORKLOAD2*ONESECONDTICK); i++)
    {
      j = ((i * 2) + j);
    }
    
    BSP_DisplayClear();
    BSP_DisplayStringDraw("TASK TWO",0u, 0u);
    OSMutexPost((OS_MUTEX *)&MutexTwo, (OS_OPT )OS_OPT_POST_NONE, (OS_ERR *)&err);
    OSMutexPost((OS_MUTEX *)&MutexThree, (OS_OPT )OS_OPT_POST_NONE, (OS_ERR *)&err);
    
    #if(MICROSD_EN == 1) 
    sprintf(g_cCmdBuf,"app log.txt  %d %d %d", j, iCounter++, (OSTimeGet(&err)/1000));
    CmdLineProcess(g_cCmdBuf);
    #endif
    
    //printf("\nT2");
    OSTaskDel((OS_TCB *)0, &err);
}

static  void  AppTaskThree (void  *p_arg)
{
    OS_ERR      err;
    CPU_INT32U  iSec, k, i, j;
    CPU_TS ts;

    iSec = WORKLOAD3;

    OSMutexPend((OS_MUTEX *)&MutexTwo, (OS_TICK )0, (OS_OPT )OS_OPT_PEND_BLOCKING, (CPU_TS *)&ts, (OS_ERR *)&err);
    
    BSP_LED_Off(0u);

    for(k=0; k<iSec; k++)
    {
      BSP_LED_Toggle(0u);
      for(i=0; i <ONESECONDTICK; i++)
         j = ((i * 2) + j);
    }
    
    OSMutexPend((OS_MUTEX *)&MutexThree, (OS_TICK )0, (OS_OPT )OS_OPT_PEND_BLOCKING, (CPU_TS *)&ts, (OS_ERR *)&err);
    BSP_DisplayClear();
    BSP_DisplayStringDraw("TASK THREE",0u, 0u);
    OSMutexPost((OS_MUTEX *)&MutexThree, (OS_OPT )OS_OPT_POST_NONE, (OS_ERR *)&err);
    
    BSP_LED_Off(0u);
    OSMutexPost((OS_MUTEX *)&MutexTwo, (OS_OPT )OS_OPT_POST_NONE, (OS_ERR *)&err);
    
    //printf("\nT3");
    OSTaskDel((OS_TCB *)0, &err);
}


void IntWheelSensor()
{
	CPU_INT32U         ulStatusR_A;
	CPU_INT32U         ulStatusL_A;

	static CPU_INT08U CountL = 0;
	static CPU_INT08U CountR = 0;

	static CPU_INT08U data = 0;

	ulStatusR_A = GPIOPinIntStatus(RIGHT_IR_SENSOR_A_PORT, DEF_TRUE);
	ulStatusL_A = GPIOPinIntStatus(LEFT_IR_SENSOR_A_PORT, DEF_TRUE);

        if (ulStatusR_A & RIGHT_IR_SENSOR_A_PIN)
        {
          GPIOPinIntClear(RIGHT_IR_SENSOR_A_PORT, RIGHT_IR_SENSOR_A_PIN);           /* Clear interrupt.*/
          CountR = CountR + 1;
        }

        if (ulStatusL_A & LEFT_IR_SENSOR_A_PIN)
        {
          GPIOPinIntClear(LEFT_IR_SENSOR_A_PORT, LEFT_IR_SENSOR_A_PIN);
          CountL = CountL + 1;
        }

	if((CountL >= Left_tgt) && (CountR >= Right_tgt))
        {
          data = 0x11;
          Left_tgt = 0;
          Right_tgt = 0;
          CountL = 0;
          CountR = 0;
          BSP_MotorStop(LEFT_SIDE);
          BSP_MotorStop(RIGHT_SIDE);
        }
        else if(CountL >= Left_tgt)
        {
          data = 0x10;
          Left_tgt = 0;
          CountL = 0;
          BSP_MotorStop(LEFT_SIDE);
        }
        else if(CountR >= Right_tgt)
        {
          data = 0x01;
          Right_tgt = 0;
          CountR = 0;
          BSP_MotorStop(RIGHT_SIDE);
        }
        return;
}


static  void  AppRobotMotorDriveSensorEnable ()
{
    BSP_WheelSensorEnable();
    BSP_WheelSensorIntEnable(RIGHT_SIDE, SENSOR_A, (CPU_FNCT_VOID)IntWheelSensor);
    BSP_WheelSensorIntEnable(LEFT_SIDE, SENSOR_A, (CPU_FNCT_VOID)IntWheelSensor);
}

static  void  AppRobotMotorDriveSensorDisable ()
{
    BSP_WheelSensorDisable();
    BSP_WheelSensorIntDisable(RIGHT_SIDE, SENSOR_A);
    BSP_WheelSensorIntDisable(LEFT_SIDE, SENSOR_A);
}

void RoboTurn(tSide dir, CPU_INT16U seg, CPU_INT16U speed)
{
	Left_tgt = seg;
        Right_tgt = seg;

	BSP_MotorStop(LEFT_SIDE);
	BSP_MotorStop(RIGHT_SIDE);

        BSP_MotorSpeed(LEFT_SIDE, speed <<8u);
	BSP_MotorSpeed(RIGHT_SIDE,speed <<8u);

	switch(dir)
	{
            case FRONT :
                    BSP_MotorDir(RIGHT_SIDE,FORWARD);
                    BSP_MotorDir(LEFT_SIDE,FORWARD);
                    BSP_MotorRun(LEFT_SIDE);
                    BSP_MotorRun(RIGHT_SIDE);
                    break;
                    
            case BACK :
                    BSP_MotorDir(LEFT_SIDE,REVERSE);
                    BSP_MotorDir(RIGHT_SIDE,REVERSE);
                    BSP_MotorRun(RIGHT_SIDE);
                    BSP_MotorRun(LEFT_SIDE);
                    break;
                    
            case LEFT_SIDE :
                    BSP_MotorDir(RIGHT_SIDE,FORWARD);
                    BSP_MotorDir(LEFT_SIDE,REVERSE);
                    BSP_MotorRun(LEFT_SIDE);
                    BSP_MotorRun(RIGHT_SIDE);
                    break;
                    
            case RIGHT_SIDE:
                    BSP_MotorDir(LEFT_SIDE,FORWARD);
                    BSP_MotorDir(RIGHT_SIDE,REVERSE);
                    BSP_MotorRun(RIGHT_SIDE);
                    BSP_MotorRun(LEFT_SIDE);
                    break;
                    
            default:
                    BSP_MotorStop(LEFT_SIDE);
                    BSP_MotorStop(RIGHT_SIDE);
                    break;
	}

	return;
}

#if(TIMER_EN == 1)
unsigned long TimerTick(void)
{
    return((TimerValueGet(TIMER3_BASE, TIMER_A)/TIMERDIV));
}

void TimerReset(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
    SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER3);
    TimerConfigure(TIMER3_BASE, TIMER_CFG_32_BIT_PER_UP);
    TimerLoadSet(TIMER3_BASE, TIMER_BOTH, 0xffffffff);
    TimerEnable(TIMER3_BASE, TIMER_BOTH);
}
#endif