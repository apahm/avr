#include "config.h"

#define DEVICES_ERROR 1

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "OneWire.h"

void oneWireInit(uint8_t pin)
{
	ONE_WIRE_DQ = pin;
	ONE_WIRE_PORT |= (1 << ONE_WIRE_DQ);
	ONE_WIRE_DDR |= (1 << ONE_WIRE_DQ); 
}

uint8_t reset()
{
	uint8_t response;

	ONE_WIRE_PORT &= ~(1 << ONE_WIRE_DQ);
	ONE_WIRE_DDR |= (1 << ONE_WIRE_DQ);
	_delay_us(480);

	ONE_WIRE_DDR &= ~(1 << ONE_WIRE_DQ);
	_delay_us(60);

	response = (ONE_WIRE_PIN & (1 << ONE_WIRE_DQ));
	_delay_us(410);

	return response;
}

void writeBit(uint8_t bit)
{
	if (bit & 1)
	{
		cli();
		
		ONE_WIRE_PORT &= ~(1 << ONE_WIRE_DQ);
		ONE_WIRE_DDR |= (1 << ONE_WIRE_DQ); 
		_delay_us(10);
		sei();
		ONE_WIRE_DDR &= ~(1 << ONE_WIRE_DQ); 
		_delay_us(55);
	}
	else
	{
		cli();
		
		ONE_WIRE_PORT &= ~(1 << ONE_WIRE_DQ);
		ONE_WIRE_DDR |= (1 << ONE_WIRE_DQ); 
		_delay_us(65);
		ONE_WIRE_DDR &= ~(1 << ONE_WIRE_DQ); 
		sei();
		_delay_us(5);
	}
}

void writeByte(uint8_t byte)
{
	uint8_t i = 8;
	while (i--)
	{
		writeBit(byte & 1);
		byte >>= 1;
	}
}

uint8_t readByte()
{
	uint8_t i = 8, byte = 0;
	while (i--)
	{
		byte >>= 1;
		byte |= (readBit() << 7);
	}
	return byte;
}


uint8_t readBit(void)
{
	uint8_t bit = 0;
	cli();
	
	ONE_WIRE_PORT &= ~(1 << ONE_WIRE_DQ);
	ONE_WIRE_DDR |= (1 << ONE_WIRE_DQ);
	_delay_us(3);

	ONE_WIRE_DDR &= ~(1 << ONE_WIRE_DQ); 
	_delay_us(10);

	if (ONE_WIRE_PIN & (1 << ONE_WIRE_DQ))
	{
		bit = 1;
	}

	sei();
	_delay_us(45);
	return bit;
}


uint64_t readRoom(void)
{
	uint64_t oneWireDevice;
	if (reset() == 0)
	{
		writeByte(CMD_READROM);
		
		oneWireDevice = readByte();
		
		oneWireDevice |= (uint16_t)readByte() << 8 | (uint32_t)readByte() << 16 | (uint32_t)readByte() << 24 | (uint64_t)readByte() << 32 | (uint64_t)readByte() << 40 | (uint64_t)readByte() << 48;
	
		oneWireDevice |= (uint64_t)readByte() << 56;
	}
	else
	{
		return 1;
	}
	return oneWireDevice;
}


void setDevice(uint64_t rom)
{
	uint8_t i = 64;
	reset();
	writeByte(CMD_MATCHROM);
	while (i--)
	{
		writeBit(rom & 1);
		rom >>= 1;
	}
}


uint8_t crcCheck(uint64_t data8x8bit, uint8_t len)
{
	uint8_t dat, crc = 0, fb, stByte = 0;
	do
	{
		dat = (uint8_t)(data8x8bit >> (stByte * 8));
		for (int i = 0; i < 8; i++)
		{ 
			fb = crc ^ dat;
			fb &= 1;
			crc >>= 1;
			dat >>= 1;
			if (fb == 1)
			{
				crc ^= 0x8c;
			}
		}
		stByte++;
	} while (stByte < len); 
	return crc;
}


void searchRom(uint64_t *roms, uint8_t *n)
{
	uint64_t lastAddress = 0;
	uint8_t lastDiscrepancy = 0;
	uint8_t err = 0;
	uint8_t i = 0;
	do
	{
		do
		{
			lastAddress = searchNextAddress(lastAddress, &lastDiscrepancy);
			if (lastAddress != DEVICES_ERROR)
			{
				uint8_t crc = crcCheck(lastAddress, 8);
				if (crc == 0)
				{
					roms[i++] = lastAddress;
					err = 0;
				}
				else
				{
					err++;
				}
			}
			else
			{
				err++;
			}
			if (err > 3)
			{
				return;
			}
		} while (err != 0);
	} while (lastDiscrepancy != 0 && i < *n);
	*n = i;
}


uint64_t searchNextAddress(uint64_t lastAddress, uint8_t *lastDiscrepancy)
{
	uint8_t searchDirection = 0;
	uint64_t newAddress = 0;
	uint8_t idBitNumber = 1;
	uint8_t lastZero = 0;
	reset();
	writeByte(CMD_SEARCHROM);

	while (idBitNumber < 65)
	{
		uint8_t idBit = readBit();
		uint8_t cmpIdBit = readBit();

		// id_bit = cmp_id_bit = 1
		if (idBit == 1 && cmpIdBit == 1)
		{
			return DEVICES_ERROR;
		}
		else if (idBit == 0 && cmpIdBit == 0)
		{
			// id_bit = cmp_id_bit = 0
			if (idBitNumber == *lastDiscrepancy)
			{
				searchDirection = 1;
			}
			else if (idBitNumber > *lastDiscrepancy)
			{
				searchDirection = 0;
			}
			else
			{
				if ((uint8_t)(lastAddress >> (idBitNumber - 1)) & 1)
				{
					searchDirection = 1;
				}
				else
				{
					searchDirection = 0;
				}
			}
			if (searchDirection == 0)
			{
				lastZero = idBitNumber;
			}
		}
		else
		{
			// id_bit != cmp_id_bit
			searchDirection = idBit;
		}
		newAddress |= ((uint64_t)searchDirection) << (idBitNumber - 1);
		writeBit(searchDirection);
		idBitNumber++;
	}
	*lastDiscrepancy = lastZero;
	return newAddress;
}


void skipRom()
{
	reset();
	writeByte(CMD_SKIPROM);
}