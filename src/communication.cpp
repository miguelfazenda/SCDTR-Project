#include "communication.h"
#include "glob.h"

/**
 * Creates a CAN frame ID that contains the message type, the destination and the sender
 */
inline canid_t canMessageId(uint8_t destination, uint8_t messageType)
{
    return ((canid_t)destination << 16) | ((canid_t)nodeId << 8) | messageType | CAN_EFF_FLAG;
}

Communication::Communication()
{
}

void Communication::init(MCP2515 *mcp2515, can_frame_stream *cf_stream)
{
    this->mcp2515 = mcp2515;
    this->cf_stream = cf_stream;

    //Init CAN Bus
    mcp2515->reset();
    mcp2515->setBitrate(CAN_1000KBPS, MCP_16MHZ);

    //Sets the filter to only allow messages where the destination is: 0 (broadcast) and nodeId (This node)
    //Mask and filter for RXB0
    mcp2515->setFilterMask(MCP2515::MASK0, true, 0x00FF0000);
    mcp2515->setFilter(MCP2515::RXF0, true, 0);
    mcp2515->setFilter(MCP2515::RXF1, true, (canid_t)nodeId << 16);
    //Mask and filter for RXB1
    mcp2515->setFilterMask(MCP2515::MASK1, true, 0x00FF0000);
    mcp2515->setFilter(MCP2515::RXF2, true, 0);
    mcp2515->setFilter(MCP2515::RXF3, true, (canid_t)nodeId << 16);

    mcp2515->setNormalMode();
}
MCP2515::ERROR Communication::writeFloat(uint32_t id, uint32_t val)
{
    can_frame frame;
    frame.can_id = id;
    frame.can_dlc = 4;
    my_can_msg msg;
    msg.value = val;            //pack data
    for (int i = 0; i < 4; i++) //prepare can message
        frame.data[i] = msg.bytes[i];
    //send data
    return mcp2515->sendMessage(&frame);
}

/**
 *  Sends the sendingFrame, and prints an error if there was one
 */
MCP2515::ERROR Communication::sendFrame()
{
    MCP2515::ERROR error = mcp2515->sendMessage(&sendingFrame);
    if (error != MCP2515::ERROR_OK)
    {
        Serial.print(F("[ERROR] Error "));
        Serial.print(error);
        Serial.print(F(" sending can frame ID=0x"));
        Serial.println(sendingFrame.can_id, HEX);
    }
    return error;
}

