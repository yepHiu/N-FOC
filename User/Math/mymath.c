//
// Created by yephi on 12/6/2024.
//

#include "mymath.h"
#include "stm32h7xx.h"

/*!
    \brief     convert int32 to floating point numbers type data
    \param[in] data: int32 type data to be converted
    \retval    converted floating point type data
*/
float uint32_to_float32(uint32_t data){

    float fp32=(*((float*)(&data)));
    return fp32;
}

/*!
    \brief     convert floating point numbers to int32 type data
    \param[in] data0: floating point type data to be converted
    \retval    converted int32 type data
*/
unsigned int float32_to_uint32(float data) {
    uint32_t uint32 = (*((unsigned int *) (&data)));
    return uint32;
}

