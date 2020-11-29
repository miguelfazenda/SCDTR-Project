#ifndef GLOB_H
#define GLOB_H

#include "luminaire.h"
#include "communication.h"
#include "mainFSM.h"
#include "lpf.h"

#define CALIB_PRINTS

//The CAN id of this arduino (uint8_t is one byte)
extern uint8_t nodeId;
extern uint8_t nodesList[10];
//This vector translates the nodeId to the index it is stored on the gainMatrix (nodeIndexOnGainMatrix[nodeId] = index)
extern uint8_t nodeIndexOnGainMatrix[160];
extern unsigned int numTotalNodes;

extern Communication communication;
extern Luminaire luminaire;
extern MainFSM mainFSM;
extern LPF lpf;
extern CalibrationFSM calibrationFSM;

void registerNewNode(uint8_t id);
#endif