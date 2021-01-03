#ifndef CONSENSUS_H
#define CONSENSUS_H

#include "calibrationFSM.h"

class Consensus
{
private:
    float dutyCycleAv[MAX_NUM_NODES] = {0.0};
    float lagrangeMultipliers[MAX_NUM_NODES] = {0.0};
    float ki[MAX_NUM_NODES] = {0.0};
    //float* ki = nullptr;
    float kiNorm = 0;
    float kinormKiiDiff = 0;
    float LiRef = 0;
    float rho = 0.07;
    uint8_t nodeIdx = -1;
    uint8_t maxIter = 50;
    uint8_t numIter = 0;
    //Number of messages that it has to received to continue to the next iteration
    uint16_t numberOfMsgExpected;

    float multiplyTwoArrays(float*, float*, uint8_t);
    float evaluate_cost(float*);
    bool check_feasibility(float*);
    void consensus_iterate();


public:
    uint8_t consensusState = 0; // 0->Nada; 1->ConsensusIterate; 2->Em comunicação; 3-> A espera de mensagens; 4->Atualização de variaveis
    float dutyCycleBest[MAX_NUM_NODES] = {0.0};
    float receivedDutyCycle[MAX_NUM_NODES] = {0.0};
    uint16_t numberOfMsgReceived = 0;
    void init();
    void consensus_main();
    //void receivedMsg(float*);
    void resetConsensus();
};

#endif