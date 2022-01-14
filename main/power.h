#ifndef POWER_H
#define POWER_H


void Ina3221Init(void);
void Ina3221Measurement(void);
void measurePowerTask(void *pvParameters);

#endif