void Communication::received(Luminaire *luminaire, can_frame *frame)
{
    /*Serial.print(F("[Comm] Received can frame id=0x"));
    Serial.println(frame->can_id, HEX);*/

    uint8_t msgType = frame->can_id & 0x000000FF; //GETs first 8 bits that have the type
    uint8_t sender = (frame->can_id & 0x0000FF00) >> 8;

    if (msgType == CAN_WAKEUP_BROADCAST)
    {
        /*Serial.print(F("Received CAN_WAKEUP_BROADCAST from node "));
        Serial.println(sender);*/

        //Register the sender node ID
        registerNewNode(sender);
    }
    else if (msgType == CAN_CALIB_READY)
    {
        Convertion convert;
        convert.valueBytes[0] = frame->data[0];
        convert.valueBytes[1] = frame->data[1];
        convert.valueBytes[2] = frame->data[2];
        convert.valueBytes[3] = frame->data[3];
        float gainValue = convert.valueFloat;
        
        if (gainValue >= 0)
        {
            //The ready message contains the gain calculated (if it is not time to send gain, it send a negative number)
            calibrationFSM.receivedGainOrResidual(sender, gainValue);
        }

        //Sets that another node is ready
        calibrationFSM.incrementNodesReady();
    }
    else if (msgType == CAN_CALIB_LED_ON)
    {
        calibrationFSM.otherNodeLedOn = true;
    }
    else if (msgType == CAN_IS_HUB_NODE)
    {
        /*Serial.print(F("Received CAN_IS_HUB_NODE from "));
        Serial.print(sender);*/

        //Changes the hubNode to be the one who sent the message
        hubNode = sender;
    }
    else if (msgType == CAN_FREQUENT_DATA)
    {
        //This is a hub node that received a frequent data packet from another node.
        // Send it on the serial interface
        /*SerialFrequentDataPacket frequentDataPacket(frame->data);
        frequentDataPacket.sendOnSerial();*/

        Serial.write(255);
        Serial.write('F');
        Serial.write(frame->data[0]);
        Serial.write(frame->data[1]);
        Serial.write(frame->data[2]);
        Serial.write(frame->data[3]);
        Serial.flush();

    }
    else if (msgType == CAN_NO_LONGER_IS_HUB_NODE)
    {
        /*Serial.print(F("Received CAN_NO_LONGER_IS_HUB_NODE from "));
        Serial.print(sender);*/

        //Changes the hubNode to be 0
        if (hubNode == sender)
            hubNode = 0;
    }
    else if (msgType == CAN_COMMANDS_REQUEST)
    {
        /*Serial.print(F("Received CAN_COMMANDS_REQUEST from "));
        Serial.print(sender);*/

        //Parses the command from the binary data on the can frame
        Command command(frame->data);

        //Executes the command and gets the return value
        uint32_t value = serialComm.executeCommand(command);
        //If this is a message request sent by the hub, respond
        sendCommandResponse(sender, value);
    }
    else if (msgType == CAN_COMMANDS_RESPONSE)
    {
        //If this is a message response sent to the hub, print it on the serial
        /*Serial.print(F("Received CAN_COMMANDS_RESPONSE from "));
        Serial.println(sender);*/

        Convertion convertion;
        convertion.valueBytes[0] = frame->data[0];
        convertion.valueBytes[1] = frame->data[1];
        convertion.valueBytes[2] = frame->data[2];
        convertion.valueBytes[3] = frame->data[3];

        //Sends the response on the serial port
        serialComm.receivedCommandResponseFromCAN(sender, convertion.value32b);
    }
    else if (msgType == CAN_CONSENSUS)
    {
        uint8_t msgI = frame->data[6];
        Serial.println(msgI);
        //Serial.println(F("--- Msg Received ---"));
        for (uint8_t i = 0; i < 3; i++)
        {
            uint8_t posInVector = msgI * 3 + i;
            if (posInVector <= numTotalNodes) 
            {
                //Converts the received integer value back to a float
                int16_t aux = (uint16_t)(frame->data[i * 2]) << 8 |
                              (uint16_t)(frame->data[i * 2 + 1]);
                float value = aux * 300.0 / 32767;
                //Serial.println(value);
                //Writes on the consensus vector
                consensus.receivedDutyCycle[posInVector] += value;
            }
        }
        //Serial.println(F("--- Msg End ---"));
        consensus.numberOfMsgReceived += 1;
    }
    else if (msgType == CAN_DO_CONSENSUS)
    {
        consensus.init();
    }
}

void Communication::sendCommandResponse(uint8_t sender, uint32_t value)
{
    /*Serial.print(F("[Comm] Sending CAN_COMMANDS_RESPONSE to "));
    Serial.println(sender);*/
    sendingFrame.can_id = canMessageId(sender, CAN_COMMANDS_RESPONSE);

    //Converts from 32bit value to 4 bytes
    Convertion sendingValue;
    sendingValue.value32b = value;

    //Sends the returned value from the command execution
    sendingFrame.data[0] = sendingValue.valueBytes[0];
    sendingFrame.data[1] = sendingValue.valueBytes[1];
    sendingFrame.data[2] = sendingValue.valueBytes[2];
    sendingFrame.data[3] = sendingValue.valueBytes[3];
    sendingFrame.can_dlc = 4;
    sendFrame();
}

