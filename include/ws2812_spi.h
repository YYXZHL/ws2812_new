#ifndef __WS2812_SPI_H__
#define __WS2812_SPI_H__

#include "tuya_cloud_types.h"
#include "tal_log.h"


// 定义灯珠数量为宏常量
#define WS2812_LED_COUNT 12

#define	WS2812_0	0xC0
#define	WS2812_1	0xFC //0xF0

// SPI 配置参数
#define WS2812_SPI_FREQ        4500000//5//6    // 8 MHz
#define WS2812_RESET_DELAY_MS  1          // > 50 μs

/**
 * @brief 初始化 WS2812 SPI 驱动并分配缓冲区
 * 
 * @param port SPI 端口号
 * @return OPERATE_RET 返回操作结果
 */
OPERATE_RET ws2812_spi_init(TUYA_SPI_NUM_E port);

/**
 * @brief 设置单个 LED 灯珠的颜色
 * 
 * @param index 灯珠索引
 * @param red 红色分量（0~255）
 * @param green 绿色分量（0~255）
 * @param blue 蓝色分量（0~255）
 * @return OPERATE_RET 返回操作结果
 */
OPERATE_RET ws2812_spi_set_pixel(UINT16_T index, UCHAR_T red, UCHAR_T green, UCHAR_T blue);

/**
 * @brief 刷新所有 LED 灯珠的颜色数据
 * 
 * @return OPERATE_RET 返回操作结果
 */
OPERATE_RET ws2812_spi_refresh(VOID_T);

/**
 * @brief 释放资源并反初始化 SPI
 * 
 * @return OPERATE_RET 返回操作结果
 */
OPERATE_RET ws2812_spi_deinit(VOID_T);

/**
 * @brief 设置所有 LED 灯珠为相同的颜色
 * 
 * @param red 红色分量（0~255）
 * @param green 绿色分量（0~255）
 * @param blue 蓝色分量（0~255）
 * @return OPERATE_RET 返回操作结果
 */
OPERATE_RET ws2812_spi_set_all(UCHAR_T red, UCHAR_T green, UCHAR_T blue);

VOID_T ws2812_app_init(VOID_T);
VOID_T ws2812_Breathing(VOID_T) ;
#endif // __WS2812_SPI_H__
