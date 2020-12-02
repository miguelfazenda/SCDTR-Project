#include <Arduino.h>
#include <EEPROM.h>
#include <mcp2515.h>
#include "communication.h"
#include "luminaire.h"
#include "can_frame_stream.h"
#include "glob.h"

uint8_t nodeId;
uint8_t nodesList[10] = {0};
uint8_t nodeIndexOnGainMatrix[160];
unsigned int numTotalNodes;

Communication communication;
Luminaire luminaire;
MainFSM mainFSM;
LPF lpf;
CalibrationFSM calibrationFSM;

#define EEPROM_ADDR_CANID 0x00 //Address of where to get the can id of this arduino on the EEPROM

MCP2515 mcp2515(10);

void readSerial();
void irqHandler();

volatile bool interrupt = false;
volatile bool mcp2515_overflow = false;
volatile bool arduino_overflow = false;

/*volatile*/ can_frame_stream cf_stream;

/*#define STATE_INIT 0
#define STATE_ALGO 1
int state = STATE_INIT;*/

void setup()
{
	// initialize serial communications at 1Mbps
	Serial.begin(1000000);

	// Change PWM frequency on PIN 9
	TCCR1B = TCCR1B & (B11111000 | B00000001);

	//Read from EEPROM the CAN id
	nodeId = EEPROM.read(EEPROM_ADDR_CANID);
	nodesList[0] = nodeId; //Add it to the list of Nodes
	numTotalNodes = 1;

	nodeIndexOnGainMatrix[0] = 0; //This means that if no led is on, it is saved on the line 0 of the matrix

	Serial.print(" ------  Node ");
	Serial.println(nodeId);
	Serial.println();

	/**
	 * INIT CAN BUS
	 */
	attachInterrupt(0, irqHandler, FALLING);
	//use interrupt at pin 2
	//Must tell SPI lib that ISR for interrupt vector zero will be using SPI
	SPI.usingInterrupt(0);
	communication.init(&mcp2515, &cf_stream);

	//luminaire.init(false);
}

/**
 * Called when a new node is known.
 * 	It adds the node Id to the nodesList (keeps it sorted ascending)
 * 	Also keeps the nodeIndexOnGainMatrix pointing to where each node belongs in the gainMatrix
 */
void registerNewNode(uint8_t id)
{
	Serial.print("Registering new node ");
	Serial.println(id);
	//Sorted insert in the nodesList
	for (unsigned int i = 0; i < numTotalNodes + 1; i++)
	{
		//Find where to insert
		if (nodesList[i] > id || nodesList[i] == 0) //(0 mean a free space in the list)
		{
			//Insert here
			//Moves all other elements down
			uint8_t oldVal = nodesList[i];
			for (unsigned int j = i + 1; j < numTotalNodes + 14; j++)
			{
				uint8_t val = oldVal;
				oldVal = nodesList[j];
				nodesList[j] = val;
				if (oldVal == 0)
					break;
			}

			//Puts the value
			nodesList[i] = id;
			break;
		}
	}
	numTotalNodes++;

	//This makes sure the nodeIndexOnGainMatrix is right
	//nodeIndexOnGainMatrix translates the nodeId to the index it is stored on the gainMatrix
	//This first nodeId -> 1, the second nodeId -> 2, ...
	for (unsigned int i = 0; i < numTotalNodes; i++)
	{
		uint8_t nodeId = nodesList[i];
		nodeIndexOnGainMatrix[nodeId] = i + 1;
	}
}

void irqHandler()
{
	can_frame frm;
	uint8_t irq = mcp2515.getInterrupts();
	//check messages in buffer 0
	if (irq & MCP2515::CANINTF_RX0IF)
	{
		mcp2515.readMessage(MCP2515::RXB0, &frm);
		/*Serial.print("BF0 ");
		Serial.print(frm.can_id);*/
		if (!cf_stream.put(frm)) //no space
			arduino_overflow = true;
	}
	//check messages in buffer 1
	if (irq & MCP2515::CANINTF_RX1IF)
	{
		mcp2515.readMessage(MCP2515::RXB1, &frm);
		/*Serial.print("BF1 ");
		Serial.print(frm.can_id);*/
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
	/*if(state == STATE_INIT) {
		if(micros() > 2000000) {
			//After 2 seconds, sends a message through the CAN bus for the other nodes to register it's presence
			communication.sendBroadcastWakeup();
			state = STATE_ALGO;
		}
	}*/

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

		while (has_data)
		{
			communication.received(&luminaire, &frame);

			cli();
			has_data = cf_stream.get(frame);
			sei();
		}
	}

	mainFSM.loop();
	//luminaire.loop();
}

int checkGetArguments(String data);
int checkSetArguments(String data, float *val);
int checkOtherArguments(String data);

/**
 * Reads if there was an input on the serial port, if so, change the lux reference
 */
