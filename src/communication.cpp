#include "communication.h"

#include "glob.h"

/**
 * Creates a CAN frame ID that contains the message type, the destination and the sender
 */
inline canid_t canMessageId(uint8_t destination, uint8_t messageType) {
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
    mcp2515->setBitrate(CAN_125KBPS, MCP_16MHZ);
    mcp2515->setNormalMode();
}
MCP2515::ERROR Communication::writeFloat(uint32_t id, uint32_t val) {
    can_frame frame;
    frame.can_id = id;
    frame.can_dlc = 4;
    my_can_msg msg;
    msg.value = val; //pack data
    for( int i = 0; i < 4; i++ ) //prepare can message
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
    my_can_msg msg;
    if (frame->can_dlc != 0)
    {
        for (size_t i = 0; i < 4; i++)
        {
            msg.bytes[i] = frame->data[i];
        }
    }
    

    if (msgType == CAN_WAKEUP_BROADCAST)
    {
        Serial.print("Received CAN_WAKEUP_BROADCAST from node ");
        Serial.println(sender);
        
        //Register the sender node ID
        registerNewNode(sender);
    }
    else if(msgType == CAN_CALIB_READY){
        Serial.println("Other Node Ready");
        calibrationFSM.incrementNodesReady();
    }
    else if(msgType == CAN_CALIB_LED_ON){
        calibrationFSM.otherNodeLedOn=true; 
    }
    else if (msgType == CAN_CALIB_GAIN){
        //calibrationFSM.gainMatrix[nodeId][sender]=msg.value;
    }
    /*else if (msgType == CALIB_READY)
    {
        float valData;
        mainFSM.calibrationFSM.SetNodeReady(sender, valData);
    }*/
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
void Communication::sendCalibReady(float val){
    Serial.println("[Comm] Sending Calib_Ready");
    communication.writeFloat(canMessageId(0, CAN_CALIB_READY), val);
}
void Communication::sendCalibGain(float val){
    Serial.println("[Comm] Sending Calib_Gain");
    communication.writeFloat(canMessageId(0, CAN_CALIB_GAIN), val);
}

/*void Communication::sendResponseLuminaireData(Luminaire *luminaire)
{
    canMsg.can_id = canId;
    canMsg.can_dlc = 2;                               //num data bytes
    canMsg.data[0] = CAN_RESPONSE_GET_LUMINAIRE_DATA; //Message id
    canMsg.data[1] = luminaire->occupied;
    /*canMsg.data[2] = lux;
    canMsg.data[3] = pwm;*/

/*mcp2515->sendMessage(&canMsg);
}

void Communication::sendRequestLuminaireData(uint8_t destination)
{
    canMsg.can_id = canId;
    canMsg.can_dlc = 2;                              //num data bytes
    canMsg.data[0] = CAN_REQUEST_GET_LUMINAIRE_DATA; //Message id
    canMsg.data[1] = destination;
    mcp2515->sendMessage(&canMsg);
}*/