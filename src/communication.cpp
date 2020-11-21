#include "communication.h"

Communication::Communication(/* args */)
{
}

void Communication::init(int canId, MCP2515* mcp2515)
{
    this->canId = canId;
    this->mcp2515 = mcp2515;

    //Init CAN Bus
    mcp2515->reset();
    mcp2515->setBitrate(CAN_125KBPS);
    mcp2515->setNormalMode();
}

void Communication::loop(Luminaire* luminaire)
{
    //Read incomming can bus messages
    if (mcp2515->readMessage(&canMsg) == MCP2515::ERROR_OK)
    {
        //canid_t sender = canMsg.can_id;
        uint8_t msgId = canMsg.data[0];
        uint8_t nodeId = canMsg.data[1];

        if(nodeId == canId) {
            //If it is respective to this node
            if (msgId == CAN_REQUEST_GET_LUMINAIRE_DATA)
            {
                //Sends response back to sender
                sendResponseLuminaireData(luminaire);
            }
        }
        
    }
}

void Communication::sendResponseLuminaireData(Luminaire* luminaire) {
    canMsg.can_id = canId;
    canMsg.can_dlc = 2;                              //num data bytes
    canMsg.data[0] = CAN_RESPONSE_GET_LUMINAIRE_DATA; //Message id
    canMsg.data[1] = luminaire->occupied;
    /*canMsg.data[2] = lux;
    canMsg.data[3] = pwm;*/

    mcp2515->sendMessage(&canMsg);
}

void Communication::sendRequestLuminaireData(uint8_t destination)
{
    canMsg.can_id = canId;
    canMsg.can_dlc = 2;                              //num data bytes
    canMsg.data[0] = CAN_REQUEST_GET_LUMINAIRE_DATA; //Message id
    canMsg.data[1] = destination;
    mcp2515->sendMessage(&canMsg);
}