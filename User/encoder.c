//
// Created by yephi on 12/9/2024.
//

#include "encoder.h"
#include "as5047p.h"

as5047p_handle_t as5047p_handle;

#include "encoder.h"
#include "as5047p.h"

// AS5047P 初始化
static int8_t init(void *handle) {
    as5047p_handle_t *as5047p = (as5047p_handle_t *)handle;
    as5047p_reset(as5047p);
    as5047p_config(as5047p, 0b00100101, 0b00000000);
    as5047p_set_zero(as5047p,0);
    return 0;
}

// 读取角度
static int8_t read_angle(void *handle, float *angle) {
    as5047p_handle_t *as5047p = (as5047p_handle_t *)handle;
    return as5047p_get_angle(as5047p, 1, angle); // 1 表示启用 DAEC
}

// 读取位置
static int8_t read_position(void *handle, uint16_t *position) {
    as5047p_handle_t *as5047p = (as5047p_handle_t *)handle;
    return as5047p_get_position(as5047p, 1, position); // 1 表示启用 DAEC
}

// 设置零点
static int8_t set_zero(void *handle) {
    as5047p_handle_t *as5047p = (as5047p_handle_t *)handle;
    as5047p_set_zero(as5047p, 0); // 将当前位置设置为零点
    return 0;
}

// 获取错误状态
static int8_t get_error_status(void *handle) {
    as5047p_handle_t *as5047p = (as5047p_handle_t *)handle;
    return as5047p_get_error_status(as5047p);
}

// 定义 AS5047P 操作接口
static const encoder_ops_t as5047p_ops = {
        .init = init,
        .read_angle = read_angle,
        .read_position = read_position,
        .set_zero = set_zero,
        .get_error_status = get_error_status,
};

// 创建一个 AS5047P 编码器
void encoder_create_as5047p(encoder_t *encoder, as5047p_handle_t *handle) {
    encoder->driver = handle;
    encoder->ops = &as5047p_ops;
}
