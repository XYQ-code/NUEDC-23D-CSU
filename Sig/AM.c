#include "stm32f10x.h"
#include "OLED.h"

void AM_Demodulate(float *buffer, uint16_t size) {
    float max = buffer[0];
    float min = buffer[0];
    
    // 遍历缓冲区找到最大值和最小值
    for (uint16_t i = 1; i < size; i++) {
        if (buffer[i] > max) max = buffer[i];
        if (buffer[i] < min) min = buffer[i];
    }
    
    // 计算调幅系数：峰峰值除以3V（放大后的峰峰值）
    float m = (max - min) / 3.0f;
    
    // 显示调幅系数，保留两位小数
    OLED_ShowString(1, 1, "AM m: 0."); // 假设m的范围是0.30到1.00
    uint16_t m_percent = (uint16_t)(m * 100);
    OLED_ShowNum(1, 7, m_percent / 10, 1); // 十位
    OLED_ShowNum(1, 8, m_percent % 10, 1); // 个位
}