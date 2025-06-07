// 主函数第二版
       /*  输入信号可为AM、FM、CW三种信号，载波频率30kHz，峰峰值100mV
    输入信号首先被放大至峰峰值3V，然后加直流偏置1.6V
    AM信号：调制频率1kHz，调幅系数 (0.3,1.0)
    FM信号：调制频率5kHz，调频系数 [1,5]，最大频偏未指定范围
    CW信号:
    */

#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "AD.h"
#include "AM.h"
#include "FM.h"
#include "Key.h"

#define BUFFER_SIZE 800 // 存储800个样本点，30kHz信号

// 信号类型定义
#define SIGNAL_UNKNOWN 0
#define SIGNAL_AM      1
#define SIGNAL_FM      2
#define SIGNAL_CW      3

uint16_t ADValue;                          // 定义AD值变量
float Voltage;                             // 定义电压变量
float VoltageBuffer[BUFFER_SIZE];          // 电压数据数组(缓冲区)
uint16_t BufferIndex     = 0;              // 当前存储位置
uint8_t SignalType       = SIGNAL_UNKNOWN; // 当前识别的信号类型
uint8_t SignalIdentified = 0;              // 信号是否已经被识别的标志

int main(void)
{
    /*模块初始化*/
    OLED_Init(); // OLED初始化，OLED大小 4*16
    AD_Init();   // AD初始化
//    Key_Init();  // 按键初始化
    OLED_ShowString(2, 1, "Voltage:0.00V");

    while (1) {
//        // 检测按键
//        uint8_t KeyNum = Key_GetNum();
//        if (KeyNum == 1) {
//            // 按键1按下：切换到AM信号
//            SignalType = SIGNAL_AM;
//            SignalIdentified = 1;  // 设置为已识别状态
//            OLED_Clear();  // 清屏
//        }
//        else if (KeyNum == 2) {
//            // 按键2按下：切换到FM信号
//            SignalType = SIGNAL_FM;
//            SignalIdentified = 1;  // 设置为已识别状态
//            OLED_Clear();  // 清屏
//        }

        // 使用4次采样的平均值
        ADValue = AD_GetAverageValue(4);

        /* 电压 Voltage 与数值 ADValue 线性相关
            电压 Voltage    0~3.3V
            数值 ADValue    0~4095
         */
        Voltage = (float)ADValue / 4095 * 3.3; // 将AD值线性变换到0~3.3的范围，表示电压

        // 显示电压值
        OLED_ShowString(2, 1, "Voltage: ");
        OLED_ShowNum(2, 9, Voltage, 1);
        OLED_ShowNum(2, 11, (uint16_t)(Voltage * 100) % 100, 2);

        // 把电压值保存到数组里
        VoltageBuffer[BufferIndex] = Voltage;

        // 移动到下一个存储位置
        BufferIndex++;

        // 如果到达数组末尾，回到开头（循环缓冲区）
        if (BufferIndex >= BUFFER_SIZE) {
            BufferIndex = 0;

        SignalType = SIGNAL_FM;
        SignalIdentified = 1;
        
        if (SignalIdentified == 1)
        {   //根据识别的信号类型进行处理
            switch (SignalType) {
                case SIGNAL_AM:
                    AM_Demodulate(VoltageBuffer, BUFFER_SIZE);
                    break;

                case SIGNAL_FM:
                    FM_Demodulate(VoltageBuffer, BUFFER_SIZE);
                    break;

                case SIGNAL_CW:
                    // CW信号处理
                    break;
                default:
                    // 未知信号类型处理
                    break;
            }
        }
        }
    }
}
