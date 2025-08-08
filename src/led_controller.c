#include "led_controller.h"
#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_gpio.h"
#include "ws2812_spi.h"
#include <string.h>

// 颜色分量结构（RGB格式）
typedef struct {
    uint8_t r;  // 红色分量
    uint8_t g;  // 绿色分量
    uint8_t b;  // 蓝色分量
} RGBColor;

// 预定义颜色（RGB格式）
static const RGBColor COLOR_BLACK   = {0, 0, 0};     // 黑色（LED关闭）
static const RGBColor COLOR_RED     = {255, 0, 0};   // 红色
static const RGBColor COLOR_GREEN   = {0, 255, 0};   // 绿色
static const RGBColor COLOR_BLUE    = {0, 0, 255};   // 蓝色
static const RGBColor COLOR_YELLOW  = {255, 255, 0}; // 黄色

// 呼吸灯亮度表（非线性变化，符合人眼感知）
static const uint8_t BREATH_BRIGHTNESS_TABLE[BREATH_TABLE_SIZE] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,
    1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,   6,
    7,   7,   8,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  20,  21,
    23,  24,  26,  27,  29,  31,  33,  35,  37,  39,  42,  44,  47,  49,  52,  55,
    58,  61,  64,  67,  71,  74,  78,  82,  86,  90,  94,  98,  103, 107, 112, 117,
    122, 127, 132, 138, 143, 149, 155, 161, 167, 174, 180, 187, 194, 201, 208, 215,
    223, 230, 238, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 246, 238, 230, 223,
    215, 208, 201, 194, 187, 180, 174, 167, 161, 155, 149, 143, 138, 132, 127, 122,
    117, 112, 107, 103, 98,  94,  90,  86,  82,  78,  74,  71,  67,  64,  61,  58,
    55,  52,  49,  47,  44,  42,  39,  37,  35,  33,  31,  29,  27,  26,  24,  23,
    21,  20,  18,  17,  16,  15,  14,  13,  12,  11,  10,  9,   8,   8,   7,   7,
    6,   6,   5,   5,   4,   4,   3,   3,   3,   2,   2,   2,   2,   2,   1,   1,
    1,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// LED控制状态机结构
typedef struct {
    LedState current_state;      // 当前状态
    LedState pending_state;      // 等待状态（在自检过程中接收的新状态）
    uint8_t pending_value;       // 等待状态参数
    BOOL_T has_pending_state;    // 是否有等待状态
    
    // 状态专用数据
    union {
        struct {
            uint8_t step;        // 自检步骤：0-红,1-绿,2-蓝
        } init;
        struct {
            uint8_t index;       // 呼吸灯查表索引 (0-255)
        } breath;
        struct {
            BOOL_T is_light_on;  // 当前LED亮灭状态
            uint16_t blink_count; // 已闪烁次数
        } blink;
    } state_data;
    
    // 定时器
    TIMER_ID main_timer;   // 主定时器：处理所有状态转换和动作
} LedController;

static LedController led_ctrl;

// LED点亮顺序表（从LED9开始的特定顺序）
// 索引0对应0挡（不亮），索引1对应1挡（亮LED9），以此类推
static const uint8_t LED_LIGHT_ORDER[13] = {
    0,    // 0挡：不亮任何LED
    9,    // 1挡：led9
    8,    // 2挡：led8
    7,    // 3挡：led7
    6,    // 4挡：led6
    5,    // 5挡：led5
    4,    // 6挡：led4
    3,    // 7挡：led3
    2,    // 8挡：led2
    1,    // 9挡：led1
    12,   // 10挡：led12
    11,   // 11挡：led11
    10    // 12挡：led10
};

// 设置所有LED为同一颜色
static void set_all_leds(const RGBColor *color) {
    ws2812_spi_set_all(color->r, color->g, color->b);
    ws2812_spi_refresh();
}

// 设置等级显示（用于信号强度和音量）- 修改为新的点亮顺序
static void set_level_leds(const RGBColor *color, uint8_t level) {
    // 确保等级在有效范围内
    if (level > 12) {
        level = 12;
    }
    
    // 首先关闭所有LED
    for (int i = 0; i < WS2812_LED_COUNT; i++) {
        ws2812_spi_set_pixel(i, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b);
    }
    
    // 按照新的顺序点亮LED
    for (int i = 1; i <= level; i++) {
        uint8_t led_num = LED_LIGHT_ORDER[i];  // 获取LED编号(1-12)
        if (led_num > 0 && led_num <= WS2812_LED_COUNT) {  // 边界检查
            uint8_t led_index = led_num - 1;  // 转换为0-based索引
            ws2812_spi_set_pixel(led_index, color->r, color->g, color->b);
        }
    }
    
    ws2812_spi_refresh();
}

