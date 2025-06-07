#ifndef __STATIC_FORCEINLINE
  #if defined(__CC_ARM) || defined(__ARMCC_VERSION)  // 针对ARM编译器
    #define __STATIC_FORCEINLINE static __forceinline
  #elif defined(__GNUC__)  // 针对GCC编译器
    #define __STATIC_FORCEINLINE static inline __attribute__((always_inline))
  #else
    #define __STATIC_FORCEINLINE static inline
  #endif
#endif

#include "stm32f10x.h"
#include "Analyze_Signal.h"
#include "math.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "OLED.h"

// 定义信号类型常量
#define SIGNAL_UNKNOWN 0
#define SIGNAL_AM      1
#define SIGNAL_FM      2
#define SIGNAL_CW      3

// FFT大小，必须是2的幂
#define FFT_SIZE 128

// FFT缓冲区
float32_t fftInput[FFT_SIZE*2];  // 复数输入（实部+虚部）
float32_t fftOutput[FFT_SIZE];   // 幅值结果

// 使用FFT分析识别信号类型
uint8_t AnalyzeSignalType(float* buffer, uint16_t bufferSize)
{
    uint16_t i;
    float32_t maxValue;
    uint32_t maxIndex;
    
    // ===== 0. 信号预处理 =====
    
    // 计算直流分量
    float dc_bias = 0.0f;
    for (i = 0; i < bufferSize; i++) {
        dc_bias += buffer[i];
    }
    dc_bias /= bufferSize;
    
    // 计算信号峰峰值
    float max_val = 0.0f, min_val = 3.3f;
    for (i = 0; i < bufferSize; i++) {
        if (buffer[i] > max_val) max_val = buffer[i];
        if (buffer[i] < min_val) min_val = buffer[i];
    }
    float peak_to_peak = max_val - min_val;
    
    // ===== 1. FFT分析 =====
    
    // 应用汉宁窗并去除直流偏置
    for (i = 0; i < FFT_SIZE; i++) {
        if (i < bufferSize) {
            float window = 0.5f * (1.0f - cosf(2.0f * 3.14159f * i / (bufferSize-1)));
            fftInput[i*2] = (buffer[i] - dc_bias) * window;
            fftInput[i*2+1] = 0.0f;
        } else {
            fftInput[i*2] = 0.0f;
            fftInput[i*2+1] = 0.0f;
        }
    }
    
    // 执行FFT变换
    arm_cfft_f32(&arm_cfft_sR_f32_len128, fftInput, 0, 1);
    
    // 计算幅值谱
    arm_cmplx_mag_f32(fftInput, fftOutput, FFT_SIZE);
    
    // 寻找最大幅值及其索引（跳过直流分量）
    arm_max_f32(&fftOutput[1], FFT_SIZE-1, &maxValue, &maxIndex);
    maxIndex += 1;  // 因为跳过了索引0
    
    // ===== 2. 频谱特征计算 =====
    
    uint16_t carrierBin = maxIndex;  // 载波频率对应的FFT索引
    float sidebandRatio = 0.0f;      // 边带与载波比率
    float sidebandSpacing = 0.0f;  // 定义并初始化边带间隔变量
    
    
    // 计算边带比率和间隔（用于区分AM和FM）
    float leftSum = 0.0f, rightSum = 0.0f;
    uint16_t leftPeak = 0, rightPeak = 0;
    float leftPeakValue = 0.0f, rightPeakValue = 0.0f;
    
    // 搜索载波左侧的最大边带
    for (i = 1; i < carrierBin; i++) {
        leftSum += fftOutput[i];
        if (fftOutput[i] > leftPeakValue) {
            leftPeakValue = fftOutput[i];
            leftPeak = i;
        }
    }
    
    // 搜索载波右侧的最大边带
    for (i = carrierBin + 1; i < FFT_SIZE/2; i++) {
        rightSum += fftOutput[i];
        if (fftOutput[i] > rightPeakValue) {
            rightPeakValue = fftOutput[i];
            rightPeak = i;
        }
    }
    
    // 如果左右边带都存在
    if (leftPeak > 0 && rightPeak > 0) {
        sidebandSpacing = (float)(rightPeak - leftPeak) / 2.0f;
        sidebandRatio = (leftPeakValue + rightPeakValue) / 
                        (2.0f * fftOutput[carrierBin] + 0.00001f);
    }
    
    // 计算边带总能量与载波的比值
    float sideband_energy = leftSum + rightSum;
    float carrier_energy = fftOutput[carrierBin];
    float energy_ratio = sideband_energy / (carrier_energy + 0.00001f);
    
    // 计算边带对称性特征，AM信号的边带更对称
    float symmetry = 1.0f;  // 默认值为最大不对称
    if (leftPeakValue > 0.00001f && rightPeakValue > 0.00001f) {
        symmetry = fabs(leftPeakValue - rightPeakValue) / 
                  (leftPeakValue + rightPeakValue + 0.00001f);
    }
    
//    // ===== 3. 调试信息显示 =====
//    // 修改显示内容，增加对称性参数
//    OLED_ShowString(2, 1, "SPC:");
//    OLED_ShowNum(2, 5, (uint16_t)(sidebandSpacing), 3);  // 显示边带间隔
//    OLED_ShowString(3, 1, "SBR:");
//    OLED_ShowNum(3, 5, (uint16_t)(sidebandRatio*100), 3);
//    OLED_ShowString(4, 1, "SYM:");
//    OLED_ShowNum(4, 5, (uint16_t)(symmetry*100), 3);
    
    // ===== 4. 基于特征识别信号类型 =====
    
    // 优化3V峰峰值信号的识别阈值
    
    // CW信号：几乎没有边带，能量集中在载波
    if ((sidebandRatio < 0.06f) && (energy_ratio < 0.6f)) {
        return SIGNAL_CW;
    }
    
    // AM信号：对称边带，中等边带比率
    // AM的边带间隔应接近调制频率(1kHz)对应的FFT索引
if ((sidebandRatio >= 0.05f) && (sidebandRatio <= 0.6f) && 
    (symmetry < 0.3f) && (sidebandSpacing >= 1.0f && sidebandSpacing <= 3.0f)) {
    return SIGNAL_AM;
}
    
    // FM信号：边带能量更分散，可能不对称
// FM的边带间隔应接近调制频率(5kHz)对应的FFT索引
if ((sidebandRatio > 0.25f) || (energy_ratio > 0.7f) || 
    (sidebandSpacing > 3.0f && sidebandSpacing < 10.0f)) {
    return SIGNAL_FM;
}
    
    return SIGNAL_UNKNOWN;
}
