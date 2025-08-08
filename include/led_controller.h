#ifndef __LED_CONTROLLER_H__
#define __LED_CONTROLLER_H__

#include "tuya_cloud_types.h"
#include "tal_sw_timer.h"
#include "tal_gpio.h"
#include "ws2812_spi.h"

// ========================== 时间参数配置 ==========================
// 自检时间参数
#define INIT_RED_TIME     1000    // 红色显示时间 (ms)
#define INIT_GREEN_TIME   1000    // 绿色显示时间 (ms)
#define INIT_BLUE_TIME    1000    // 蓝色显示时间 (ms)

// 状态超时参数
#define CONFIG_SUCCESS_TIMEOUT  2000  // 配网成功显示时间 (ms)
#define VOLUME_DISPLAY_TIMEOUT  2000  // 音量显示时间 (ms)
#define WAKE_TOTAL_TIME       12000   // 唤醒状态总时间 (ms)

// 闪烁时间参数
#define CONFIGURING_BLINK_TIME  250   // 配网中闪烁间隔 (ms)
#define CONFIG_SUCCESS_LED_INTERVAL 200 // 配网成功LED点亮间隔 (ms)
#define WAKE_FLASH_ON_TIME    200     // 唤醒闪烁亮灯时间 (ms)
#define WAKE_FLASH_OFF_TIME   200     // 唤醒闪烁灭灯时间 (ms)
#define STANDBY_MOVE_INTERVAL 250     // 待机跑马灯移动间隔 (ms)

// 呼吸灯参数
#define BREATH_TIMER_INTERVAL   15    // 呼吸灯定时器周期 (ms)
#define BREATH_TABLE_SIZE       256   // 呼吸灯亮度表大小

// ========================== 状态枚举定义 ==========================
typedef enum {
    LED_IDLE,         ///< 空闲状态（所有LED熄灭）
    LED_INIT,         ///< 上电自检状态（红->绿->蓝）
    LED_CONFIGURING,  ///< 配网中（绿灯闪烁250ms亮、250ms灭）
    LED_CONFIG_SUCCESS,///< 配网成功（显示WIFI信号强度，每200ms亮一个灯，灯亮完保持2s后灭。如：等级5，每200ms亮一个灯，亮至第5个时，灯保持两秒熄灭）
    LED_NET_ERROR,    ///< 网络异常（红灯常亮）
    LED_WAKE,         ///< 唤醒（蓝灯闪两下后常亮，12秒内无对话或对话无效关闭。闪两下200ms，后常亮）
    LED_VOLUME,       ///< 调节音量（黄灯等级显示）
    LED_STANDBY       ///< 待机（单个绿灯360度循环跑马，每250ms移动至下一个）
} LedState;

/**
 * @brief 初始化LED控制器
 * 
 * 功能说明：
 * 1. 初始化状态机数据结构
 * 2. 初始化WS2812驱动
 * 3. 创建主定时器
 * 4. 进入上电自检状态
 */
void led_controller_init(void);

/**
 * @brief 设置LED状态
 * 
 * @param new_state 新状态（LedState枚举值）
 * @param value 状态附加参数：
 *   - LED_CONFIG_SUCCESS: WIFI信号强度(0-8)
 *   - LED_VOLUME: 音量等级(0-8)
 *   - 其他状态: 忽略此参数
 * 
 * 状态转换说明：
 * 1. 如果当前处于自检状态，新状态将被缓存，自检完成后自动执行
 * 2. 其他状态下立即执行新状态，并清理前一个状态的资源
 */
void set_led_state(LedState new_state, uint8_t value);

#endif /* __LED_CONTROLLER_H__ */