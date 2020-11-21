#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <SPI.h>
#include <mcp2515.h>
#include "luminaire.h"

#define CAN_REQUEST_GET_LUMINAIRE_DATA 0x01
#define CAN_RESPONSE_GET_LUMINAIRE_DATA 0x02

class Communication
{
private:
    struct can_frame canMsg;
    MCP2515* mcp2515;
    uint8_t canId;

    /**
     * CAN_RESPONSE_GET_LUMINAIRE_DATA
     * Broacasts the luminaire's status (PWM, occupation, etc.)
     */
    void sendResponseLuminaireData(Luminaire* luminaire);
public:
    Communication(/* args */);
    void init(int canId, MCP2515* mcp2515);
    void loop(Luminaire* luminaire);



    /**
     * THIS FUNCTIONS ARE FOR THE HUB NODE
     */
    /**
     * Sends a request to a certain node, and the node will broadcast it's data (sendResponseLuminaireData)
     * @param destination the node
     */
    void sendRequestLuminaireData(uint8_t destination);
};

#endif