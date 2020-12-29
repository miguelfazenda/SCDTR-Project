#include <Arduino.h>
#include <EEPROM.h>
#include <mcp2515.h>
#include "communication.h"
#include "luminaire.h"
#include "can_frame_stream.h"
#include "glob.h"

uint8_t nodeId;
uint8_t nodesList[MAX_NUM_NODES] = {0};
uint8_t nodeIndexOnGainMatrix[MAX_NODE_ID + 1] = {0};
uint8_t numTotalNodes;

Communication communication;
Luminaire luminaire;
MainFSM mainFSM;
LPF lpf;
CalibrationFSM calibrationFSM;
Consensus consensus;

MCP2515 mcp2515(10);

void readSerial();
void irqHandler();

volatile bool interrupt = false;
volatile bool mcp2515_overflow = false;
volatile bool arduino_overflow = false;

/*volatile*/ can_frame_stream cf_stream;

bool used_RX0 = false;
bool used_RX1 = false;

void setup()
{
	// initialize serial communications at 1Mbps
	Serial.begin(1000000);

	// Change PWM frequency on PIN 9
	TCCR1B = TCCR1B & (B11111000 | B00000001);

	numTotalNodes = 0;

	//Read from EEPROM the CAN id
	nodeId = EEPROM.read(EEPROM_ADDR_NODEID);
	//Registers this node's id on the nodesList array
	registerNewNode(nodeId);

	nodeIndexOnGainMatrix[0] = 0; //This means that if no led is on, it is saved on the line 0 of the matrix

	Serial.print("\n ------  Node ");
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

	luminaire.init(false);
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
	for (uint8_t i = 0; i < numTotalNodes + 1; i++)
	{
		//Find where to insert
		if (nodesList[i] > id || nodesList[i] == 0) //(0 mean a free space in the list)
		{
			//Insert here
			//Moves all other elements down
			uint8_t oldVal = nodesList[i];
			for (uint8_t j = i + 1; j < numTotalNodes + 1; j++)
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
	for (uint8_t i = 0; i < numTotalNodes; i++)
	{
		uint8_t nodeId = nodesList[i];
		nodeIndexOnGainMatrix[nodeId] = i;
	}
}

void irqHandler()
{

	can_frame frm;
	uint8_t irq = mcp2515.getInterrupts();
	//check messages in buffer 0
	if (irq & MCP2515::CANINTF_RX0IF)
	{
		used_RX0 = true;
		mcp2515.readMessage(MCP2515::RXB0, &frm);
		/*Serial.print("BF0 ");
		Serial.print(frm.can_id);*/
		if (!cf_stream.put(frm)) //no space
			arduino_overflow = true;
	}
	//check messages in buffer 1
	if (irq & MCP2515::CANINTF_RX1IF)
	{
		used_RX1 = true;
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
	if (consensus.consensusState != 0)
	{
		consensus.consensus_main();
	}
}

int checkGetArguments(String data, int *flagT);
int checkSetArguments(String data, float *val);
int checkOtherArguments(String data);
bool checkIfNodeExists(uint8_t destination);
bool checkAndPrintCommandError(uint8_t destination);

/**
 * Reads if there was an input on the serial port, if so, change the lux reference
 */
void readSerial()
{
	String data;
	int destination = 0;
	float val = 0;
	int flagT = 0; //flag that indicates if the get response should be for all the system(flagT=1) or not

	//On get commands this stores what type of value. I - iluminance, d - dutycycle
	char valueType;

	while (Serial.available())
	{
		data = Serial.readString();
		switch (data[0])
		{
		case 'g': //command type get
			destination = checkGetArguments(data, &flagT);
			if (checkAndPrintCommandError(destination) && flagT != 1)
				break;

			//start get process
			valueType = data[2];
			Serial.print("valueType = ");
			Serial.println(valueType);
			if (flagT != 1)
			{ //last argument inst T (T is a flag that indicates the response should be for all the system)
				communication.sendRequestHubGetValue(destination, valueType);
				break;
			}
			//falta meter aqui função para quando ultimo argumento é 'T'
			break;
		case 'o': //command type occupancy
			destination = checkSetArguments(data, &val);
			if (checkAndPrintCommandError(destination))
				break;
			//start occupancy process

			break;
		case 'O': //command type set Occupied reference
			destination = checkSetArguments(data, &val);
			if (checkAndPrintCommandError(destination))
				break;
			//start set illuminance occupied process
			break;
		case 'U': //command type set unnocupied refference
			destination = checkSetArguments(data, &val);
			if (checkAndPrintCommandError(destination))
				break;
			//start illuminance unoccupied  process
			break;
		case 'c': //command type energy cost
			destination = checkSetArguments(data, &val);
			if (checkAndPrintCommandError(destination))
				break;
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
			if (checkAndPrintCommandError(destination))
				break;
			//starts buffer process
			break; //command type stop/start
		case 's':
			destination = checkOtherArguments(data);
			if (checkAndPrintCommandError(destination))
				break;
			//starts stop/start process
			break;
		case 'D':
			Serial.println("------FLAGS-------");
			Serial.println(used_RX0);
			Serial.println(used_RX1);
			break;

		default: //no messagem type recognized
			Serial.println("No command recognized!");
			return;
			break;
		}
	}
}

/**
 * Returns true if there is an error
 * 	There is an error when destination == -1 our the destination doesnt't exist
 */
bool checkAndPrintCommandError(uint8_t destination)
{
	if (destination == -1)
	{
		Serial.println("No command recognized!");
		return true;
	}
	if (!checkIfNodeExists(destination))
	{
		Serial.println("Destination doesn't exist");
		return true;
	}
	return false;
}

bool checkIfNodeExists(uint8_t destination)
{
	bool destinationExists = false;
	for (size_t i = 0; i < numTotalNodes; i++) //checks if destination exists
	{
		if (destination == nodesList[i])
		{
			destinationExists = true;
			break;
		}
	}
	return destinationExists;
}

int checkGetArguments(String data, int *flagT)
{
	String arguments = "IdoOULxRcptevf"; //string that has every char thats corresponds to
										 //one argument of comands type get
	String argumentsWithT = "pevf";		 //string that has every char thats corresponds to
										 //one argument of comands type get that allows the last argument to be 'T'
	uint8_t destination = 0;
	bool validCommand = false;
	if (data[1] != ' ' || data[3] != ' ')
	{
		return -1;
	}
	for (size_t i = 0; i < arguments.length(); i++)
	{
		if (data[2] == arguments[i])
		{
			validCommand = true;
			break;
		}
	}
	for (size_t i = 0; i < argumentsWithT.length(); i++)
	{
		if (data[2] == argumentsWithT[i] && data[4] == 'T' && data.length() == 5)
		{
			*flagT = 1; //Value that corresponts to the command beeing valid and the last argument is 'T'
			return 0;
		}
	}

	if (!validCommand)
		return -1; //this value represents that the command doesnt exist

	destination = (data.substring(3, data.length() - 1)).toInt();
	if (destination == 0)
	{
		return -1;
	}
	return destination;
}

int checkSetArguments(String data, float *val)
{
	int idx_aux = 0;
	uint8_t destination = 0;
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
	*val = (data.substring(idx_aux + 1, data.length() - 1)).toFloat();

	if (destination == 0 || *val == 0)
	{
		return -1;
	}
	return destination;
}

int checkOtherArguments(String data)
{
	uint8_t destination = 0;
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
	return destination;
}