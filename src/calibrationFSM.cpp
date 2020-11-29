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

    gainMatrixSize = 0;
    gainMatrix = nullptr;

    done = false;
}
CalibrationFSM::~CalibrationFSM() {
    freeGainMatrix();
}

void CalibrationFSM::setState(CalibrationState state)
{
    this->state = state;
    lastTransitionTime = micros();
}

void CalibrationFSM::runStateInit() {
    //Allocates gain matrix
    allocateGainMatrix();

    //Reads residual luminance and saves it on the gainMatrix
    float residualReading = luminaire.voltageToLux(luminaire.getVoltage());
    Serial.println(residualReading);
    //gainMatrix[nodeIndexOnGainMatrix[nodeId]][0] = residualReading; //TODO confirmar isto!!!!!!!!
    setGain(nodeIndexOnGainMatrix[nodeId], 0, residualReading);

    //Sends that this node is ready for the next step
    communication.sendCalibReady(residualReading);// Writes in the message that has the residual reading
    incrementNodesReady();

    nodeLedOn=-1; //Index from the node ID vector (-1 none of the leds are on)

#ifdef CALIB_PRINTS
    Serial.println("[CalibFSM] State = WaitReady");
#endif
    setState(CalibrationState::WaitReady);
}
void CalibrationFSM::runStateWaitReady() {
    analogWrite(LED_PIN, 0); //TODO isto aqui corre muitas vezes :/

    if (numNodesReady == numTotalNodes){ //every node is ready
        numNodesReady = 0; //resets the 
#ifdef CALIB_PRINTS
        Serial.println("[CalibFSM] All nodes ready!");
#endif

        if (nodeLedOn == -1 || luxReadNum == 2)
        {
            nodeLedOn++;//next node to turn on the led
            luxReadNum = 0; //Reset reading number
        }

        if(nodeLedOn >= (int)numTotalNodes)
        {
#ifdef CALIB_PRINTS
            Serial.println("DONE calibrating!");
#endif
            done = true;
        }
        else if (nodesList[nodeLedOn] == nodeId)
        {
            //If it's this node's turn to turn on LED, broadcast that it turned it on,
            //  and then wait for the transient of the measure
            analogWrite(LED_PIN, pwm[luxReadNum]);
	        communication.sendCalibLedOn(); //sends message that indicates that the led is on
#ifdef CALIB_PRINTS
            Serial.println("[CalibFSM] State = WaitTransient");
#endif
            setState(CalibrationState::WaitTransient);
        }
        else
        {
            //If it's some other node's turn to turn on LED, go to state that waits for it to turn it on
#ifdef CALIB_PRINTS
            Serial.println("[CalibFSM] State = WaitLedOn");
#endif
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
#ifdef CALIB_PRINTS
        Serial.println("[CalibFSM] State = WaitTransient");
#endif
        setState(CalibrationState::WaitTransient);
    }
}
void CalibrationFSM::runStateWaitTrasient(unsigned long timeSinceLastTransition) {
    //After some time has passed, the measure should be stable
    if(timeSinceLastTransition>1000000) { //falta o tau
        //Measure 
        luxReads[luxReadNum]= luminaire.voltageToLux(luminaire.getVoltage());
#ifdef CALIB_PRINTS
        Serial.print("luxReads[");
        Serial.print(luxReadNum);
        Serial.print("] = ");
        Serial.println(luxReads[luxReadNum]);
#endif
        luxReadNum++;//increments the number of Luxreads

        if (luxReadNum == 2)
        {
            Serial.println(luxReads[1]-luxReads[0]);
            Serial.println(pwm[1]-pwm[0]);
            //setGain(nodeId, nodeLedOn, );
            float gain = (float)(luxReads[1]-luxReads[0]) / (pwm[1]-pwm[0]);

            // Saves it on the gain matrix. (nodeLedOn+1 is because nodeLedOn counts from 0 to numTotalNodes-1, but 0 is for the residual luminance)
            //gainMatrix[nodeIndexOnGainMatrix[nodeId]][nodeLedOn+1] = gain;
            setGain(nodeIndexOnGainMatrix[nodeId], nodeLedOn+1, gain);
            communication.sendCalibReady(gain);
        }
        else
        {
            communication.sendCalibReady(-1);
        }

        incrementNodesReady();
#ifdef CALIB_PRINTS
        Serial.println("[CalibFSM] State = WaitReady");
#endif
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
#ifdef CALIB_PRINTS
    Serial.print("numNodesReady = ");
    Serial.println(calibrationFSM.numNodesReady);
#endif
}

void CalibrationFSM::receivedGain(uint8_t sender, float gain) {
    //nodeLedOn + 1  -> when it is reading the residual, nodeLedOn = -1, so it writed on j=0
    calibrationFSM.setGain(nodeIndexOnGainMatrix[sender], nodeLedOn + 1, gain);
}


void CalibrationFSM::freeGainMatrix() {
    for(int i = 0; i < gainMatrixSize; ++i) {
        delete gainMatrix[i];
    }
    gainMatrixSize = 0;
    delete gainMatrix;
}

void CalibrationFSM::allocateGainMatrix() {
    //Frees the previous gain matrix in case this isn't the first time running the calibration
    freeGainMatrix();

    gainMatrixSize = numTotalNodes+1;
#ifdef CALIB_PRINTS
    Serial.print("ALOCATED ");
    Serial.print(gainMatrixSize);
    Serial.print("X");
    Serial.println(gainMatrixSize);
#endif
    //Alocates the matrix with the gains
    gainMatrix = new float*[gainMatrixSize];
    for(uint8_t i = 0; i < gainMatrixSize; ++i) {
        gainMatrix[i] = new float[gainMatrixSize];
        for(uint8_t j = 0; j < gainMatrixSize; ++j)
            gainMatrix[i][j] = 0.0f;
    }
}

void CalibrationFSM::setGain(uint8_t i, uint8_t j, float gain) {
#ifdef CALIB_PRINTS
    Serial.print("gainMatrix[");
    Serial.print(i);
    Serial.print(", ");
    Serial.print(j);
    Serial.print("] = ");
    Serial.println(gain);
#endif
    gainMatrix[i][j] = gain;
}