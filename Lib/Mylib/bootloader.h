#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "common_defs.h"

/*
Переход в системный загрузчик STM32 (USB DFU, 0483:df11).
Запрос ставит метку в .noinit и перезагружает МК, переход выполняется
сразу после сброса, пока периферия в исходном состоянии.
*/

/* Вызывать первой строкой main() */
void bootloader_check(void);

/* Запросить переход (можно из колбэка USB) */
void bootloader_request(void);

/* Основной цикл: отключение от USB и перезагрузка после запроса */
void bootloader_poll(void);

#endif
