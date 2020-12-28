#include "controller.h"
#include "consensus.h"
#include "luminaire.h"
#include "calibrationFSM.h"
#include "glob.h"

Consensus::Consensus()
{
    cost[nodeId] = 1.0; // TO DO: ir buscar ao sitio certo!!

    nodeIdx = nodeIndexOnGainMatrix[nodeId];
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        ki[i] = calibrationFSM.gainMatrix[nodeIdx][i];
        kiNorm += pow(ki[i], 2);
    }

    kinormKiiDiff = kiNorm - pow(ki[nodeIdx], 2);

    LiRef = luminaire.luxRef;
}

float Consensus::evaluate_cost(float *dutyClicleToCheck)
{
    float norm = 0.0;
    float diff[MAX_NUM_NODES];
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        norm += pow(dutyClicleToCheck[i] - dutyCycleAv[i], 2);
        diff[i] = dutyClicleToCheck[i] - dutyCycleAv[i];
    }

    return multiplyTwoArrays(cost, dutyClicleToCheck, numTotalNodes) + multiplyTwoArrays(cost, diff, numTotalNodes) + rho / 2 * norm;
}

bool Consensus::check_feasibility(float *dutyClicleToCheck)
{
    float tol = 0.001;

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
    int costBest = 10000; // large number

    // ---------------- z value and unconstrained solution ----------------
    float z[MAX_NUM_NODES] = {0};
    float dutyCycleUnconstraint[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        z[i] = rho * dutyCycleAv[i] - lagrangeMultipliers[i] - cost[i];
        dutyCycleUnconstraint[i] = 1 / rho * z[i];
    }

    float cost_unconstrained = 0.0;
    if (check_feasibility(dutyCycleUnconstraint))
    {
        cost_unconstrained = evaluate_cost(dutyCycleUnconstraint);
        if (cost_unconstrained < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleUnconstraint, numTotalNodes*sizeof(float));
            //dutyCycleBest = dutyCycleUnconstraint;
            costBest = cost_unconstrained;
        }
    }

    //---------------- Compute minimum constrained to linear boundary ----------------
    float dutyCycleBestLinear[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleBestLinear[i] = 1 / rho * z[i] - ki[i] / kiNorm * (calibrationFSM.residualArray[i] - LiRef + 1 / rho * z[i] * ki[i]);
    }

    //check feasibility of minimum constrained to linear boundary
    float costBoundaryLinear = 0.0;
    if (check_feasibility(dutyCycleBestLinear))
    {
        costBoundaryLinear = evaluate_cost(dutyCycleBestLinear);
        if (costBoundaryLinear < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleBestLinear, numTotalNodes*sizeof(float));
            costBest = costBoundaryLinear;
        }
    }

    //---------------- Compute minimum constrained to 0 boundary ----------------
    float dutyCycleBest0[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleBest0[i] = 1 / rho * z[i];
    }
    dutyCycleBest0[nodeId] = 0;

    //check feasibility of minimum constrained to 0 boundary
    float costBoundary0 = 0.0;
    if (check_feasibility(dutyCycleBest0))
    {
        costBoundary0 = evaluate_cost(dutyCycleBest0);
        if (costBoundary0 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleBest0, numTotalNodes*sizeof(float));
            costBest = costBoundary0;
        }
    }

    //---------------- Compute minimum constrained to 100 boundary ----------------
    float dutyCycleBest100[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleBest100[i] = 1 / rho * z[i];
    }
    dutyCycleBest100[nodeId] = 100;

    //check feasibility of minimum constrained to 100 boundary
    float costBoundary100 = 0.0;
    if (check_feasibility(dutyCycleBest100))
    {
        costBoundary100 = evaluate_cost(dutyCycleBest100);
        if (costBoundary100 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleBest100, numTotalNodes*sizeof(float));
            costBest = costBoundary100;
        }
    }

    //---------------- Compute minimum constrained to linear and 0 boundary ----------------
    float dutyCycleBestL0[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleBestL0[i] = 1 / rho * z[i] - ki[i] / kinormKiiDiff * (calibrationFSM.residualArray[i] - LiRef) + ki[i] / (rho * kinormKiiDiff) * (ki[nodeId] * z[nodeId] - z[i] * ki[i]);
    }

    dutyCycleBestL0[nodeId] = 0;

    //check feasibility
    float costBoundaryL0 = 0.0;
    if (check_feasibility(dutyCycleBestL0))
    {
        costBoundaryL0 = evaluate_cost(dutyCycleBestL0);
        if (costBoundaryL0 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleBestL0, numTotalNodes*sizeof(float));
            costBest = costBoundaryL0;
        }
    }

    //---------------- Compute minimum constrained to linear and 100 boundary ----------------
    float dutyCycleBestL100[MAX_NUM_NODES] = {0};
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        dutyCycleBestL100[i] = 1 / rho * z[i] - ki[i] / kinormKiiDiff * (calibrationFSM.residualArray[i] - LiRef + 100 * ki[nodeId]) + ki[i] / (rho * kinormKiiDiff) * (ki[nodeId] * z[nodeId] - z[i] * ki[i]);
    }
    dutyCycleBestL100[nodeId] = 100;

    //check feasibility
    float costBoundaryL100 = 0.0;
    if (check_feasibility(dutyCycleBestL100))
    {
        costBoundaryL100 = evaluate_cost(dutyCycleBestL100);
        if (costBoundaryL100 < costBest)
        {
            memcpy(dutyCycleBest, dutyCycleBestL100, numTotalNodes*sizeof(float));
            costBest = costBoundaryL100;
        }
    }
}

void Consensus::consensus_main()
{
    for (uint8_t i = 0; i < maxIter; i++)
    {
        consensus_iterate();

        //Protocolo de enviar e receber dutyCycleBest's (Maquina de estados?)

        //Averaging dutyCycleBest AND Computation of the LAGRANGIAN UPDATES
        for (uint8_t j = 0; j < numTotalNodes; j++)
        {
            for (uint8_t h = 0; h < numTotalNodes; h++)
            {
                dutyCycleAv[j] += dutyCycleBest[h]; //!!!!
            }
            dutyCycleAv[j] = dutyCycleAv[j] / numTotalNodes;

            lagrangeMultipliers[j] += rho * (dutyCycleBest[j] - dutyCycleAv[j]);
        }
    }
}

void Consensus::receivedMsg(float* receivedArray)
{
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        receivedDutyCycle[i] += receivedArray[i];
    }
    numberOfMsgReceived += 1;
    if(numberOfMsgReceived == numTotalNodes-1)
    {
        waitingMsg = false;
        consensus_main();
    } 
}

float Consensus::multiplyTwoArrays(float *array1, float *array2, uint8_t dimention)
{
    float mult = 0.0;
    for (int i = 0; i < dimention; i++)
    {
        mult += array1[i] * array2[i];
    }

    return mult;
}
