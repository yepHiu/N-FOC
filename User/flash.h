//
// Created by yephi on 12/6/2024.
//

#ifndef PROJECT_FLASH_H
#define PROJECT_FLASH_H
#include "stm32h7xx.h"

typedef struct {
    void (*init)(void);
    void (*read)(uint32_t address, uint8_t *data, uint32_t length);
    void (*write)(uint32_t address, uint8_t *data, uint32_t length);
    void (*erase)(uint32_t address, uint32_t length);
} FlashDriver;

extern FlashDriver w25q64_driver;

#endif //PROJECT_FLASH_H
