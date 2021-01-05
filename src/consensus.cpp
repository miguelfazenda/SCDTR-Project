#include "controller.h"
#include "consensus.h"
#include "luminaire.h"
#include "calibrationFSM.h"
#include "glob.h"

void Consensus::init()
{
    // Serial.println(F("---- CONSENSUS INIT"));
    
    //Number of messages sent by each node * number of nodes
    //Each message can send 3 values of the dutyCycle vector
    numberOfMsgExpected = ((numTotalNodes - 1)/3 + 1) * (numTotalNodes-1);

    nodeIdx = nodeIndexOnGainMatrix[nodeId];
    consensusState = 1;
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
     Serial.print(F("No init ref = ")); Serial.println(LiRef);
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
            Serial.print(F("Foi escolhido unconstrained -> "));
            Serial.println(cost_unconstrained);
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = cost_unconstrained;
        }
    }

    //---------------- Compute minimum constrained to linear boundary ----------------
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        Serial.print(F("rho= "));
        Serial.print(rho);
        Serial.print(F(" Z= "));
        Serial.print(z[i]);
        Serial.print(F(" o= "));
        Serial.print(calibrationFSM.residualArray[nodeIdx]);
        Serial.print(F(" LiRef= "));
        Serial.print(LiRef);
        Serial.print(F(" 1/rho*Z= "));
        Serial.print(1 / rho * z[i]);
        Serial.print(F(" ki[i] / kiNorm= "));
        Serial.print(ki[i] / kiNorm);
        Serial.print(F(" parentesis= "));
        Serial.println((calibrationFSM.residualArray[nodeIdx] - LiRef + 1 / rho * multiplyTwoArrays(z, ki, numTotalNodes)));
        dutyCycleTest[i] = 1 / rho * z[i] - ki[i] / kiNorm * (calibrationFSM.residualArray[nodeIdx] - LiRef + 1 / rho * multiplyTwoArrays(z, ki, numTotalNodes));
    }
    

    //check feasibility of minimum constrained to linear boundary
    float costBoundaryLinear = 0.0;
    if (check_feasibility(dutyCycleTest))
    {
        costBoundaryLinear = evaluate_cost(dutyCycleTest);
        if (costBoundaryLinear < costBest)
        {
            Serial.print(F("Foi escolhido linear -> "));
            Serial.println(costBoundaryLinear);
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
            Serial.print(F("Foi escolhido 0 -> "));
            Serial.println(costBoundary0);
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
            Serial.print(F("Foi escolhido 100 -> "));
            Serial.println(costBoundary100);
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
            Serial.print(F("Foi escolhido L0 -> "));
            Serial.println(costBoundaryL0);
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
        Serial.print(F("No L0 CB < Cbest? -> "));
            Serial.println(costBoundaryL100 < costBest);
            Serial.println(costBoundaryL100);
            Serial.println(costBest);
        if (costBoundaryL100 < costBest)
        {
            Serial.print(F("Foi escolhido L100 -> "));
            Serial.println(costBoundaryL100);
            memcpy(dutyCycleBest, dutyCycleTest, numTotalNodes * sizeof(float));
            costBest = costBoundaryL100;
        }
    }
}

void Consensus::consensus_main()
{

    if (consensusState == 1)
    {
        Serial.println(F("-- Consensus Iterate --"));
        consensus_iterate();
        Serial.println(dutyCycleBest[0]);
        Serial.println(dutyCycleBest[1]);
        Serial.println(dutyCycleBest[2]);
        consensusState = 2;
    }

    if (consensusState == 2)
    {
        Serial.println(F("-------- Sending dutyCycle ----------"));
        communication.sendConsensusDutyCycle(dutyCycleBest);
        consensusState = 3;
    }

    if (consensusState == 3)
    {
        Serial.print(F("numberOfMsgReceived -> ")); Serial.println(numberOfMsgReceived);
        if (numberOfMsgReceived == numberOfMsgExpected) //Ou >= ?????
        {
            consensusState = 4;
        }
    }
    //Serial.print(F("Consensus -> ")); Serial.println(consensusState);
    if (consensusState == 4)
    {
        //Serial.println(F("---AVG----"));
        for (uint8_t j = 0; j < numTotalNodes; j++)
        {
            dutyCycleAv[j] = (receivedDutyCycle[j] + dutyCycleBest[j]) / (numberOfMsgReceived+1);
            lagrangeMultipliers[j] += rho * (dutyCycleBest[j] - dutyCycleAv[j]);
        }
        //Serial.println(F("---END AVG----"));
        numberOfMsgReceived = 0;
        consensusState = 1;
        numIter += 1;
        for (uint8_t i = 0; i < numTotalNodes; i++)
        {
            receivedDutyCycle[i] = 0;
        }
        Serial.print(numIter); Serial.print("   "); Serial.print(maxIter);  Serial.print("   "); 
        if (numIter == maxIter)
        {
            consensusState = 0;
            numIter = 0;
            numberOfMsgReceived = 0;

            Serial.println(dutyCycleBest[0]);
            Serial.println(dutyCycleBest[1]);
            Serial.println(dutyCycleBest[2]);
            
            //New lux reference, after consensus. dutyCycleBest is in %, we must convert it to 0-255 range and convert to lux
            //float newLuxRef = (dutyCycleBest[nodeIdx]*255.0/100.0) * ki[nodeIdx] - calibrationFSM.residualArray[nodeIdx];
            float newLuxRef = 0.0;
            Serial.println(F("---- MULti ------"));
            for(uint8_t i = 0; i < numTotalNodes; i++)
            {
                Serial.print(dutyCycleBest[i]);Serial.print(" * ");Serial.println(ki[i]);
                newLuxRef += (dutyCycleBest[i]) * ki[i];
            }
            newLuxRef -= calibrationFSM.residualArray[nodeIdx];
            Serial.print(F("No FINAL DO CONSENSUS ----->"));
        	Serial.println(newLuxRef);
            luminaire.setLuxRefAfterConsensus(newLuxRef); //Change luxRefAfterConsensus and starts simulation
        }
    }
}

// void Consensus::receivedMsg(float *receivedArray)
// {
//     for (uint8_t i = 0; i < numTotalNodes; i++)
//     {
//         receivedDutyCycle[i] += receivedArray[i];
//         //Serial.println(receivedArray[i]);
//     }
//     numberOfMsgReceived += 1;
// }

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