#include <Arduino.h>
#include <EEPROM.h>
#include <mcp2515.h>
#include "util.h"
#include "communication.h"
#include "luminaire.h"

#define EEPROM_ADDR_CANID 0x00 //Address of where to get the can id of this arduino on the EEPROM

//The CAN id of this arduino (uint8_t is one byte)
uint8_t canId;

Communication communication;
MCP2515 mcp2515(10);

//If this is the hub node that connects to the computer
bool hubNode = false;

Luminaire luminaire;

void readSerial();

void setup()
{
	// initialize serial communications at 1Mbps
	Serial.begin(1000000);

	// Change PWM frequency on PIN 9
	TCCR1B = TCCR1B & B11111000 | B00000001;

	//Read from EEPROM the CAN id
	canId = EEPROM.read(EEPROM_ADDR_CANID);
	//The hub arduino is the one that has canId 0
	hubNode = (canId == 0x00);

	communication.init(canId, &mcp2515);
	luminaire.init(false);
}

void loop()
{
	if (hubNode)
	{
		readSerial();
	}

	communication.loop(&luminaire);
	luminaire.loop();
}



/**
 * Reads if there was an input on the serial port, if so, change the lux reference
 */
void readSerial()
{
	while (Serial.available())
	{
		int data = Serial.parseInt();
		if(data == 0 || data == 1) {
			luminaire.setOccupied(data == 1);
		}
		//setLuxRef(data);
	}
}