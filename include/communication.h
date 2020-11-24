#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <SPI.h>
#include <mcp2515.h>
#include "luminaire.h"
#include "can_frame_stream.h"

#define CAN_WAKEUP_BROADCAST 1

class Communication
{
private:
    MCP2515* mcp2515;
    uint8_t nodeId;
    can_frame_stream* cf_stream;
    
    /**
     * CAN_RESPONSE_GET_LUMINAIRE_DATA
     * Broacasts the luminaire's status (PWM, occupation, etc.)
     */
    //void sendResponseLuminaireData(Luminaire* luminaire);
public:
    Communication();
    void init(int nodeId, MCP2515* mcp2515, can_frame_stream* cf_stream);
    void received(Luminaire *luminaire, can_frame *frame);

    
    int putFrameInBuffer(can_frame &);
    int getFrameInBuffer(can_frame &);

    /**
     * THIS FUNCTIONS ARE FOR THE HUB NODE
     */
    /**
     * Sends a request to a certain node, and the node will broadcast it's data (sendResponseLuminaireData)
     * @param destination the node
     */
    //void sendRequestLuminaireData(uint8_t destination);

    void sendBroadcastWakeup();
};

#endif