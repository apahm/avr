#include "crc16.h"

unsigned short makeCRC16(unsigned char * data, unsigned short len, unsigned char check)
{
	const unsigned short poly = 0x1021;
	unsigned short crc16 = 0x1D0F;
	unsigned int t_len;
	
	if(!check) // Transmit
	{ 
		t_len = len - 2;
		data[len - 2] = 0;
		data[len - 1] = 0;
	} 
	else // Receive
	{ 
		t_len = len;
	}

	for(unsigned int j = 0; j < t_len; j++) {
		crc16 ^= data[j] << 8;
		for(int i = 0; i < 8; i++)
		crc16 = crc16 & 0x8000 ? (crc16 << 1) ^ poly : (crc16 << 1);
	}

	// Transmit
	if(!check) 
	{ 
		data[len - 1] = (crc16 >> 0) & 0xFF;
		data[len - 2] = (crc16 >> 8) & 0xFF;
	}

	return crc16;
}