void Communication::sendCommandRequest(uint8_t destination, Command &command)
{
    /*Serial.print(F("[Comm] Sending CAN_COMMANDS_REQUEST to "));
    Serial.println(destination);*/

    sendingFrame.can_id = canMessageId(destination, CAN_COMMANDS_REQUEST);

    size_t numBytes = command.toByteArray((char *)sendingFrame.data);
    sendingFrame.can_dlc = numBytes;

    sendFrame();
}

void Communication::sendBroadcastWakeup()
{
    //Serial.println(F("[Comm] Sending CAN_WAKEUP_BROADCAST"));
    sendingFrame.can_id = canMessageId(0, CAN_WAKEUP_BROADCAST);
    sendingFrame.can_dlc = 0;
    sendFrame();
}
void Communication::sendCalibLedOn()
{
    //Serial.println(F("[Comm] Sending Calib_Led_on"));
    sendingFrame.can_id = canMessageId(0, CAN_CALIB_LED_ON);
    sendingFrame.can_dlc = 0;
    sendFrame();
}

void Communication::sendCalibReady(float val)
{
    /*Serial.print(F("[Comm] Sending Calib_Ready with gain="));
    Serial.println(val);*/
    sendingFrame.can_id = canMessageId(0, CAN_CALIB_READY);
    sendingFrame.can_dlc = 4;

    unsigned char *p = (unsigned char *)(&val);
    sendingFrame.data[0] = p[0];
    sendingFrame.data[1] = p[1];
    sendingFrame.data[2] = p[2];
    sendingFrame.data[3] = p[3];

    sendFrame();
}

/**
 * Sends a Frequent Data Packet to the hub node
 */
void Communication::sendFrequentDataToHub(SerialFrequentDataPacket frequentDataPacket)
{
    /*Serial.print(F("[Comm] Sending CAN_FREQUENT_DATA to hub id:"));
    Serial.println(hubNode);*/
    //communication.writeFloat(canMessageId(0, CAN_CALIB_READY), val);
    sendingFrame.can_id = canMessageId(hubNode, CAN_FREQUENT_DATA);

    size_t numBytes = frequentDataPacket.toByteArray(sendingFrame.data);
    sendingFrame.can_dlc = numBytes;

    sendFrame();
}

void Communication::sendDoConsensus()
{
    //Serial.println(F("[Comm] Sending CAN_DO_CONSENSUS"));
    sendingFrame.can_id = canMessageId(0, CAN_DO_CONSENSUS);
    sendingFrame.can_dlc = 0;
    sendFrame();
}

void Communication::sendConsensusDutyCycle(float *val)
{
    //Each message can send 3 values of the dutyCycle vestor
    uint8_t numMessages = (numTotalNodes - 1) / 3 + 1;

    sendingFrame.can_id = canMessageId(0, CAN_CONSENSUS);
    sendingFrame.can_dlc = 7;

    for (uint8_t msgI = 0; msgI < numMessages; msgI++)
    {
        for (uint8_t i = 0; i < 3; i++) //prepare can message
        {
            if (msgI * 3 + i >= numTotalNodes)
                break;
            //Converts float to -32767 to 32767 encoding
            int16_t aux = round(val[msgI * 3 + i] * 32767 / 300.0);

            //Converts aux in 2 bytes
            sendingFrame.data[i * 2] = (aux >> 8) & 0xFF;
            sendingFrame.data[i * 2 + 1] = aux & 0xFF;
        }

        sendingFrame.data[6] = msgI;
        /*Serial.print(F("sending msgI = "));
        Serial.println(msgI);*/
        //send data
        sendFrame();
    }
}

/**
 * Send a message saying this node is now the HUB node
 */
void Communication::sendBroadcastIsHubNode()
{
    sendingFrame.can_id = canMessageId(0, CAN_IS_HUB_NODE);
    sendingFrame.can_dlc = 0;
    sendFrame();
}

void Communication::sendBroadcastNoLongerIsHubNode()
{
    sendingFrame.can_id = canMessageId(0, CAN_NO_LONGER_IS_HUB_NODE);
    sendingFrame.can_dlc = 0;
    sendFrame();
}