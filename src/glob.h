#ifndef GLOB_H
#define GLOB_H

#include "luminaire.h"
#include "communication.h"
#include "mainFSM.h"
#include "calibrationFSM.h"
#include "lpf.h"

#define CALIB_PRINTS

#define MAX_NODE_ID 160

//The CAN id of this arduino (uint8_t is one byte)
extern uint8_t nodeId;
extern uint8_t nodesList[MAX_NUM_NODES];
//This vector translates the nodeId to the index it is stored on the nodesList, gainMatrix and residualArray (nodeIndexOnGainMatrix[nodeId] = index)
extern uint8_t nodeIndexOnGainMatrix[MAX_NODE_ID+1];
extern uint8_t numTotalNodes;

extern Communication communication;
extern Luminaire luminaire;
extern MainFSM mainFSM;
extern LPF lpf;
extern CalibrationFSM calibrationFSM;

void registerNewNode(uint8_t id);
#endif