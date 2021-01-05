#include "calibrationFSM.h"
#include <Arduino.h>
#include "glob.h"

CalibrationFSM::CalibrationFSM()
{
    state = CalibrationState::CalibInit;
    numNodesReady = 0;
    luxReadNum = 0; //number of the Lux read
    //PWM values
    pwm[0] = 20;
    pwm[1] = 240;
    luxReads[0] = 0;
    luxReads[1] = 0;
    otherNodeLedOn = false;

    done = false;

    //Zero gain matrix
    /*for (uint8_t i = 0; i < MAX_NUM_NODES; ++i)
    {
        for (uint8_t j = 0; j < MAX_NUM_NODES; ++j) {
            gainMatrix[i][j] = 0.0f;
        }
    }*/

    //TODO Vetor residual a 0
}

CalibrationFSM::~CalibrationFSM()
{
}

void CalibrationFSM::setState(CalibrationState state)
{
    this->state = state;
    lastTransitionTime = micros();
}

/**
 * Reads the residual luminance, saves it on the gainMatrix.
 *  Informs other nodes that it is ready(sending the reading alongside)
 *  Next State -> WaitReady (waits for other nodes be ready)
 */
void CalibrationFSM::runStateInit()
{
    //Reads residual luminance and saves it on the residualArray
    float residualReading = luminaire.voltageToLux(luminaire.getVoltage());
    Serial.println(residualReading);
    
    residualArray[nodeIndexOnGainMatrix[nodeId]] = residualReading;

    //Sends that this node is ready for the next step
    communication.sendCalibReady(residualReading); // Writes in the message that has the residual reading
    incrementNodesReady();

    nodeLedOn = -1; //Index from the node ID vector (-1 none of the leds are on)

#ifdef CALIB_PRINTS
    Serial.println("[CalibFSM] State = WaitReady");
#endif
    setState(CalibrationState::WaitReady);
}


/**
 *  Waits until all nodes inform they are ready
 *   When they are, either increment the reading number (1st or 2nd reading for a certain LED on) or
 *   increments which node should turn on the LED.
 *   If it's this node's turn, turn on LED, inform others that it turned it on, and
 *      Next State -> WaitTrasient (waits for LDR transient)
 *   Else if it is another node's turn, go to the state where it waits for the message it turned it on
 *      Next State -> WaitLedOn (waits for other nodes to turn on LED)
 */
