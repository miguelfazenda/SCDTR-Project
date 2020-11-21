#ifndef LPF_H
#define LPF_H

#include "util.h"

#define LPF_TOTAL_SAMPLES 20	//Number of samples the LPF moving average stores

class LPF
{
private:
    int lpfNumSamples = 0;
    int lpfIdx = 0;
    float lpfSamples[LPF_TOTAL_SAMPLES] = {0};
public:
    void sample();
    float value();
};

#endif