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
    else if (msgType == CAN_IS_HUB_NODE)
    {
        Serial.print("Received CAN_IS_HUB_NODE from ");
        Serial.print(sender);

        //Changes the hubNode to be the one who sent the message
        hubNode = sender;
    }
    else if(msgType == CAN_FREQUENT_DATA)
    {
        //This is a hub node that received a frequent data packet from another node.
        // Send it on the serial interface
        SerialFrequentDataPacket frequentDataPacket(frame->data);
        frequentDataPacket.sendOnSerial();
    }
    else if (msgType == CAN_NO_LONGER_IS_HUB_NODE)
    {
        Serial.print("Received CAN_NO_LONGER_IS_HUB_NODE from ");
        Serial.print(sender);

        //Changes the hubNode to be 0
        if(hubNode == sender)
            hubNode = 0;
    }
    else if(msgType == CAN_COMMANDS_REQUEST)
    {
        Serial.print("Received CAN_COMMANDS_REQUEST from ");
        Serial.print(sender);

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
        Serial.print("Received CAN_COMMANDS_RESPONSE from ");
        Serial.println(sender);

        Convertion value;
        value.valueBytes[0] = sendingFrame.data[0];
        value.valueBytes[1] = sendingFrame.data[1];
        value.valueBytes[2] = sendingFrame.data[2];
        value.valueBytes[3] = sendingFrame.data[3];

        //Sends the response on the serial port
        serialComm.receivedCommandResponseFromCAN(sender, value.value32b);
    }
}

void Communication::sendCommandResponse(uint8_t sender, uint32_t value) {
    Serial.print("[Comm] Sending CAN_COMMANDS_REQUEST to ");
    Serial.println(sender);
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
    mcp2515->sendMessage(&sendingFrame);
}

void Communication::sendCommandRequest(uint8_t destination, Command& command) {
    Serial.print("[Comm] Sending CAN_COMMANDS_REQUEST to ");
    Serial.println(destination);

    sendingFrame.can_id = canMessageId(destination, CAN_COMMANDS_REQUEST);
    
    size_t numBytes = command.toByteArray((char*)sendingFrame.data);
    sendingFrame.can_dlc = numBytes;

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

/**
 * Sends a Frequent Data Packet to the hub node
 */
void Communication::sendFrequentDataToHub(SerialFrequentDataPacket frequentDataPacket)
{
    Serial.print("[Comm] Sending CAN_FREQUENT_DATA to hub id:");
    Serial.println(hubNode);
    //communication.writeFloat(canMessageId(0, CAN_CALIB_READY), val);
    sendingFrame.can_id = canMessageId(hubNode, CAN_FREQUENT_DATA);

    size_t numBytes = frequentDataPacket.toByteArray(sendingFrame.data);
    sendingFrame.can_dlc = numBytes;

    mcp2515->sendMessage(&sendingFrame);
}


/**
 * Send a message saying this node is now the HUB node
 */
void Communication::sendBroadcastIsHubNode()
{
    sendingFrame.can_id = canMessageId(0, CAN_IS_HUB_NODE);
    sendingFrame.can_dlc = 0;
    mcp2515->sendMessage(&sendingFrame);
}

void Communication::sendBroadcastNoLongerIsHubNode()
{
    sendingFrame.can_id = canMessageId(0, CAN_NO_LONGER_IS_HUB_NODE);
    sendingFrame.can_dlc = 0;
    mcp2515->sendMessage(&sendingFrame);
}