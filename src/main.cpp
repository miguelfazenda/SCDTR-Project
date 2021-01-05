#include <Arduino.h>
#include <EEPROM.h>
#include <mcp2515.h>
#include "communication.h"
#include "luminaire.h"
#include "can_frame_stream.h"
#include "serialComm.h"
#include "glob.h"

uint8_t nodeId;
uint8_t nodesList[MAX_NUM_NODES] = {0};
uint8_t nodeIndexOnGainMatrix[MAX_NODE_ID + 1] = {0};
uint8_t numTotalNodes;
uint8_t hubNode = 0;
unsigned long timeSinceLastReset = 0;
bool didControl = false;

Communication communication;
Luminaire luminaire;
MainFSM mainFSM;
LPF lpf;
CalibrationFSM calibrationFSM;
SerialComm serialComm;
Consensus consensus;

MCP2515 mcp2515(10);

void readSerial();
void irqHandler();
bool checkIfNodeExists(uint8_t destination);
void sendFrequentData();

volatile bool interrupt = false;
volatile bool mcp2515_overflow = false;
volatile bool arduino_overflow = false;

/*volatile*/ can_frame_stream cf_stream;

//frequent data is the iluminance and duty cycle that should be periodicaly sent
unsigned long timeLastSentFrequentData = 0;

void setup()
{
	// initialize serial communications at 1Mbps
	Serial.begin(1000000);

	// TODO Retirar
	// pinMode(LED_BUILTIN, OUTPUT);
	// TODO Retirar

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

	if (nodeId == hubNode)
	{
		serialComm.sendListNodesToPC();
	}
}

void resetGlob()
{
	for (uint8_t i = 0; i < numTotalNodes; i++)
	{
		nodeIndexOnGainMatrix[nodesList[i]] = 0;
		nodesList[i] = 0;
	}
	numTotalNodes = 0;

	//Registers this node's id on the nodesList array
	registerNewNode(nodeId);
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
	irq = mcp2515.getInterrupts();
	if (irq & MCP2515::CANINTF_RX1IF)
	{
		mcp2515.readMessage(MCP2515::RXB1, &frm);
		if (!cf_stream.put(frm)) //no space
			arduino_overflow = true;
	}
	uint8_t irq_error = mcp2515.getErrorFlags();
	//read EFLG
	if ((irq_error & MCP2515::EFLG_RX0OVR) | (irq_error & MCP2515::EFLG_RX1OVR))
	{
		mcp2515_overflow = true;
		mcp2515.clearRXnOVRFlags();
	}

	//mcp2515.clearInterrupts();

	interrupt = true; //notifyloop()
}

void loop()
{
	serialComm.readSerial();

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

	unsigned long timeNow = millis();
	if (timeNow - timeLastSentFrequentData > 2000)
	{
		serialComm.sendPCDiscovery();

		if (hubNode != 0)
			sendFrequentData();
			
		timeLastSentFrequentData = timeNow;
	}

	mainFSM.loop();

	// if (didControl)
	// 	if (hubNode != 0)
	// 		sendFrequentData();

	if (consensus.consensusState != 0)
	{
		Serial.print(F("Main .-.-> "));
		Serial.println(consensus.consensusState);
		consensus.consensus_main();
		Serial.print(F("SAI DO COSEN MAIN COM "));
		Serial.println(consensus.consensusState);
	}

	if (didControl)
		didControl = false;
}

int checkGetArguments(String data, int *flagT);
int checkSetArguments(String data, float *val);
int checkOtherArguments(String data);
bool checkIfNodeExists(uint8_t destination);
bool checkAndPrintCommandError(uint8_t destination);

void sendFrequentData()
{
	//Only sends the frequent data packet if there is a hubnode
	if (hubNode != 0)
	{
		float iluminance = luminaire.voltageToLux(luminaire.measuredVoltage); //TODO replace with luminance
		uint8_t pwm = luminaire.controller.u;

		SerialFrequentDataPacket frequentDataPacket(nodeId, iluminance, pwm);

		if (hubNode == nodeId)
		{
			//Envia pelo serial ao PC
			frequentDataPacket.sendOnSerial();
		}
		else
		{
			//Envia pelo can para o HUB
			communication.sendFrequentDataToHub(frequentDataPacket);
		}
	}
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