void readSerial()
{
	String data;
	int destination = 0;
	float val = 0;
	while (Serial.available())
	{
		data = Serial.readString();
		switch (data[0])
		{
		case 'g': //command type get
			destination = checkGetArguments(data);
			if (destination == -1)
			{
				Serial.println("No command recognized!");
				return;
			}
			if (destination == -2)
			{
				Serial.println("Destination doesn't exist");
				return;
			}
			//start get process
			if (val != -1) // if val argument is a float
			{
				//if(data[3])
				communication.sendHubGetValue(destination, 'I');
			}
			else
			{ //if val argument is 'T'
			}

			break;
		case 'o': //command type occupancy
			destination = checkSetArguments(data, &val);
			if (destination == -1)
			{
				Serial.println("No command recognized!");
				return;
			}
			if (destination == -2)
			{
				Serial.println("Destination doesn't exist");
				return;
			}
			//start occupancy process

			break;
		case 'O': //command type set Occupied reference
			destination = checkSetArguments(data, &val);
			if (destination == -1)
			{
				Serial.println("No command recognized!");
				return;
			}
			if (destination == -2)
			{
				Serial.println("Destination doesn't exist");
				return;
			}
			//start set illuminance occupied process
			break;
		case 'U': //command type set unnocupied refference
			destination = checkSetArguments(data, &val);
			if (destination == -1)
			{
				Serial.println("No command recognized!");
				return;
			}
			if (destination == -2)
			{
				Serial.println("Destination doesn't exist");
				return;
			}
			//start illuminance unoccupied  process
			break;
		case 'c': //command type energy cost
			destination = checkSetArguments(data, &val);
			if (destination == -1)
			{
				Serial.println("No command recognized!");
				return;
			}
			if (destination == -2)
			{
				Serial.println("Destination doesn't exist");
				return;
			}
			//start energy cost process

			break;
		case 'r': //command type reset
			if (data.length() != 1)
			{
				Serial.println("No command recognized!");
				return;
			}
			//start reset process
			break;
		case 'b': //command type buffer
			destination = checkOtherArguments(data);
			if (destination == -1)
			{
				Serial.println("No command recognized!");
				return;
			}
			if (destination == -2)
			{
				Serial.println("Destination doesn't exist");
				return;
			}
			//starts buffer process
			break; //command type stop/start
		case 's':
			destination = checkOtherArguments(data);
			if (destination == -1)
			{
				Serial.println("No command recognized!");
				return;
			}
			if (destination == -2)
			{
				Serial.println("Destination doesn't exist");
				return;
			}
			//starts stop/start process
			break;
		default: //no messagem type recognized
			Serial.println("No command recognized!");
			return;
			break;
		}
	}
}
int checkGetArguments(String data)
{
	String arguments = "IdoOULxRcptevf"; //string that has every char thats corresponds to
										 //one argument of comands type get
	int destination = 0;
	if (data[1] != ' ' || data[3] != ' ')
	{
		return -1;
	}
	for (size_t i = 0; i < data.length(); i++)
	{
		if (data[2] != arguments[i])
		{
			return -1; //this value represents that the command doesnt exist
		}
	}
	destination = (data.substring(3, data.length() - 1)).toInt();
	if (destination == 0)
	{
		return -1;
	}
	for (size_t i = 0; i < numTotalNodes; i++) //checks if destination exists
	{
		if (destination != nodesList[i])
		{
			return -2;
		}
	}
	return destination;
}

int checkSetArguments(String data, float *val)
{
	int idx_aux = 0;
	int destination = 0;
	if (data[1] != ' ')
	{
		return -1;
	}
	idx_aux = data.indexOf(' ', 2);
	if (idx_aux == -1)
	{
		return -1;
	}
	destination = (data.substring(2, idx_aux - 1)).toInt();
	if (data[idx_aux + 1] == 'T' && data.length() == idx_aux + 2)
	{
		*val = -1; //argument isn't a float,
	}
	else
	{
		*val = (data.substring(idx_aux + 1, data.length() - 1)).toFloat();
	}
	if (destination == 0 || *val == 0)
	{
		return -1;
	}
	for (size_t i = 0; i < numTotalNodes; i++) //checks if destination exists
	{
		if (destination != nodesList[i])
		{
			return -2;
		}
	}
	return destination;
}

int checkOtherArguments(String data)
{
	int destination = 0;
	if (data[1] != ' ' || data[3] != ' ')
	{
		return -1;
	}
	else if (data[2] != 'I' || data[2] != 'd')
	{
		return -1;
	}
	destination = (data.substring(3, data.length() - 1)).toInt();
	if (destination == 0)
	{
		return -1;
	}
	for (size_t i = 0; i < numTotalNodes; i++) //checks if destination exists
	{
		if (destination != nodesList[i])
		{
			return -2;
		}
	}
	return destination;
}