// 主定时器回调：处理所有状态事件
static void main_timer_cb(TIMER_ID timer_id, VOID_T *arg) {
    switch (led_ctrl.current_state) {
        case LED_INIT:
            // 自检状态转换：红->绿->蓝
            led_ctrl.state_data.init.step++;
            
            if (led_ctrl.state_data.init.step == 1) {
                // 切换到绿色
                set_all_leds(&COLOR_GREEN);
                tal_sw_timer_start(led_ctrl.main_timer, INIT_GREEN_TIME, TAL_TIMER_ONCE);
            } else if (led_ctrl.state_data.init.step == 2) {
                // 切换到蓝色
                set_all_leds(&COLOR_BLUE);
                tal_sw_timer_start(led_ctrl.main_timer, INIT_BLUE_TIME, TAL_TIMER_ONCE);
            } else {
                // 自检完成
                TAL_PR_DEBUG("Init complete");
                
                // 进入空闲状态
                led_ctrl.current_state = LED_IDLE;
                
                // 执行等待状态或进入空闲
                if (led_ctrl.has_pending_state) {
                    // 保存等待状态值
                    LedState next_state = led_ctrl.pending_state;
                    uint8_t next_value = led_ctrl.pending_value;
                    
                    // 重置等待状态
                    led_ctrl.has_pending_state = FALSE;
                    led_ctrl.pending_state = LED_IDLE;  // 清除pending_state
                    led_ctrl.pending_value = 0;         // 清除pending_value
                    
                    // 直接执行新状态
                    set_led_state(next_state, next_value);
                } else {
                    set_all_leds(&COLOR_BLACK);
                }
            }
            break;
            
        case LED_CONFIG_SUCCESS:
        case LED_VOLUME:
            // 显示状态超时，进入空闲
            led_ctrl.current_state = LED_IDLE;
            set_all_leds(&COLOR_BLACK);
            TAL_PR_DEBUG("Display timeout, entering idle state");
            break;
            
        case LED_DIALOG:
            // 对话状态：切换亮灭状态
            if (led_ctrl.state_data.blink.is_light_on) {
                // 当前亮 -> 切换为灭
                set_all_leds(&COLOR_BLACK);
                led_ctrl.state_data.blink.is_light_on = FALSE;
                tal_sw_timer_start(led_ctrl.main_timer, DIALOG_LIGHT_OFF_TIME, TAL_TIMER_ONCE);
            } else {
                // 当前灭 -> 切换为亮
                set_all_leds(&COLOR_BLUE);
                led_ctrl.state_data.blink.is_light_on = TRUE;
                led_ctrl.state_data.blink.blink_count++;
                
                // 检查是否达到总闪烁次数
                if (led_ctrl.state_data.blink.blink_count >= DIALOG_BLINK_COUNT) {
                    led_ctrl.current_state = LED_IDLE;
                    set_all_leds(&COLOR_BLACK);
                    TAL_PR_DEBUG("Dialog blinking complete, entering idle state");
                } else {
                    tal_sw_timer_start(led_ctrl.main_timer, DIALOG_LIGHT_ON_TIME, TAL_TIMER_ONCE);
                }
            }
            break;
            
        case LED_CONFIGURING: // 配网中（绿灯呼吸效果）
        case LED_BREATHING:   // 呼吸灯效果（蓝灯呼吸）
        {
            // 更新呼吸灯索引
            led_ctrl.state_data.breath.index++;
            
            // 处理索引循环
            if (led_ctrl.state_data.breath.index >= BREATH_TABLE_SIZE) {
                led_ctrl.state_data.breath.index = 0;
            }
            
            // 获取当前亮度值
            uint8_t brightness = BREATH_BRIGHTNESS_TABLE[led_ctrl.state_data.breath.index];
            
            // 设置LED颜色
            if (led_ctrl.current_state == LED_CONFIGURING) {
                // 配网中：绿灯呼吸
                ws2812_spi_set_all(0, brightness, 0);
            } else {
                // 呼吸灯：蓝灯呼吸
                ws2812_spi_set_all(0, 0, brightness);
            }
            ws2812_spi_refresh();
            
            // 设置下一次呼吸定时
            tal_sw_timer_start(led_ctrl.main_timer, BREATH_TIMER_INTERVAL, TAL_TIMER_ONCE);
            break;
        }
            
        default:
            // 其他状态无需处理
            break;
    }
}

// 清理当前状态资源
static void cleanup_current_state(void) {
    // 停止主定时器
    tal_sw_timer_stop(led_ctrl.main_timer);
    
    // 重置状态数据
    memset(&led_ctrl.state_data, 0, sizeof(led_ctrl.state_data));
}

