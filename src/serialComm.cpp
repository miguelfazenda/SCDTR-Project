#include "serialComm.h"
#include "glob.h"

SerialComm::SerialComm() 
{


}

struct Command {
	char cmd;
	uint8_t destination;
	union {
		float value;
		char type;
	} data;
};

void SerialComm::sendFrequentData() 
{
    Serial.write((uint8_t)255);
    Serial.write('F'); //F for frequent data, R for response
    Serial.write('I');
    Serial.write(10); //TODO replace with luminance

    Serial.write((uint8_t)255);
    Serial.write('F'); //F for frequent data, R for response
    Serial.write('d');
    Serial.write(128); //TODO replace with duty cycle

	Serial.flush();
}

void SerialComm::readCommandGet(uint8_t getParam, uint8_t destination)
{
    Serial.print("GET RECEIVED  ");
    Serial.print((char) getParam);
    Serial.print("  ");
    Serial.println(destination);
}

void SerialComm::readCommandSet(uint8_t setParam, uint8_t destination, float value)
{
    Serial.print("SET RECEIVED  ");
    Serial.print((char) setParam);
    Serial.print("  ");
    Serial.print(destination);
    Serial.print("  ");
    Serial.println(value);

    if(destination == nodeId)
    {
        //Destination was this node
        
    }
    else
    {
        //Transmit to node
    }
}

/**
 * Reads if there was an input on the serial port, if so, change the lux reference
 */
void SerialComm::readSerial()
{
	while(Serial.available())
	{
		uint8_t b = Serial.read();
		if(b == 255)
		{
			// Syncronism byte was read. (255) This means a message is incoming
			//Waits for and reads the first byte
			uint8_t cmd;
			Serial.readBytes(&cmd, 1);
			
			uint8_t buf[5];

			if(cmd == 'g')
			{
				Serial.readBytes(buf, 2);

				//GET MESSAGE
				uint8_t getParam = buf[0];
				uint8_t destination = buf[1];
                readCommandGet(getParam, destination);
			}
			else if(cmd == 'O' || cmd == 'U' || cmd == 'c' /*|| cmd == 'o'*/)
			{
				Serial.readBytes(buf, 3);

				//GET MESSAGE
				uint8_t destination = buf[0];
				uint8_t valInt = buf[1];
				uint8_t valDecimal = buf[2];

				// To convert the decimal part which comes as a byte to a float we need to divide it by 100 (if valDecimal > 1000), 100 or 10
				float decimalDivider = 10;
				if(valDecimal/100 > 1)
					decimalDivider = 1000;
				else if(valDecimal/10 > 1)
					decimalDivider = 100;

				float value = valInt + (float)valDecimal/decimalDivider;

				readCommandSet(cmd, destination, value);
			}
			else if(cmd == 'o')
			{
				Serial.readBytes(buf, 2);

				//GET MESSAGE
				uint8_t destination = buf[0];
				uint8_t valBool = buf[1];
				Serial.print("SET RECEIVED  ");
				Serial.print((char) cmd);
				Serial.print("  ");
				Serial.print(destination);
				Serial.print(" ");
				Serial.println(valBool ? "true" : "false");
			}
		}
	}
}