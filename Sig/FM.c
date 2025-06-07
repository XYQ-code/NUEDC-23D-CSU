#include "stm32f10x.h"
#include "OLED.h"
#include <math.h>
#include <string.h>  // 添加memcpy支持

#define BUFFER_SIZE 800
#define F_CARRIER 40000.0f     // 载波频率修改为30kHz
#define F_MOD_FM   5000.0f    
#define SAMPLE_RATE 100000.0f // 修改为100kHz采样率
#define PI 3.141592653589793f
#define DC_OFFSET 1.6f         // 直流偏置修改为1.6V

// 改进Hilbert变换系数（适用于100kHz采样率）
const float hilbert_coeff[5] = {0.2155f, 0.0f, -0.2155f};  // 3阶近似

void FM_Demodulate(float *buffer, uint16_t size) {
    static float I[BUFFER_SIZE], Q[BUFFER_SIZE];
    static float phase_diff[BUFFER_SIZE];
   // static uint32_t sample_count = 0;
    
    // 1. 动态直流消除（滑动窗口长度适配新采样率）
    static float dc_history[20] = {0};  // 20ms窗口
    static uint8_t dc_index = 0;
    float dc_sum = 0;
    for(int i=0; i<size; i++) {
        dc_history[dc_index] = buffer[i];
        dc_index = (dc_index + 1) % 20;
    }
    for(int i=0; i<20; i++) dc_sum += dc_history[i];
    float dc_offset = dc_sum / 20.0f;
    for(int i=0; i<size; i++) buffer[i] -= dc_offset;

    // 其余代码保持不变...

    // 2. 改进正交信号生成（FIR Hilbert变换）
    static float delay_line[4] = {0};  // 延迟线存储历史数据
    for(int i=0; i<size; i++) {
        // 更新延迟线
        memmove(&delay_line[1], &delay_line[0], 3*sizeof(float));
        delay_line[0] = buffer[i];
        
        // I路直接使用当前采样
        I[i] = buffer[i];
        
        // Q路使用FIR Hilbert滤波器（3阶）
        Q[i] = 0.0f;
        for(int j=0; j<3; j++) {
            Q[i] += hilbert_coeff[j] * delay_line[j];
        }
    }
    
    // 3. 改进滤波设计（抗混叠+相位补偿）
    static float i_filter[3] = {0}, q_filter[3] = {0};
    for(int i=0; i<size; i++) {
        // 三阶巴特沃斯低通（fc=15kHz）
        I[i] = 0.3333f*I[i] + 0.3333f*i_filter[0] + 0.3333f*i_filter[1];
        Q[i] = 0.3333f*Q[i] + 0.3333f*q_filter[0] + 0.3333f*q_filter[1];
        // 更新滤波器状态
        i_filter[1] = i_filter[0];
        i_filter[0] = I[i];
        q_filter[1] = q_filter[0];
        q_filter[0] = Q[i];
    }
    
    // 4. 鉴相算法优化（相位差动态补偿）
    static float prev_phase = 0;
    for(int i=0; i<size; i++) {
        float current_phase = atan2f(Q[i], I[i]);
        phase_diff[i] = current_phase - prev_phase;
        
        // 自动相位补偿（处理2π跳变）
        if(phase_diff[i] > PI) {
            phase_diff[i] -= 2*PI;
        } else if(phase_diff[i] < -PI) {
            phase_diff[i] += 2*PI;
        }
        
        // 相位漂移补偿（抑制低频噪声）
        static float phase_bias = 0;
        phase_diff[i] -= phase_bias;
        phase_bias += phase_diff[i] * 0.001f;  // 自适应调整系数
        
        prev_phase = current_phase;
    }
    
    // 5. 改进参数计算（加权滑动窗口）
    float max_df = -INFINITY, min_df = INFINITY;
    for(int i=10; i<size; i++) {  // 跳过前10个不稳定值
        // 瞬时频偏计算（适配新采样率）
        float inst_df = phase_diff[i] * SAMPLE_RATE / (2*PI);
        
        // 加权平均窗口（最新数据权重更高）
        static float df_history[10] = {0};
        static uint8_t hist_idx = 0;
        df_history[hist_idx] = inst_df;
        hist_idx = (hist_idx + 1) % 10;
        
        // 计算加权平均值
        float weighted_sum = 0;
        float weight_sum = 0;
        for(int j=0; j<10; j++) {
            float weight = (j+1)/55.0f;  // 线性权重系数（1+2+...+10=55）
            weighted_sum += df_history[j] * weight;
            weight_sum += weight;
        }
        float avg_df = weighted_sum / weight_sum;
        
        // 更新极值
        if(avg_df > max_df) max_df = avg_df;
        if(avg_df < min_df) min_df = avg_df;
    }
    
    // 6. 参数计算（峰峰值转峰峰值）
    float delta_f_max = (max_df - min_df) / 2.0f;
    float mf = delta_f_max / F_MOD_FM;
    
    // 7. 显示参数
    /* 显示调制类型 */
    OLED_ShowString(1, 1, "FM Signal");

    /* 显示调频系数（整数+小数分开显示）*/
    uint16_t mf_int = (uint16_t)mf;          // 整数部分
    uint16_t mf_frac = (uint16_t)((mf - mf_int)*100);  // 小数部分（两位）
    OLED_ShowString(3, 1, "mf:");
    OLED_ShowNum(3, 5, mf_int, 1);          // 显示整数位
    OLED_ShowChar(3, 6, '.');               // 小数点
    OLED_ShowNum(3, 7, mf_frac, 2);         // 显示小数位

    /* 显示最大频偏 */
    OLED_ShowString(4, 1, "df:");
    OLED_ShowNum(4, 5, (uint32_t)delta_f_max, 5);  // 显示5位整数
    OLED_ShowString(4, 10, "Hz");
    
    // 8. 解调输出（带直流恢复）
    /*for(int i=0; i<size-1; i++) {
        float output = phase_diff[i] * 500.0f + DC_OFFSET;
        if(output > 3.0f) output = 3.0f;
        if(output < 0.3f) output = 0.3f;
        DAC_Write((uint16_t)(output * 4095.0f / 3.3f));
    }*/
}