// 初始化LED控制器
void led_controller_init(void) {
    TAL_PR_DEBUG("Initializing LED controller");
    
    // 清零控制结构体
    memset(&led_ctrl, 0, sizeof(LedController));
    
    // 初始化WS2812驱动
    ws2812_app_init();
    TAL_PR_DEBUG("WS2812 driver initialized");
    
    // 创建主定时器
    tal_sw_timer_create(main_timer_cb, NULL, &led_ctrl.main_timer);
    
    TAL_PR_DEBUG("LED controller initialized");
    
    // // 初始状态：上电自检
    set_led_state(LED_INIT, 0);
}

// 设置LED状态
void set_led_state(LedState new_state, uint8_t value) {
    TAL_PR_DEBUG("Setting LED state: %d, value: %d", new_state, value);
    
    // 上电自检独占处理：自检过程中接收的新状态将被缓存
    if (led_ctrl.current_state == LED_INIT && new_state != LED_INIT) {
        led_ctrl.pending_state = new_state;
        led_ctrl.pending_value = value;
        led_ctrl.has_pending_state = TRUE;
        TAL_PR_DEBUG("Init in progress, pending state: %d", new_state);
        return;
    }
    
    // 如果设置相同状态且参数相同，避免重复操作
    if (led_ctrl.current_state == new_state) {
        // 对于有参数的状态，检查参数是否相同
        if (new_state == LED_CONFIG_SUCCESS || new_state == LED_VOLUME) {
            // 对于等级显示状态，如果等级相同则重新启动定时器
            if (new_state == LED_CONFIG_SUCCESS) {
                set_level_leds(&COLOR_GREEN, value);
                tal_sw_timer_stop(led_ctrl.main_timer);
                tal_sw_timer_start(led_ctrl.main_timer, CONFIG_SUCCESS_TIMEOUT, TAL_TIMER_ONCE);
            } else if (new_state == LED_VOLUME) {
                set_level_leds(&COLOR_YELLOW, value);
                tal_sw_timer_stop(led_ctrl.main_timer);
                tal_sw_timer_start(led_ctrl.main_timer, VOLUME_DISPLAY_TIMEOUT, TAL_TIMER_ONCE);
            }
            return;
        } else if (new_state == LED_IDLE) {
            // IDLE状态需要确保所有LED都关闭，不能跳过
            set_all_leds(&COLOR_BLACK);
            return;
        } else if (new_state == LED_NET_ERROR) {
            // 网络错误状态重复设置时不需要额外操作
            return;
        }
        // 其他状态如呼吸灯等需要重新初始化
    }
    
    // 清理前一个状态
    cleanup_current_state();
    
    // 执行新状态
    switch (new_state) {
        case LED_INIT: // 上电自检（红->绿->蓝）
            set_all_leds(&COLOR_RED);
            led_ctrl.state_data.init.step = 0;
            tal_sw_timer_start(led_ctrl.main_timer, INIT_RED_TIME, TAL_TIMER_ONCE);
            break;
            
        case LED_IDLE: // 空闲状态（所有LED熄灭）
            set_all_leds(&COLOR_BLACK);
            break;
            
        case LED_CONFIGURING: // 配网中（绿灯呼吸效果）
            led_ctrl.state_data.breath.index = 0;
            tal_sw_timer_start(led_ctrl.main_timer, BREATH_TIMER_INTERVAL, TAL_TIMER_ONCE);
            break;
            
        case LED_CONFIG_SUCCESS: // 配网成功（显示WIFI信号强度）
            set_level_leds(&COLOR_GREEN, value);
            tal_sw_timer_start(led_ctrl.main_timer, CONFIG_SUCCESS_TIMEOUT, TAL_TIMER_ONCE);
            break;
            
        case LED_NET_ERROR: // 网络异常（红灯常亮）
            set_all_leds(&COLOR_RED);
            break;
            
        case LED_DIALOG: // 对话中（蓝灯闪烁）
            set_all_leds(&COLOR_BLUE);
            led_ctrl.state_data.blink.is_light_on = TRUE;
            led_ctrl.state_data.blink.blink_count = 0;
            tal_sw_timer_start(led_ctrl.main_timer, DIALOG_LIGHT_ON_TIME, TAL_TIMER_ONCE);
            break;
            
        case LED_VOLUME: // 音量调节（黄灯等级显示）
            set_level_leds(&COLOR_YELLOW, value);
            tal_sw_timer_start(led_ctrl.main_timer, VOLUME_DISPLAY_TIMEOUT, TAL_TIMER_ONCE);
            break;
            
        case LED_BREATHING: // 呼吸灯效果（蓝灯呼吸）
            led_ctrl.state_data.breath.index = 0;
            tal_sw_timer_start(led_ctrl.main_timer, BREATH_TIMER_INTERVAL, TAL_TIMER_ONCE);
            break;
    }
    
    // 更新当前状态
    led_ctrl.current_state = new_state;
}