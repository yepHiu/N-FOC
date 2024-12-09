//
// Created by yephi on 12/6/2024.
//

#include "flash.h"
#include "octospi.h"
#include "Hardware/w25q64.h"
static void flash_init(void){
    OSPI_W25Qxx_Init();
}

static void flash_read(uint32_t address, uint8_t *data, uint32_t length) {
    // 读取数据
    OSPI_W25Qxx_ReadBuffer(data,address,length);
}

static void flash_write(uint32_t address, uint8_t *data, uint32_t length) {
    // 写入数据
    OSPI_W25Qxx_WriteBuffer(data,address,length);
}

static void flash_erase(uint32_t address, uint32_t length) {
    // 计算需要擦除的扇区数量
    uint32_t sector_size = 4096;  // 假设扇区大小为 4096 字节
    uint32_t start_sector = address / sector_size;
    uint32_t end_sector = (address + length - 1) / sector_size;

    // 擦除每个扇区
    for (uint32_t sector = start_sector; sector <= end_sector; sector++) {
        OSPI_W25Qxx_SectorErase(sector * sector_size);
    }
}

FlashDriver flash_driver = {
        .init = flash_init,
        .read = flash_read,
        .write = flash_write,
        .erase = flash_erase
};

