#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <SPI.h>
#include <mcp2515.h>
#include "luminaire.h"
#include "can_frame_stream.h"
#include "serialComm.h"

#define CAN_WAKEUP_BROADCAST 1
#define CAN_CALIB_READY 2
#define CAN_CALIB_LED_ON 3
#define CAN_CALIB_GAIN 4
#define CAN_DO_CONSENSUS 5
#define CAN_CONSENSUS 6

#define CAN_IS_HUB_NODE 10
#define CAN_NO_LONGER_IS_HUB_NODE 11
#define CAN_FREQUENT_DATA 12

#define CAN_COMMANDS_REQUEST 8
#define CAN_COMMANDS_RESPONSE 9




class Communication
{
private:
    MCP2515* mcp2515;
    can_frame_stream* cf_stream;
    struct can_frame sendingFrame;
    
    MCP2515::ERROR sendFrame();
    /**
     * CAN_RESPONSE_GET_LUMINAIRE_DATA
     * Broacasts the luminaire's status (PWM, occupation, etc.)
     */
    //void sendResponseLuminaireData(Luminaire* luminaire);
public:
    Communication();
    void init(MCP2515* mcp2515, can_frame_stream* cf_stream);
    void received(Luminaire *luminaire, can_frame *frame);
    MCP2515::ERROR writeFloat(uint32_t id, uint32_t val);

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
    void sendCalibLedOn();
    void sendCalibReady(float val);

    void Communication::sendDoConsensus();
    MCP2515::ERROR sendConsensusDutyCycle(float* val);
    void sendBroadcastIsHubNode();
    void sendBroadcastNoLongerIsHubNode();

    void sendFrequentDataToHub(SerialFrequentDataPacket frequentDataPacket);

    void sendCommandResponse(uint8_t sender, uint32_t value);
    void sendCommandRequest(uint8_t destination, Command& command);
};
union my_can_msg {
    float value;
    unsigned char bytes[4];
};

#endif