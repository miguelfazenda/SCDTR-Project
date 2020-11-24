#include "communication.h"

Communication::Communication()
{
}

void Communication::init(int nodeId, MCP2515 *mcp2515, can_frame_stream* cf_stream)
{
    this->nodeId = nodeId;
    this->mcp2515 = mcp2515;
    this->cf_stream = cf_stream;

    //Init CAN Bus
    mcp2515->reset();
    mcp2515->setBitrate(CAN_125KBPS);
    mcp2515->setNormalMode();
}

void Communication::received(Luminaire *luminaire, can_frame *frame)
{
    int msgType = frame->can_id & 0x000000FF; //GETs first 8 bits that have the type
    
    if(msgType == CAN_WAKEUP_BROADCAST) {
        int sender = frame->can_id & (0x06000000);

        Serial.print("Received CAN_WAKEUP_BROADCAST from node ");
        Serial.println(sender);
    }
}

void Communication::sendBroadcastWakeup() {
    struct can_frame canMsg;
    canMsg.can_id = (0 << 27) | (nodeId << 25) | CAN_WAKEUP_BROADCAST;
    canMsg.can_dlc = 0;
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