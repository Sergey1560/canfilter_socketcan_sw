#ifndef LOG_H
#define LOG_H

#define DEBUG_RTT

#define DEBUG_MSG
#define INFO_MSG
#define WARNING_MSG
#define ERROR_MSG


#ifdef DEBUG_RTT
    #include "dwt.h"
    #include "SEGGER_RTT.h"

    #ifdef DEBUG_MSG
        #define DEBUG(fmt, ...) SEGGER_RTT_printf(0,"[D] %-20s:%-4d [%d]:" fmt "\r\n", __func__, __LINE__,dwt_get_tick_in_sec() __VA_OPT__(,) __VA_ARGS__)
    #else
        #define DEBUG(fmt, ...)
    #endif
    #ifdef INFO_MSG
        #define INFO(fmt, ...)  SEGGER_RTT_printf(0,"[I] %-20s:%-4d [%d]:" fmt "\r\n", __func__, __LINE__,dwt_get_tick_in_sec() __VA_OPT__(,) __VA_ARGS__)
    #else
        #define INFO(fmt, ...)
    #endif
    #ifdef WARNING_MSG
        #define WARNING(fmt, ...) SEGGER_RTT_printf(0,"[W] %-20s:%-4d [%d]:" fmt "\r\n",  __func__, __LINE__,dwt_get_tick_in_sec() __VA_OPT__(,) __VA_ARGS__)
    #else
        #define WARNING(fmt, ...)
    #endif
    #ifdef ERROR_MSG
        #define ERROR(fmt, ...) SEGGER_RTT_printf(0,"[E] %-20s:%-4d [%d]:" fmt "\r\n",  __func__, __LINE__,dwt_get_tick_in_sec() __VA_OPT__(,) __VA_ARGS__)
    #else
        #define ERROR(fmt, ...)
    #endif
#else
#define DEBUG(fmt, ...)
#define INFO(fmt, ...)
#define WARNING(fmt, ...)
#define ERROR(fmt, ...)
#endif

#endif
