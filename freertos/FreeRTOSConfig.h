#pragma once
#include <stdint.h>

#define configUSE_PREEMPTION            1
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configCPU_CLOCK_HZ              ( 125000000UL )
#define configTICK_RATE_HZ              ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES            ( 5 )
#define configMINIMAL_STACK_SIZE        ( 256 )
#define configTOTAL_HEAP_SIZE           ( ( size_t ) ( 40 * 1024 ) )
#define configMAX_TASK_NAME_LEN         ( 16 )
#define configUSE_MUTEXES               1
#define configQUEUE_REGISTRY_SIZE       8
#define configCHECK_FOR_STACK_OVERFLOW  2
#define configUSE_MALLOC_FAILED_HOOK    1
#define configUSE_TIMERS                1
#define configTIMER_TASK_STACK_DEPTH    ( 512 )
#define configTIMER_TASK_PRIORITY       ( 3 )

/* RP2040 port specifics */
#define configSUPPORT_STATIC_ALLOCATION 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1

/* Logging */
#define configASSERT(x) if(!(x)) { taskDISABLE_INTERRUPTS(); for(;;); }
