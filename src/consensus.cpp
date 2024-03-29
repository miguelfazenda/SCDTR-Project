#include "controller.h"
#include "consensus.h"
#include "luminaire.h"
#include "calibrationFSM.h"
#include "glob.h"

void Consensus::init()
{
    //Number of messages sent by each node * number of nodes
    //Each message can send 3 values of the dutyCycle vector
    numberOfMsgExpected = ((numTotalNodes - 1)/3 + 1) * (numTotalNodes-1);

    nodeIdx = nodeIndexOnGainMatrix[nodeId];
    consensusState = ITERATE_STATE;
    numIter = 0;
    numberOfMsgReceived = 0;
    //cost[nodeIdx] = luminaire.cost;
    //ki = calibrationFSM.gainMatrix[nodeIdx];
    
    kiNorm = 0;
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        //Changes gain values to %
        ki[i] = calibrationFSM.gainMatrix[nodeIdx][i]*255.0/100.0; 

        kiNorm += pow(ki[i], 2);
        dutyCycleBest[i] = 0.0;
        dutyCycleAv[i] = 0.0;
        lagrangeMultipliers[i] = 0.0;
        receivedDutyCycle[i] = 0.0;
    }

    kinormKiiDiff = kiNorm - pow(ki[nodeIdx], 2);
    LiRef = luminaire.luxRef;
}

float Consensus::evaluate_cost(float *dutyClicleToCheck)
{
    float norm = 0.0;
    float diff[numTotalNodes];
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        norm += pow(dutyClicleToCheck[i] - dutyCycleAv[i], 2);
        diff[i] = dutyClicleToCheck[i] - dutyCycleAv[i];
    }
    //return multiplyTwoArrays(cost, dutyClicleToCheck, numTotalNodes) + multiplyTwoArrays(lagrangeMultipliers, diff, numTotalNodes) + rho / 2 * norm;
    return luminaire.cost * dutyClicleToCheck[nodeIdx] + multiplyTwoArrays(lagrangeMultipliers, diff, numTotalNodes) + rho / 2 * norm;
}

bool Consensus::check_feasibility(float *dutyClicleToCheck)
{
    float tol = 0.01;

    if (dutyClicleToCheck[nodeIdx] < 0 - tol)
        return false;
    if (dutyClicleToCheck[nodeIdx] > 100 + tol)
        return false;
    if (multiplyTwoArrays(dutyClicleToCheck, ki, numTotalNodes) < LiRef - calibrationFSM.residualArray[nodeIdx] - tol)
        return false;
    return true;
}

void Consensus::consensus_iterate()
{
    //float *dutyCycleBest = NULL;
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleBest[i] = -1;
    }

    float costBest = 100000; // large number

    // ---------------- z value and unconstrained solution ----------------
    float z[numTotalNodes] = {0};
    float dutyCycleTest[numTotalNodes] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        z[i] = rho * dutyCycleAv[i] - lagrangeMultipliers[i];
        dutyCycleTest[i] = 1 / rho * z[i];
    }
    //
    z[nodeIdx] -= luminaire.cost;
    dutyCycleTest[nodeIdx] = 1 / rho * z[nodeIdx];

    float cost_unconstrained = 0.0;
    if (check_feasibility(dutyCycleTest))
    {
        cost_unconstrained = evaluate_cost(dutyCycleTest);
        if (cost_unconstrained < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = cost_unconstrained;
        }
    }

    //---------------- Compute minimum constrained to linear boundary ----------------
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleTest[i] = 1 / rho * z[i] - ki[i] / kiNorm * (calibrationFSM.residualArray[nodeIdx] - LiRef + 1 / rho * multiplyTwoArrays(z, ki, numTotalNodes));
    }
    

    //check feasibility of minimum constrained to linear boundary
    float costBoundaryLinear = 0.0;
    if (check_feasibility(dutyCycleTest))
    {
        costBoundaryLinear = evaluate_cost(dutyCycleTest);
        if (costBoundaryLinear < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = costBoundaryLinear;
        }
    }

    //---------------- Compute minimum constrained to 0 boundary ----------------
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleTest[i] = 1 / rho * z[i];
    }
    dutyCycleTest[nodeIdx] = 0;

    //check feasibility of minimum constrained to 0 boundary
    float costBoundary0 = 0.0;
    if (check_feasibility(dutyCycleTest))
    {
        costBoundary0 = evaluate_cost(dutyCycleTest);
        if (costBoundary0 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = costBoundary0;
        }
    }

    //---------------- Compute minimum constrained to 100 boundary ----------------
    //float dutyCycleBest100[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleTest[i] = 1 / rho * z[i];
    }
    dutyCycleTest[nodeIdx] = 100;

    //check feasibility of minimum constrained to 100 boundary
    //float costBoundary100 = 0.0;
    if (check_feasibility(dutyCycleTest))
    {
        float costBoundary100 = evaluate_cost(dutyCycleTest);
        if (costBoundary100 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = costBoundary100;
        }
    }

    //---------------- Compute minimum constrained to linear and 0 boundary ----------------
    //float dutyCycleBestL0[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleTest[i] = 1 / rho * z[i] - ki[i] / kinormKiiDiff * (calibrationFSM.residualArray[nodeIdx] - LiRef) + ki[i] / (rho * kinormKiiDiff) * (ki[nodeIdx] * z[nodeIdx] - multiplyTwoArrays(z, ki, numTotalNodes));
    }
    dutyCycleTest[nodeIdx] = 0;

    //check feasibility
    float costBoundaryL0 = 0.0;
    if (check_feasibility(dutyCycleTest))
    {
        costBoundaryL0 = evaluate_cost(dutyCycleTest);
        if (costBoundaryL0 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = costBoundaryL0;
        }
    }

    //---------------- Compute minimum constrained to linear and 100 boundary ----------------
    //float dutyCycleBestL100[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleTest[i] = 1 / rho * z[i] - ki[i] / kinormKiiDiff * (calibrationFSM.residualArray[nodeIdx] - LiRef + 100 * ki[nodeIdx]) + ki[i] / (rho * kinormKiiDiff) * (ki[nodeIdx] * z[nodeIdx] - multiplyTwoArrays(z, ki, numTotalNodes));
    }
    dutyCycleTest[nodeIdx] = 100;

    //check feasibility
    float costBoundaryL100 = 0.0;
    if (check_feasibility(dutyCycleTest))
    {
        costBoundaryL100 = evaluate_cost(dutyCycleTest);
        if (costBoundaryL100 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = costBoundaryL100;
        }
    }
}

