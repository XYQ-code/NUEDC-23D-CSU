// AD.h第二版

#ifndef __AD_H
#define __AD_H

void AD_Init(void);
uint16_t AD_GetValueWhenReady(void);
uint16_t AD_GetAverageValue(uint8_t times);
uint16_t AD_GetMedianValue(uint8_t times);


#endif
