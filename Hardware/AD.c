// AD.c 第二版
#include "stm32f10x.h"

/**
  * 函    数：AD初始化
  * 参    数：无
  * 返 回 值：无
  */
void AD_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);	//开启ADC1的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	
	/*设置ADC时钟*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);						//选择时钟6分频，ADCCLK = 72MHz / 6 = 12MHz
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//将PA0引脚初始化为模拟输入
	
	/*规则组通道配置*/
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_13Cycles5);		//规则组序列1的位置，配置为通道0
	
	/*ADC初始化*/
	ADC_InitTypeDef ADC_InitStructure;						//定义结构体变量
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;		//模式，选择独立模式，即单独使用ADC1
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;	//数据对齐，选择右对齐
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//外部触发，使用软件触发，不需要外部触发
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;		//连续转换
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;			//扫描模式，失能，只转换规则组的序列1这一个位置
	ADC_InitStructure.ADC_NbrOfChannel = 1;					//通道数，为1，仅在扫描模式下，才需要指定大于1的数，在非扫描模式下，只能是1
	ADC_Init(ADC1, &ADC_InitStructure);						//将结构体变量交给ADC_Init，配置ADC1
	
	/*ADC使能*/
	ADC_Cmd(ADC1, ENABLE);									//使能ADC1，ADC开始运行
	
	/*ADC校准*/
	ADC_ResetCalibration(ADC1);								//固定流程，内部有电路会自动执行校准
	while (ADC_GetResetCalibrationStatus(ADC1) == SET);
	ADC_StartCalibration(ADC1);
	while (ADC_GetCalibrationStatus(ADC1) == SET);

	ADC_SoftwareStartConvCmd(ADC1, ENABLE);					//软件触发AD转换一次
}

/**
  * 函    数：等待AD转换完成并获取值
  * 参    数：无
  * 返 回 值：AD转换的值，范围：0~4095
  * 说    明：确保读取到的是完成转换后的值
  */
uint16_t AD_GetValueWhenReady(void)
{
    // 等待转换完成
    while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    
    // 清除EOC标志
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    
    // 返回转换结果
    return ADC_GetConversionValue(ADC1);
}

/**
  * 函    数：获取多次AD采样的平均值
  * 参    数：times: 采样次数
  * 返 回 值：平均后的AD值，范围：0~4095
  * 说    明：多次采样平均可以降低随机噪声的影响
  */
uint16_t AD_GetAverageValue(uint8_t times)
{
    uint32_t sum = 0;
    
    for (uint8_t i = 0; i < times; i++)
    {
        sum += AD_GetValueWhenReady();
    }
    
    return sum / times;
}

/**
  * 函    数：获取中值滤波后的AD值
  * 参    数：times: 采样次数（建议为奇数）
  * 返 回 值：中值滤波后的AD值，范围：0~4095
  * 说    明：中值滤波对抑制脉冲干扰特别有效
  */
uint16_t AD_GetMedianValue(uint8_t times)
{
    uint16_t buffer[15];  // 最多支持15次采样
    uint16_t temp;
    
    if (times > 15) times = 15;  // 限制最大采样次数
    
    // 采集多次AD值
    for (uint8_t i = 0; i < times; i++)
    {
        buffer[i] = AD_GetValueWhenReady();
    }
    
    // 冒泡排序
    for (uint8_t i = 0; i < times - 1; i++)
    {
        for (uint8_t j = 0; j < times - i - 1; j++)
        {
            if (buffer[j] > buffer[j + 1])
            {
                temp = buffer[j];
                buffer[j] = buffer[j + 1];
                buffer[j + 1] = temp;
            }
        }
    }
    
    // 返回中值
    return buffer[times / 2];
}
