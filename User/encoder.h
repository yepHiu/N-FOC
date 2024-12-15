//
// Created by yephi on 12/9/2024.
//

#ifndef PROJECT_ENCODER_H
#define PROJECT_ENCODER_H

#include <stdint.h>
#include "as5047p.h"

typedef struct {
    int8_t (*init)(void *handle);              // 初始化编码器
    int8_t (*read_angle)(void *handle, float *angle); // 读取角度（单位：度）
    int8_t (*read_position)(void *handle, uint16_t *position); // 读取位置
    int8_t (*set_zero)(void *handle);          // 设置零点
    int8_t (*get_error_status)(void *handle);  // 获取错误状态
} encoder_ops_t;

// 编码器设备结构
typedef struct {
    void *driver;           // 指向底层驱动的句柄
    const encoder_ops_t *ops; // 编码器操作接口
} encoder_t;

extern as5047p_handle_t g_encoder_handle; // 全局变量定义


#endif //PROJECT_ENCODER_H
