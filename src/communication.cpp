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

void Communication::received(Luminaire *luminaire, can_frame *frame)
{
    Serial.print("[Comm] Received can frame id=0x");
    Serial.println(frame->can_id, HEX);

    uint8_t msgType = frame->can_id & 0x000000FF; //GETs first 8 bits that have the type
    uint8_t sender = (frame->can_id & 0x0000FF00) >> 8;
    /*my_can_msg msg;
    if (frame->can_dlc != 0)
    {
        for (size_t i = 0; i < 4; i++)
        {
            msg.bytes[i] = frame->data[i];
        }
    }*/

    if (msgType == CAN_WAKEUP_BROADCAST)
    {
        Serial.print("Received CAN_WAKEUP_BROADCAST from node ");
        Serial.println(sender);

        //Register the sender node ID
        registerNewNode(sender);
    }
    else if (msgType == CAN_CALIB_READY)
    {
        float gainValue = *((float *)frame->data);
        if (gainValue >= 0)
        {
            //The ready message contains the gain calculated (if it is not time to send gain, it send a negative number)
            Serial.print(sender);
            Serial.print(" sent gain ");
            Serial.println(gainValue);

            calibrationFSM.receivedGainOrResidual(sender, gainValue);
        }

        //Sets that another node is ready
        Serial.print("Node ");
        Serial.print(sender);
        Serial.println("is ready!");
        calibrationFSM.incrementNodesReady();
    }
    else if (msgType == CAN_CALIB_LED_ON)
    {
        calibrationFSM.otherNodeLedOn = true;
    }
    else if (msgType == CAN_CALIB_GAIN)
    {
        //calibrationFSM.gainMatrix[nodeId][sender]=msg.value;
    }
    else if(msgType == CAN_HUB_GET_VALUE_REQUEST)
    {
        Serial.print("Received get value request from ");
        Serial.print(sender);
        Serial.print(" ");
        Serial.println((char)frame->data[0]);
        //If this is a message request sent by the hub, respond
        sendResponseGetHubValue(sender, frame->data);
    }
    else if (msgType == CAN_HUB_GET_VALUE_RESPONSE)
    {
        //If this is a message response sent to the hub, print it on the serial
        Serial.print("Received get value response from ");
        Serial.println(sender);

        //TODO meter isto dentro do hub maybe?
        //Print do do output deste comando
        Serial.print((char)frame->data[0]);
        Serial.print(' ');
        Serial.print(sender);
        Serial.print(' ');
        Serial.print(frame->data[1]);
    }
}

void Communication::sendResponseGetHubValue(uint8_t sender, uint8_t* data) {
    sendingFrame.can_id = canMessageId(sender, CAN_HUB_GET_VALUE_RESPONSE);

    Serial.println(data[0]);
    Serial.println((char)data[0]);

    char valueType = data[0];

    sendingFrame.can_dlc = 2;

    sendingFrame.data[0] = valueType;
    sendingFrame.data[1] = 0;

    switch(valueType) {
    case 'I':
        //Responds with the the iluminance read by the LDR
        sendingFrame.data[1] = 255;
        break;
    case 'd':
        //Responds with the PWM of the luminaire
        sendingFrame.data[1] = 255;
        break;
    }
    
    mcp2515->sendMessage(&sendingFrame);
}

void Communication::sendRequestHubGetValue(uint8_t destination, char valueType) {
    Serial.print("[Comm] Sending CAN_HUB_GET_VALUE_REQUEST to ");
    Serial.println(destination);
    sendingFrame.can_id = canMessageId(destination, CAN_HUB_GET_VALUE_REQUEST);
    sendingFrame.can_dlc = 1;
    sendingFrame.data[0] = valueType;
    Serial.println(sendingFrame.data[0]);
    mcp2515->sendMessage(&sendingFrame);
}

void Communication::sendBroadcastWakeup()
{
    Serial.println("[Comm] Sending CAN_WAKEUP_BROADCAST");
    sendingFrame.can_id = canMessageId(0, CAN_WAKEUP_BROADCAST);
    sendingFrame.can_dlc = 0;
    mcp2515->sendMessage(&sendingFrame);
}
void Communication::sendCalibLedOn()
{
    Serial.println("[Comm] Sending Calib_Led_on");
    sendingFrame.can_id = canMessageId(0, CAN_CALIB_LED_ON);
    sendingFrame.can_dlc = 0;
    mcp2515->sendMessage(&sendingFrame);
}
void Communication::sendCalibReady(float val)
{
    Serial.print("[Comm] Sending Calib_Ready with gain=");
    Serial.println(val);
    //communication.writeFloat(canMessageId(0, CAN_CALIB_READY), val);
    sendingFrame.can_id = canMessageId(0, CAN_CALIB_READY);
    sendingFrame.can_dlc = 4;

    unsigned char *p = (unsigned char *)(&val);
    sendingFrame.data[0] = p[0];
    sendingFrame.data[1] = p[1];
    sendingFrame.data[2] = p[2];
    sendingFrame.data[3] = p[3];

    mcp2515->sendMessage(&sendingFrame);
}
void Communication::sendCalibGain(float val)
{
    Serial.println("[Comm] Sending Calib_Gain");
    communication.writeFloat(canMessageId(0, CAN_CALIB_GAIN), val);
}

/*void Communication::sendResponseLuminaireData(Luminaire *luminaire)
{
    canMsg.can_id = canId;
    canMsg.can_dlc = 2;                               //num data bytes
    canMsg.data[0] = CAN_RESPONSE_GET_LUMINAIRE_DATA; //Message id
    canMsg.data[1] = luminaire->occupied;
    canMsg.data[2] = lux;
    canMsg.data[3] = pwm;

mcp2515->sendMessage(&canMsg);
}

void Communication::sendRequestLuminaireData(uint8_t destination)
{
    canMsg.can_id = canId;
    canMsg.can_dlc = 2;                              //num data bytes
    canMsg.data[0] = CAN_REQUEST_GET_LUMINAIRE_DATA; //Message id
    canMsg.data[1] = destination;
    mcp2515->sendMessage(&canMsg);
}*/