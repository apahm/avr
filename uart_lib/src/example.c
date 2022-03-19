#include "config.h"

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/delay.h>

#include "uart.h"
#include "OneWire.h"
#include "crc16.h"
/* Define CPU frequency in Hz in Makefile or toolchain compiler configuration */

#ifndef F_CPU
#error "F_CPU undefined, please define CPU frequency in Hz in Makefile or compiler configuration"
#endif

/* Define UART baud rate here */
#define UART_BAUD_RATE 9600

/* Define char symbol */

#define TRUE 1
#define FALSE 0
#define CHAR_NEWLINE '\n'
#define CHAR_RETURN '\r'
#define RETURN_NEWLINE "\r\n"

unsigned char data_count = 0;
unsigned char data_in[128];
unsigned char command_in[128];

uint8_t flag_first_symbol = 0;
uint8_t length_msg = 0;

void main_parser()
{
}

void copy_command()
{
	uint8_t length_receive_msg = data_in[1];
	// Copy the contents of data_in into command_in
	memcpy(command_in, data_in, length_receive_msg);
	// Now clear data_in, the UART can reuse it now
	memset(data_in, 0, length_receive_msg);
}

void process_command()
{
	uint8_t length_receive_msg = command_in[1];
	char buffer[8];

	if (makeCRC16(command_in, length_receive_msg, TRUE) == 0)
	{
		for (uint8_t i = 0; i < length_receive_msg; i++)
		{
			uart_putc(command_in[i]);
		}
	}
}

void explodeDoubleNumber(int *numbers, double flt)
{
	numbers[0] = abs((int)flt);
	numbers[1] = abs((int)((flt - ((int)flt)) * 10));
}

void printTemp(double d)
{
	char text[17] = "T = ";
	int fs[2];
	char num[5];

	explodeDoubleNumber(fs, d);
	if (d < 0)
	{
		strcat(text, "-");
	}

	itoa(fs[0], num, 10);
	strcat(text, num);
	strcat(text, ".");
	itoa(fs[1], num, 10);
	strcat(text, num);
	strcat(text, "'C");
	// uart_puts(text);
}

double getTemp(void)
{

	uint8_t temperatureL;
	uint8_t temperatureH;
	double retd = 0;

	skipRom();
	writeByte(CMD_CONVERTTEMP);

	_delay_ms(750);

	skipRom();
	writeByte(CMD_RSCRATCHPAD);

	temperatureL = readByte();
	temperatureH = readByte();

	retd = ((temperatureH << 8) + temperatureL) * 0.0625;

	return retd;
}

int main(void)
{
	double temperature;

	uart_init(UART_BAUD_SELECT(UART_BAUD_RATE, F_CPU));

	oneWireInit(PINB0);

	sei();

	temperature = getTemp();
	printTemp(temperature);

	memset(data_in, 0, 128);

	for (;;)
	{
		unsigned int c = uart_getc();
		if (c & UART_NO_DATA)
		{
			if (flag_first_symbol == 1)
			{
				data_count = 0;
				length_msg = 0;
				flag_first_symbol = 0;
				memset(data_in, 0, 128);
			}
		}
		else
		{
			// new data available from UART check for Frame or Overrun error
			if (c & UART_FRAME_ERROR)
			{
				/* Framing Error detected, i.e no stop bit detected */
			}
			if (c & UART_OVERRUN_ERROR)
			{
				/* Overrun, a character already present in the UART UDR register was
				 * not read by the interrupt handler before the next character arrived,
				 * one or more received characters have been dropped */
			}
			if (c & UART_BUFFER_OVERFLOW)
			{
				/* We are not reading the receive buffer fast enough,
				 * one or more received character have been dropped  */
			}

			// Add char to input buffer
			data_in[data_count] = c;

			if (data_count > 1)
			{
				data_count++;
			}

			if (data_count == 1 & flag_first_symbol == 1)
			{
				length_msg = c;
				data_count++;
			}

			if (c == 0x01 & flag_first_symbol == 0)
			{
				flag_first_symbol = 1;
				data_count++;
				_delay_ms(100);
			}

			if (data_count == length_msg)
			{
				data_count = 0;
				length_msg = 0;
				flag_first_symbol = 0;
				copy_command();
				process_command();
			}
		}
	}
}
