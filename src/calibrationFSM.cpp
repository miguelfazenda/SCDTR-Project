#include "calibrationFSM.h"
#include <Arduino.h>
#include "glob.h"

CalibrationFSM::CalibrationFSM()
{
    state = CalibrationState::CalibInit;
    numNodesReady=0;
    luxReadNum=0;  //number of the Lux read
    //PWM values
    pwm[0]=20;pwm[1]=240;
    luxReads[0]=0;luxReads[1]=0;
    otherNodeLedOn=false;
}
CalibrationFSM::~CalibrationFSM() {
    delete gainMatrix;
    //TODO delete gainMatrix sub vectors
}

void CalibrationFSM::setState(CalibrationState state)
{
    this->state = state;
    lastTransitionTime = micros();
}

void CalibrationFSM::runStateInit() {
    float residualReading = luminaire.voltageToLux(luminaire.getVoltage());
    communication.sendCalibReady(residualReading);// Writes the mensage that has the residual reading
    incrementNodesReady();

    uint8_t maxNodeId = nodesList[numTotalNodes-1];
    Serial.print("ALOCATED ");
    Serial.print(maxNodeId);
    Serial.print("X");
    Serial.println(maxNodeId);

    /*gainMatrix = new float*[maxNodeId+1];
    for(int i = 0; i < maxNodeId+1; ++i) {
        gainMatrix[i] = new float[maxNodeId+1];
        for(int j = 0; j < maxNodeId+1; ++j)
            gainMatrix[i][j] = 0.0f;
    }*/

    nodeLedOn=-1; //Index from the node ID vector (-1 none of the leds are on)

    Serial.println("[CalibFSM] State = WaitReady");
    setState(CalibrationState::WaitReady);
}
void CalibrationFSM::runStateWaitReady() {
    analogWrite(LED_PIN, 0); //TODO isto aqui corre muitas vezes :/

    if (numNodesReady == numTotalNodes){ //every node is ready
        numNodesReady = 0; //resets the 
        Serial.println("[CalibFSM] All nodes ready!");

        if (nodeLedOn == -1 || luxReadNum == 2)
        {
            nodeLedOn++;//next node to turn on the led
            luxReadNum = 0; //Reset reading number
        }

        if(nodeLedOn >= numTotalNodes)
        {
            Serial.println("DONE calibrating!");
        }
        else if (nodesList[nodeLedOn] == nodeId)
        {
            //If it's this node's turn to turn on LED, broadcast that it turned it on,
            //  and then wait for the transient of the measure
            analogWrite(LED_PIN, pwm[luxReadNum]);
	        communication.sendCalibLedOn(); //sends message that indicates that the led is on
            Serial.println("[CalibFSM] State = WaitTransient");
            setState(CalibrationState::WaitTransient);
        }
        else
        {
            //If it's some other node's turn to turn on LED, go to state that waits for it to turn it on
            Serial.println("[CalibFSM] State = WaitLedOn");
            setState(CalibrationState::WaitLedOn);
        }   
    }
}

void CalibrationFSM::runStateWaitLedOn(){
    //otherNodeLedOn is an Event.
    // When communications receives that a node turned on LED, otherNodeLedOn == true
    if (otherNodeLedOn)
    {
        otherNodeLedOn = false; //Reset event bool
        //Go to state that waits for the transient of the measure 
        Serial.println("[CalibFSM] State = WaitTransient");
        setState(CalibrationState::WaitTransient);
    }
}
void CalibrationFSM::runStateWaitTrasient(unsigned long timeSinceLastTransition) {
    //After some time has passed, the measure should be stable
    if(timeSinceLastTransition>1000000) { //falta o tau
        //Measure 
        luxReads[luxReadNum]= luminaire.voltageToLux(luminaire.getVoltage());
        Serial.print("luxReads[");
        Serial.print(luxReadNum);
        Serial.print("] = ");
        Serial.println(luxReads[luxReadNum]);
        luxReadNum++;//increments the number of Luxreads

        //float calculatedGain
        if (luxReadNum == 2)
        {
            //gainMatrix[nodeId][nodesList[nodeLedOn]] = (float)(luxReads[0]-luxReads[1]) / (pwm[0]-pwm[1]);
            //communication.sendCalibGain(0/*gainMatrix[nodeId][nodesList[nodeLedOn]]*/);
        }

        communication.sendCalibReady(0);
        incrementNodesReady();
        Serial.println("[CalibFSM] State = WaitReady");
        setState(CalibrationState::WaitReady);
    }
}

void CalibrationFSM::loop() {
    unsigned long currentTime = micros();
    unsigned long timeSinceLastTransition = currentTime - lastTransitionTime;

    switch (state)
    {
    case CalibrationState::CalibInit:
        runStateInit();
        break;
    case CalibrationState::WaitReady:
        runStateWaitReady();
        break;
    case CalibrationState::WaitLedOn:
        runStateWaitLedOn();
        break;
    case CalibrationState::WaitTransient:
        runStateWaitTrasient(timeSinceLastTransition);
        break;
    default:
        break;
    }
}

void CalibrationFSM::incrementNodesReady() {
    numNodesReady++;
    Serial.print("numNodesReady = ");
    Serial.println(calibrationFSM.numNodesReady);
}