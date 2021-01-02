#include "command.h"

#include "convertion.h"

Command::Command()
{
	cmd = '\0';
	destination = 0;
	value = 0;
}
Command::Command(char cmd, uint8_t destination, char type)
{
	this->cmd = cmd;
	this->destination = destination;
	//this->data.type = type;
	setType(type);
}
Command::Command(char cmd, uint8_t destination, float value)
{
	this->cmd = cmd;
	this->destination = destination;
	setFloatValue(value);
}

Command::Command(uint8_t* buffer)
{
	this->destination = buffer[0];
	this->cmd = buffer[1];
	this->value = int((uint32_t)(buffer[2]) << 24 |
		(uint32_t)(buffer[3]) << 16 |
		(uint32_t)(buffer[4]) << 8 |
		(uint32_t)(buffer[5]));
}

size_t Command::toByteArray(char* array)
{
	array[0] = destination;
	array[1] = cmd;
	array[2] = (value >> 24) & 0xFF;
	array[3] = (value >> 16) & 0xFF;
	array[4] = (value >> 8) & 0xFF;
	array[5] = value & 0xFF;
	return 6;
}

void Command::setType(char t)
{
	value = t;
}
char Command::getType()
{
	return (int8_t)value;
}
void Command::setFloatValue(float v)
{
	Convertion convertion;
	convertion.valueFloat = v;
	value = convertion.value32b;
}
float Command::getFloatValue()
{
	Convertion convertion;
	convertion.value32b = value;
	return convertion.valueFloat;
}