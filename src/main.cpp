#include <Arduino.h>
#include <EEPROM.h>
#include <mcp2515.h>
#include "util.h"
#include "communication.h"
#include "luminaire.h"
#include "can_frame_stream.h"

#define EEPROM_ADDR_CANID 0x00 //Address of where to get the can id of this arduino on the EEPROM

//The CAN id of this arduino (uint8_t is one byte)
uint8_t nodeId;

Communication communication;
MCP2515 mcp2515(10);

//If this is the hub node that connects to the computer
bool hubNode = false;

Luminaire luminaire;

void readSerial();
void irqHandler();

volatile bool interrupt = false;
volatile bool mcp2515_overflow = false;
volatile bool arduino_overflow = false;

/*volatile*/ can_frame_stream cf_stream;

void setup()
{
	// initialize serial communications at 1Mbps
	Serial.begin(1000000);

	// Change PWM frequency on PIN 9
	TCCR1B = TCCR1B & B11111000 | B00000001;

	//Read from EEPROM the CAN id
	nodeId = EEPROM.read(EEPROM_ADDR_CANID);
	//The hub arduino is the one that has canId 0
	hubNode = (nodeId == 0x00);

	/**
	 * INIT CAN BUS
	 */
	attachInterrupt(0, irqHandler, FALLING);
	//use interrupt at pin 2
	//Must tell SPI lib that ISR for interrupt vector zero will be using SPI
	SPI.usingInterrupt(0);
	communication.init(nodeId, &mcp2515, &cf_stream);

	luminaire.init(false);

	//After 2 seconds, sends a message through the CAN bus for the other nodes to register it's presence
	delay(2000);
	communication.sendBroadcastWakeup();
}

void irqHandler()
{
	can_frame frm;
	uint8_t irq = mcp2515.getInterrupts();
	//check messages in buffer 0
	if (irq & MCP2515::CANINTF_RX0IF)
	{
		mcp2515.readMessage(MCP2515::RXB0, &frm);
		if (!cf_stream.put(frm)) //no space
			arduino_overflow = true;
	}
	//check messages in buffer 1
	if (irq & MCP2515::CANINTF_RX1IF)
	{
		mcp2515.readMessage(MCP2515::RXB1, &frm);
		if (!cf_stream.put(frm)) //no space
			arduino_overflow = true;
	}
	irq = mcp2515.getErrorFlags();
	//read EFLG
	if ((irq & MCP2515::EFLG_RX0OVR) | (irq & MCP2515::EFLG_RX1OVR))
	{
		mcp2515_overflow = true;
		mcp2515.clearRXnOVRFlags();
	}
	mcp2515.clearInterrupts();
	interrupt = true; //notifyloop()
}

void loop()
{
	/*if (hubNode)
	{*/
	readSerial();
	//}
	if (interrupt)
	{
		interrupt = false;
		if (mcp2515_overflow)
		{
			Serial.println("\t\t\t\tMCP2516 RX BufOverflow");
			mcp2515_overflow = false;
		}
		if (arduino_overflow)
		{
			Serial.println("\t\t\t\tArduinoBuffers Overflow");
			arduino_overflow = false;
		}

		can_frame frame;
		bool has_data;

		cli();
		has_data = cf_stream.get(frame);
		sei();
		
		while(has_data)
		{
			communication.received(&luminaire, &frame);

			cli();
			has_data = cf_stream.get(frame);
			sei();
		}
	}

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
		if (data == 0 || data == 1)
		{
			luminaire.setOccupied(data == 1);
		}
		communication.sendRequestLuminaireData(0);
		//setLuxRef(data);
	}
}