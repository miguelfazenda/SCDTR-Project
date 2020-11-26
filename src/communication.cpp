#include "communication.h"

#include "glob.h"

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
    Serial.print("Received can frame id=0x");
    Serial.println(frame->can_id, HEX);

    int msgType = frame->can_id & 0x000000FF; //GETs first 8 bits that have the type
    int sender = frame ->can_id & 0x03000000;
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
        int sender = frame->can_id & (0x06000000);

        Serial.print("Received CAN_WAKEUP_BROADCAST from node ");
        Serial.println(sender);

        
        nodesList[numTotalNodes] = sender;
        numTotalNodes++;
    }
    else if(msgType == CAN_CALIB_READY){
        calibrationFSM.numNodesReady++;
    }
    else if(msgType == CAN_CALIB_LED_ON){
        calibrationFSM.otherNodeLedOn=true; 
    }
    else if (msgType == CAN_CALIB_GAIN){
        calibrationFSM.gainMatrix[nodeId][sender]=msg.value;
    }
    /*else if (msgType == CALIB_READY)
    {
        float valData;
        mainFSM.calibrationFSM.SetNodeReady(sender, valData);
    }*/
}

void Communication::sendBroadcastWakeup()
{
    Serial.print("Sending CAN_WAKEUP_BROADCAST");
    sendingFrame.can_id = (0 << 27) | (nodeId << 25) | CAN_WAKEUP_BROADCAST;
    sendingFrame.can_dlc = 0;
    mcp2515->sendMessage(&sendingFrame);
}
void Communication::sendCalibLedOn()
{
    Serial.print("Sending Calib_Led_on");
    sendingFrame.can_id = (0 << 27) | (nodeId << 25) | CAN_CALIB_LED_ON;
    sendingFrame.can_dlc = 0;
    mcp2515->sendMessage(&sendingFrame);
}
void Communication::sendCalibReady(float val){
    Serial.print("Sending Calib_Ready");
    communication.writeFloat((0 << 27) | (nodeId << 25) | CAN_CALIB_READY, val);
}
void Communication::sendCalibGain(float val){
    Serial.print("Sending Calib_Gain");
    communication.writeFloat((0 << 27) | (nodeId << 25) | CAN_CALIB_GAIN, val);
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