void CalibrationFSM::runStateWaitReady()
{
    analogWrite(LED_PIN, 0); //TODO isto aqui corre muitas vezes :/

    if (numNodesReady == numTotalNodes)
    {                      //every node is ready
        numNodesReady = 0; //resets the
#ifdef CALIB_PRINTS
        Serial.println("[CalibFSM] All nodes ready!");
#endif

        if (nodeLedOn == -1 || luxReadNum == 2)
        {
            nodeLedOn++;    //next node to turn on the led
            luxReadNum = 0; //Reset reading number
        }

        if (nodeLedOn >= (int)numTotalNodes)
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

/**
 * This state waits for the node who is in charge of turning on the LED to send a message informing of that.
 *  When it is received otherNodeLedOn == true
 *  Next State -> WaitTrasient (waits for LDR transient)
 */
void CalibrationFSM::runStateWaitLedOn()
{
    //otherNodeLedOn is an Event.
    // When communications receives that a node turned on LED, otherNodeLedOn == true
    if (otherNodeLedOn)
    {
        otherNodeLedOn = false; //Reset event bool
#ifdef CALIB_PRINTS
        Serial.println("[CalibFSM] State = WaitTransient");
#endif
        //Go to state that waits for the transient of the measure
        setState(CalibrationState::WaitTransient);
    }
}

/**
 * In this state we wait for the transient of the LDR, and then read the luminance and store it on luxReads.
 *  If it is the second reading calculates the gain. Then it sends to the other nodes it is ready (and the calculated gain).
 */
void CalibrationFSM::runStateWaitTrasient(unsigned long timeSinceLastTransition)
{
    //After some time has passed, the measure should be stable
    if (timeSinceLastTransition > 1000000) //TODO falta o tau
    {
        //Measure
        luxReads[luxReadNum] = luminaire.voltageToLux(luminaire.getVoltage()) - residualArray[nodeId];
#ifdef CALIB_PRINTS
        Serial.print(F("luxReads["));
        Serial.print(luxReadNum);
        Serial.print("] = ");
        Serial.println(luxReads[luxReadNum]);
#endif
        luxReadNum++; //increments the number of Luxreads

        float gain = -1;

        //If it is the second reading, calculates gain, and sets it on the gain matrix
        if (luxReadNum == 2)
        {
            gain = (float)(luxReads[1] - luxReads[0]) / (pwm[1] - pwm[0]);

            // Saves it on the gain matrix.
            setGain(nodeIndexOnGainMatrix[nodeId], nodeLedOn, gain);
        }

        // Inform other nodes that this node is ready. Along with this message, send the calculated gain (-1 if this isn't the second reading)
        communication.sendCalibReady(gain);
        incrementNodesReady();
#ifdef CALIB_PRINTS
        Serial.println("[CalibFSM] State = WaitReady");
#endif
        //Changes to the state where it waits for every node to be ready
        setState(CalibrationState::WaitReady);
    }
}

/**
 * The loop function that calls each of the run functions depening on the state the machine is on.
 */
void CalibrationFSM::loop()
{
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

void CalibrationFSM::incrementNodesReady()
{
    numNodesReady++;
#ifdef CALIB_PRINTS
    Serial.print(F("numNodesReady = "));
    Serial.println(numNodesReady);
#endif
}

/**
 * This is called by Communication when it receives a gain from another node
 *  It sets the gain for that sender, taking into account that the node who turned on the led is nodeLedOn 
 * @param value the gain or residual reading
 */
void CalibrationFSM::receivedGainOrResidual(uint8_t sender, float value)
{
    if(nodeLedOn == -1)
    {
        //No led is turned on yet. This means that what was received was the residual reading the sender read
        residualArray[nodeIndexOnGainMatrix[sender]] = value;
    }
    else
    {
        setGain(nodeIndexOnGainMatrix[sender], nodeLedOn, value);
    }
}

/**
 * Sets a value on the gain matrix
 */
void CalibrationFSM::setGain(uint8_t i, uint8_t j, float gain)
{
#ifdef CALIB_PRINTS
    Serial.print(F("gainMatrix["));
    Serial.print(i);
    Serial.print(", ");
    Serial.print(j);
    Serial.print("] = ");
    Serial.println(gain);
#endif
    gainMatrix[i][j] = gain;
}

void CalibrationFSM::printResidualAndGainMatrix() {
    Serial.println("Residual:");
    for(int j = 0; j < numTotalNodes; ++j) {
        Serial.print("\t");
        Serial.print(residualArray[j], 8);
    }

    Serial.println("\nGains:");
    for(int i = 0; i < numTotalNodes; ++i) {
        Serial.println("\n");
        for(int j = 0; j < numTotalNodes; ++j) {
            Serial.print("\t\t");
            Serial.print(gainMatrix[i][j], 8);
        }
    }
}

void CalibrationFSM::resetCalib()
{
    state = CalibrationState::CalibInit;
    numNodesReady = 0;
    luxReadNum = 0; //number of the Lux read
    //PWM values
    pwm[0] = 20;
    pwm[1] = 240;
    luxReads[0] = 0;
    luxReads[1] = 0;
    otherNodeLedOn = false;
    done = false;
    for (uint8_t i = 0; i < numTotalNodes; i++)
    {
        for (uint8_t j = 0; j < numTotalNodes; j++)
        {
            gainMatrix[i][j] = 0;
        }
        residualArray[i] = 0;
    }
    
}