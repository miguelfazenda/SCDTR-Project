#ifndef GLOB_H
#define GLOB_H

#include "luminaire.h"
#include "communication.h"
#include "mainFSM.h"
#include "calibrationFSM.h"
#include "lpf.h"

#define CALIB_PRINTS

#define MAX_NODE_ID 160

#define EEPROM_ADDR_SLOPEM 0 //Address of where to get M of this arduino on the EEPROM
#define EEPROM_ADDR_SLOPEB 4 //Address of where to get B can id of this arduino on the EEPROM
#define EEPROM_ADDR_NODEID 8 //Address of where to get the can id of this arduino on the EEPROM

//The CAN id of this arduino (uint8_t is one byte)
extern uint8_t nodeId;
extern uint8_t nodesList[MAX_NUM_NODES];
//This vector translates the nodeId to the index it is stored on the nodesList, gainMatrix and residualArray (nodeIndexOnGainMatrix[nodeId] = index)
extern uint8_t nodeIndexOnGainMatrix[MAX_NODE_ID+1];
extern uint8_t numTotalNodes;

//extern unsigned int lastResetTime; //time since last restart

extern Communication communication;
extern Luminaire luminaire;
extern MainFSM mainFSM;
extern LPF lpf;
extern CalibrationFSM calibrationFSM;



void registerNewNode(uint8_t id);
#endif