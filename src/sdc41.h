#ifndef SDC41_H
#define SDC41_H

int initSDC41(void);
bool measureCO2(uint16_t* co2, float* temperature, float* humidity);

#endif // SDC41_H
