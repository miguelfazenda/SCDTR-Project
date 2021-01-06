#include "serialComm.h"
#include "EEPROM.h"
#include "glob.h"

union FloatTo4Bytes
{
	float floatValue;
	uint32_t value;
};

/**
 * Reads if there was an input on the serial port, if so, change the lux reference
 */
void SerialComm::readSerial()
{
	while (Serial.available())
	{
		uint8_t b = Serial.read();
		if (b == 255)
		{
			// Syncronism byte was read. (255) This means a message is incoming
			//Waits for and reads the command bytes
			uint8_t buf[6];
			Serial.readBytes(buf, 6);

			//uint8_t destination = buf[0];
			Command command(buf);

			/*Serial.print("[DEBUG] command destination = ");
			Serial.println(command.destination);*/
			if (command.cmd == 'D')
			{
				//Received response from 'PC Discovery', which means it is now a hub node

				//Store current time, so we know when it stopped receiving the response in some time
				pcDiscoveryHadResponse = true;
				if (hubNode != nodeId)
				{
					Serial.print("[Debug] This node is now HUB");
					hubNode = nodeId;
					communication.sendBroadcastIsHubNode();
					serialComm.sendListNodesToPC();
				}
			}
			else if (command.destination == 0)
			{
				//TODOS
				//Sends the request for the command to all nodes except for this hub
				for (uint8_t nodeIdx = 0; nodeIdx < numTotalNodes; nodeIdx++)
				{
					if (nodesList[nodeIdx] == nodeId)
						//Ignore hub (only send CAN message to other nodes)
						continue;

					communication.sendCommandRequest(nodesList[nodeIdx], command);
				}

				// The number of nodes that still haven't sent their result
				numNodesWaitingResult = numTotalNodes - 1;

				//Executes the command and stores the result (reseting the totalCommandResult accumulator)
				FloatTo4Bytes result;
				result.value = executeCommand(command);
				totalCommandResult = result.floatValue;

				//If the hub is the only node, send the total back
				sendTotalIfAllValueForTotalReceived();
			}
			else if (command.destination == nodeId)
			{
				//Este nó responde

				//Converts received bytes to object
				//Command command(buf);
				uint32_t responseValue = executeCommand(command);
				sendResponse(responseValue);
			}
			else
			{
				//Transmit on the CAN command to other node
				if (checkIfNodeExists(command.destination))
				{
					Serial.println("Sending command request!");
					communication.sendCommandRequest(command.destination, command);
				}
				else
				{
					Serial.print("[DEBUG] error, node ");
					Serial.print(command.destination);
					Serial.println(" doesn't exist");
				}

				//TODO else responde error? (talvez com um '255' e 'E'?)
			}
		}
	}
}

/**
 * Returns: the response to this command (can be a float conve to uint32_t for example)
 */
