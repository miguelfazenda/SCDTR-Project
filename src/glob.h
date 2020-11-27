#ifndef GLOB_H
#define GLOB_H

#include "luminaire.h"
#include "communication.h"
#include "mainFSM.h"
#include "lpf.h"

//The CAN id of this arduino (uint8_t is one byte)
extern uint8_t nodeId;
extern uint8_t nodesList[10];
extern int numTotalNodes;

extern Communication communication;
extern Luminaire luminaire;
extern MainFSM mainFSM;
extern LPF lpf;
extern CalibrationFSM calibrationFSM;

void registerNewNode(uint8_t id);
#endif