void Consensus::consensus_main()
{

    if (consensusState == ITERATE_STATE)
    {
        consensus_iterate();
        consensusState = SENDING_DUTY_CYCLE_STATE;
    }

    if (consensusState == SENDING_DUTY_CYCLE_STATE)
    {
        communication.sendConsensusDutyCycle(dutyCycleBest);
        consensusState = WAITING_RECIVE_DUTY_CYCLE_STATE;
    }

    if (consensusState == WAITING_RECIVE_DUTY_CYCLE_STATE)
    {
        if (numberOfMsgReceived == numberOfMsgExpected)
        {
            consensusState = UPDATE_STATE;
        }
    }
    if (consensusState == UPDATE_STATE)
    {
        for (uint8_t j = 0; j < numTotalNodes; j++)
        {
            dutyCycleAv[j] = (receivedDutyCycle[j] + dutyCycleBest[j]) / (numberOfMsgReceived+1);
            lagrangeMultipliers[j] += rho * (dutyCycleBest[j] - dutyCycleAv[j]);
        }
        numberOfMsgReceived = 0;
        consensusState = 1;
        numIter += 1;
        for (uint8_t i = 0; i < numTotalNodes; i++)
        {
            receivedDutyCycle[i] = 0;
        }
        if (numIter == maxIter)
        {
            consensusState = OFF_STATE;
            numIter = 0;
            numberOfMsgReceived = 0;
            
            //New lux reference, after consensus. dutyCycleBest is in %, we must convert it to 0-255 range and convert to lux
            //float newLuxRef = (dutyCycleBest[nodeIdx]*255.0/100.0) * ki[nodeIdx] - calibrationFSM.residualArray[nodeIdx];
            float newLuxRef = 0.0;
            for(uint8_t i = 0; i < numTotalNodes; i++)
            {
                newLuxRef += (dutyCycleBest[i]) * ki[i];
            }
            newLuxRef += calibrationFSM.residualArray[nodeIdx];
            luminaire.setLuxRefAfterConsensus(newLuxRef); //Change luxRefAfterConsensus and starts simulation
        }
    }
}

float Consensus::multiplyTwoArrays(float *array1, float *array2, uint8_t dimention)
{
    float mult = 0.0;
    for (int i = 0; i < numTotalNodes; i++)
    {
        mult += array1[i] * array2[i];
    }
    return mult;
}

void Consensus::resetConsensus()
{
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleAv[i] = 0.0;
        lagrangeMultipliers[i] = 0.0;
        receivedDutyCycle[i] = 0.0;
        dutyCycleBest[i] = 0.0;
        ki[i] = 0;
    }

    kiNorm = 0;
    kinormKiiDiff = 0;
    LiRef = 0;
    rho = 0.07;
    nodeIdx = -1;
    maxIter = 50;
    numberOfMsgReceived = 0;
    numIter = 0;
    consensusState = 0;
}