uint32_t SerialComm::executeCommand(Command command)
{
	if (command.cmd == 'g')
	{
		Convertion convert;
		if (command.getType() == 'I')
		{
			float voltage = luminaire.getVoltage();
			convert.valueFloat = luminaire.voltageToLux(voltage);
		}
		else if (command.getType() == 'd')
		{
			convert.valueFloat = luminaire.controller.u* 100.0 / 255.0;
		}
		else if (command.getType() == 'o')
		{
			convert.value32b = (luminaire.occupied == false) ? 0 : 1;
		}
		else if (command.getType() == 'O')
		{
			convert.valueFloat = luminaire.luxOccupied;
		}
		else if (command.getType() == 'U')
		{
			convert.valueFloat = luminaire.luxNonOccupied;
		}
		else if (command.getType() == 'L')
		{
			convert.valueFloat = luminaire.luxRef;			
		}
		else if (command.getType() == 'x')
		{
			uint8_t nodeIdx = nodeIndexOnGainMatrix[nodeId];
			convert.valueFloat = calibrationFSM.residualArray[nodeIdx]; 
		}
		else if (command.getType() == 'r')
		{
			convert.valueFloat = luminaire.luxRefAfterConsensus;
		}
		else if (command.getType() == 'c')
		{
			convert.valueFloat = luminaire.cost;
		}
		else if (command.getType() == 't')
		{
			convert.valueFloat = (millis() - timeSinceLastReset)/1000;
		}
		else if (command.getType() == 'p')
		{
			
		}
		else if (command.getType() == 'e')
		{
			
		}
		else if (command.getType() == 'v')
		{
			
		}

		return convert.value32b;
	}
	else if (command.cmd == 'O' || command.cmd == 'U' || command.cmd == 'c')
	{
		Serial.print(F("Value = ")); Serial.println(command.value);
		Serial.print(F("getFloatValue = ")); Serial.println(command.getFloatValue());
		if (command.cmd == 'O')
		{
			if (luminaire.luxOccupied != command.getFloatValue())
			{
				luminaire.luxOccupied = command.getFloatValue();
				//in case the node is occupied and we change it's refference we need to run consensus again
				if (luminaire.occupied == true)
				{
					luminaire.luxRef = command.getFloatValue();
					communication.sendDoConsensus();
					consensus.init();
				}
			}
		}
		else if (command.cmd == 'U')
		{
			if (luminaire.luxNonOccupied != command.getFloatValue())
			{
				luminaire.luxNonOccupied = command.getFloatValue();
				//in case the node is occupied and we change it's refference we need to run consensus again
				if (luminaire.occupied == false)
				{
					luminaire.luxRef = command.getFloatValue();
					communication.sendDoConsensus();
					consensus.init();
				}
			}
		}
		else if (command.cmd == 'c')
		{
			if(luminaire.cost != command.getFloatValue())
			{
				//Serial.println(luminaire.cost);
				luminaire.cost = command.getFloatValue();
				//Serial.println(luminaire.cost);
				communication.sendDoConsensus();
				consensus.init();
				EEPROM.put(EEPROM_ADDR_COST, luminaire.cost);
			}
		}
		return 1;
	}
	else if (command.cmd == 'r')
	{
		calibrationFSM.resetCalib();
		consensus.resetConsensus();
		lpf.resetLPF();
		luminaire.resetLuminaire();
		mainFSM.resetMainFSM();
		resetGlob();
		resetSerialComm();
		timeSinceLastReset = millis();
		return 1;
	}
	else if (command.cmd == 'o')
	{

		bool RecievedOccupiedState = command.value == 1 ? true : false;
		if (RecievedOccupiedState != luminaire.occupied)
		{
			luminaire.setOccupied(RecievedOccupiedState);
			communication.sendDoConsensus();
			consensus.init();
		}
		/*Serial.print("SET RECEIVED  ");
		Serial.print((char) command.cmd);
		Serial.print("  ");
		Serial.print(command.destination);
		Serial.print(" ");
		Serial.println(command.getValue() == 1 ? "true" : "false");*/
		return 1;
	}

	return 0;
}

/**
 * Sends the response value to a command through the serial.
 */
void SerialComm::sendResponse(uint32_t value)
{
	Serial.write(255);
	Serial.write('R');
	Serial.write((value >> 24) & 0xFF);
	Serial.write((value >> 16) & 0xFF);
	Serial.write((value >> 8) & 0xFF);
	Serial.write(value & 0xFF);
	Serial.flush();
}

void SerialComm::sendResponse(float value)
{
	//Converts float to 4byte int, to be converted in 4 bytes
	FloatTo4Bytes convertion;
	convertion.floatValue = value;
	sendResponse(convertion.value);
}

void SerialComm::receivedCommandResponseFromCAN(uint8_t sender, uint32_t value)
{
	if (!runningTotalCommand)
	{
		//The command running isn't a total command, because the
		sendResponse(value);
	}
	else
	{
		//Running a total command
		//Send the result if this was the last value the HUB was waiting for
		FloatTo4Bytes convertion;
		convertion.value = value;

		totalCommandResult += convertion.floatValue;
		numNodesWaitingResult--;
		sendTotalIfAllValueForTotalReceived();
	}
}

/**
 * For total commands (ex. g I T), this function checks if the values from all nodes were received.
 *   If so, send the result back to the computer
 */
void SerialComm::sendTotalIfAllValueForTotalReceived()
{
	if (numNodesWaitingResult == 0)
	{
		//Send value
		sendResponse(totalCommandResult);

		//Finished running the 'total command'
		runningTotalCommand = false;
	}
}

//PC Discovery - sends message to see if pc responds
void SerialComm::sendPCDiscovery()
{
	if (hubNode == nodeId)
	{
		// If it's a hub node, it decides it is no longer connected to a PC if the computer
		//   didn't respond to the last descovery message
		if (!pcDiscoveryHadResponse)
		{
			hubNode = 0;
			communication.sendBroadcastNoLongerIsHubNode();
		}
	}

	// Sets as false so we check next time if it was changed to true
	pcDiscoveryHadResponse = false;

	Serial.write(255);
	Serial.write('D');
	Serial.flush();
}

/**
 * Sends a message with the list of nodes to the PC
 */
void SerialComm::sendListNodesToPC()
{
	Serial.write(255);
	Serial.write('L');
	//Indicates the number of nodes
	Serial.write(numTotalNodes);
	for (uint8_t i = 0; i < numTotalNodes; i++)
		Serial.write(nodesList[i]);

	Serial.flush();
}

void SerialComm::resetSerialComm()
{
	numNodesWaitingResult = 0;

    runningTotalCommand = false;

    totalCommandResult = 0.0;
}