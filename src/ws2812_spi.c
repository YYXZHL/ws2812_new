#include "ws2812_spi.h"
#include "tuya_iot_config.h"
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tkl_spi.h"

static UCHAR_T *s_buffer = NULL;
static TUYA_SPI_NUM_E s_spi_port;

/**
 * @brief 初始化驱动并分配缓冲区
 */
OPERATE_RET ws2812_spi_init(TUYA_SPI_NUM_E port) {
    if (s_buffer != NULL) {
        return OPRT_OK;  // 已初始化
    }

    size_t buf_len = (size_t)WS2812_LED_COUNT * 24;  // 每灯24字节编码
    s_buffer = malloc(buf_len);
    if (!s_buffer) {
        return OPRT_MALLOC_FAILED;
    }
    TUYA_SPI_BASE_CFG_T cfg = {
    .spi_dma_flags = TRUE,
    .role = TUYA_SPI_ROLE_MASTER,
    .mode = TUYA_SPI_MODE0,
    .type = TUYA_SPI_SOFT_TYPE,
    .databits = TUYA_SPI_DATA_BIT8,
    .freq_hz = WS2812_SPI_FREQ
    };
    

    OPERATE_RET rt = tkl_spi_init(port, &cfg);
    if (rt != OPRT_OK) {
        free(s_buffer);
        s_buffer = NULL;
        return rt;
    }

    s_spi_port = port;
    return OPRT_OK;
}

/**
 * @brief 设置单个像素的 GRB 数据到缓冲区
 */
OPERATE_RET ws2812_spi_set_pixel(UINT16_T index, UCHAR_T red, UCHAR_T green, UCHAR_T blue) {
    if (index >= WS2812_LED_COUNT || s_buffer == NULL) {
        return OPRT_INVALID_PARM;
    }

    uint32_t color = ((uint32_t)green << 16) | ((uint32_t)red << 8) | blue;
    size_t base = (size_t)index * 24;
    for (int bit = 0; bit < 24; bit++) {
        s_buffer[base + bit] = ((color << bit) & 0x800000) ? WS2812_1 : WS2812_0;
    }
    return OPRT_OK;
}

/**
 * @brief 刷新发送像素数据
 */
OPERATE_RET ws2812_spi_refresh(VOID_T) {
    if (s_buffer == NULL) {
        return OPRT_RESOURCE_NOT_READY;
    }

    size_t len = (size_t)WS2812_LED_COUNT * 24;
    OPERATE_RET rt = tkl_spi_send(s_spi_port, s_buffer, len);
    if (rt != OPRT_OK) {
        return rt;
    }

    //tal_system_sleep(1);  // 确保数据发送完成
    return OPRT_OK;
}

/**
 * @brief 释放资源并反初始化 SPI
 */
OPERATE_RET ws2812_spi_deinit(VOID_T) {
    if (s_buffer) {
        free(s_buffer);
        s_buffer = NULL;
    }
    return tkl_spi_deinit(s_spi_port);
}

/**
 * @brief 设置所有 LED 为相同的颜色
 */
OPERATE_RET ws2812_spi_set_all(UCHAR_T red, UCHAR_T green, UCHAR_T blue) {
    if (s_buffer == NULL) {
        return OPRT_RESOURCE_NOT_READY;
    }
    for (UINT16_T i = 0; i < WS2812_LED_COUNT; i++) {
        ws2812_spi_set_pixel(i, red, green, blue);
    }
    return OPRT_OK;
}

// -------------------- 呼吸灯测试 --------------------

#define W2812_TEST 0

VOID_T ws2812_app_init(VOID_T) 
{
    ws2812_spi_init(TUYA_SPI_NUM_0);
    ws2812_spi_set_all(0, 0, 0);
    ws2812_spi_refresh();

#if (W2812_TEST == 0)
    // 常规功能测试
    //ws2812_spi_set_all(0x00, 0x00, 0xFF);  // 设置为绿色
    //ws2812_spi_refresh();

#else
    // 呼吸灯亮度表（非线性）
    static const uint8_t gammaBreath[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
        1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21,
        23, 24, 26, 27, 29, 31, 33, 35, 37, 39, 42, 44, 47, 49, 52, 55,
        58, 61, 64, 67, 71, 74, 78, 82, 86, 90, 94, 98, 103, 107, 112, 117,
        122, 127, 132, 138, 143, 149, 155, 161, 167, 174, 180, 187, 194, 201, 208, 215,
        223, 230, 238, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 246, 238, 230, 223,
        215, 208, 201, 194, 187, 180, 174, 167, 161, 155, 149, 143, 138, 132, 127, 122,
        117, 112, 107, 103, 98, 94, 90, 86, 82, 78, 74, 71, 67, 64, 61, 58,
        55, 52, 49, 47, 44, 42, 39, 37, 35, 33, 31, 29, 27, 26, 24, 23,
        21, 20, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 8, 7, 7,
        6, 6, 5, 5, 4, 4, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1,
        1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    uint8_t color = 0;
    while (1) {
        ws2812_spi_set_all(0x00, 0x00, gammaBreath[color++]);
        ws2812_spi_refresh();
        TAL_PR_DEBUG("WS2812 Breath Color = %d", color);
        if (color >= 256) {
            color = 0;
        }
        delay_ms(20);
    }
#endif
}
