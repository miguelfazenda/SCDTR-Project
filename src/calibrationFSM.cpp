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

    gainMatrix = new float*[3];
    for(int i = 0; i < 3; ++i)
        gainMatrix[i] = new float[3];
}
CalibrationFSM::~CalibrationFSM() {
    delete gainMatrix;
}

void CalibrationFSM::setState(CalibrationState state)
{
    this->state = state;
    lastTransitionTime = micros();
}

void CalibrationFSM::runStateInit() {
    float residualReading = luminaire.voltageToLux(luminaire.getVoltage());
    communication.sendCalibReady(residualReading);// Writes the mensage that has the residual reading
    numNodesReady++;

    nodeLedOn=-1;//Index from the node ID vector (-1 none of the leds are on)

    setState(CalibrationState::WaitReady);
}
void CalibrationFSM::runStateWaitReady() {

    if (numNodesReady == numTotalNodes){ //every node is ready
        numNodesReady=0; //resets the 

        if (nodeLedOn == -1 || luxReadNum==2)
        {
            nodeLedOn++;//next node to turn on the led
        }
        if (nodeLedOn == nodeId)
        {
            analogWrite(LED_PIN, pwm[luxReadNum]);
	        communication.sendCalibLedOn(); //sends message that indicates that the led is on
            setState(CalibrationState::WaitTransient);
        }
        else
        {
            setState(CalibrationState::WaitLedOn);
        }   
    }
}

void CalibrationFSM::runStateWaitLedOn(){
    if (otherNodeLedOn == true)
    {
        otherNodeLedOn= false;
        setState(CalibrationState::WaitTransient);
    }
    

}
void CalibrationFSM::runStateWaitTrasient(unsigned long timeSinceLastTransition) {
    if(timeSinceLastTransition>500000){ //falta o tau
        luxReads[luxReadNum]= luminaire.voltageToLux(luminaire.getVoltage());

        luxReadNum++;//increments the number of Luxreads

        if (luxReadNum==2)
        {
            gainMatrix[nodeId][nodeLedOn] = (float)(luxReads[0]-luxReads[1]) / (pwm[0]-pwm[1]);
            communication.sendCalibGain(gainMatrix[nodeId][nodeLedOn]);
        }
        else{
            communication.sendCalibReady(0);
            numNodesReady++;
            setState(CalibrationState::WaitReady);
        }
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

void CalibrationFSM::setNodeReady(uint8_t nodeId, float data) {
    //nodesReady++;
    //if all nodes ready
    //   cenas


}