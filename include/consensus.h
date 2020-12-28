#ifndef CONSENSUS_H
#define CONSENSUS_H

#include "glob.h"

class Consensus
{
private:
    float dutyCycleBest[MAX_NUM_NODES] = {0.0};
    float dutyCycleAv[MAX_NUM_NODES] = {0.0};
    float lagrangeMultipliers[MAX_NUM_NODES] = {0.0};
    float ki[MAX_NUM_NODES] = {0.0};
    float kiNorm = 0;
    float kinormKiiDiff = 0;
    float cost[MAX_NUM_NODES] = {0.0};
    float receivedDutyCycle[MAX_NUM_NODES] = {0.0};
    float LiRef = 0;
    float rho = 0.07;
    uint8_t nodeIdx = -1;
    uint8_t maxIter = 50;
    uint8_t numberOfMsgReceived = 0;
    bool waitingMsg = true;

public:

    Consensus();
    float evaluate_cost(float*);
    bool check_feasibility(float*);
    void consensus_iterate();
    void consensus_main();
    float multiplyTwoArrays(float*, float*, uint8_t);
    void receivedMsg(float*);
};

#endif