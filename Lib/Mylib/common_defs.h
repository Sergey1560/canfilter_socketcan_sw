#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

#include "stdint.h"
#include "stm32g4xx.h"
#include "log.h"
#include "common_data.h"

#define ALGN4 __attribute__ ((aligned (4)))
#define ALGN8 __attribute__ ((aligned (8)))
#define ALGN32 __attribute__ ((aligned (32)))


/* GPIO SPEED */
#define S_LOW 0
#define S_MED 1
#define S_HI  2
#define S_VH  3

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#  define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
#  define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

/* GPIO SPEED */
#define S_LOW 0
#define S_MED 1
#define S_HI  2
#define S_VH  3

#if defined(SEGGER_SYSVIEW)
#include "SEGGER_SYSVIEW.h"
#define TRACE_START                SEGGER_SYSVIEW_Conf()
#define TRACE_ENTER_ISR            SEGGER_SYSVIEW_RecordEnterISR()
#define TRACE_EXIT_ISR             SEGGER_SYSVIEW_RecordExitISR()
#define TRACE_RECORD_ENTER(num)    SEGGER_SYSVIEW_RecordVoid(num)
#define TRACE_RECORD_EXIT(num)     SEGGER_SYSVIEW_RecordEndCall(num)
#define TRACE_PRINTF(fmt, ...) SEGGER_SYSVIEW_PrintfHost(fmt __VA_OPT__(,) __VA_ARGS__)
#define TRACE_ONIDLE                SEGGER_SYSVIEW_OnIdle()
#elif defined(TRACEALYZER)
#define TRACE_START                vTraceEnable(TRC_START)
#define TRACE_ENTER_ISR            
#define TRACE_EXIT_ISR             
#define TRACE_RECORD_ENTER(num)    
#define TRACE_RECORD_EXIT(num)     
#define TRACE_PRINTF(fmt, args...) 
#else
#define TRACE_START                
#define TRACE_ENTER_ISR            
#define TRACE_EXIT_ISR             
#define TRACE_RECORD_ENTER(num)    
#define TRACE_RECORD_EXIT(num)     
#define TRACE_PRINTF(fmt, ...) 
#define TRACE_ONIDLE
#endif


#endif
