#ifndef GLOB_H
#define GLOB_H

#include "luminaire.h"
#include "communication.h"
#include "mainFSM.h"
#include "calibrationFSM.h"
#include "lpf.h"
#include "serialComm.h"
#include "consensus.h"
#include "metrics.h"

#define CALIB_PRINTS

#define MAX_NODE_ID 20

#define EEPROM_ADDR_SLOPEM 0 //Address of where to get M of this arduino on the EEPROM
#define EEPROM_ADDR_SLOPEB 4 //Address of where to get B can id of this arduino on the EEPROM
#define EEPROM_ADDR_NODEID 8 //Address of where to get the can id of this arduino on the EEPROM
#define EEPROM_ADDR_COST 9 //Address of where to get the luminair cost of this arduino on the EEPROM
#define EEPROM_ADDR_a 13 //Address of where to get the a parameter for the tau function (a+b*log())
#define EEPROM_ADDR_b 17 //Address of where to get the b parameter for the tau function (a+b*log())

#define CONTROL_DELAY 10000						//100 Hz corresponds to 1/100 s = 10000us

//The CAN id of this arduino (uint8_t is one byte)
extern uint8_t nodeId;
extern uint8_t nodesList[MAX_NUM_NODES];
//This vector translates the nodeId to the index it is stored on the nodesList, gainMatrix and residualArray (nodeIndexOnGainMatrix[nodeId] = index)
extern uint8_t nodeIndexOnGainMatrix[MAX_NODE_ID+1];
extern uint8_t numTotalNodes;
//Which node is the hub node (0 for none)
extern uint8_t hubNode;
extern unsigned long timeSinceLastReset;
//This is set to true when it's time to do the control (and send frequent data)
extern bool didControl;

//extern unsigned int lastResetTime; //time since last restart

extern Communication communication;
extern Luminaire luminaire;
extern MainFSM mainFSM;
extern LPF lpf;
extern CalibrationFSM calibrationFSM;
extern SerialComm serialComm;
extern Consensus consensus;
extern Metrics metrics;


void registerNewNode(uint8_t id);
bool checkIfNodeExists(uint8_t destination);
void resetGlob();
#endif