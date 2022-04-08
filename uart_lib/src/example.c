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
unsigned char data_in[32];
unsigned char command_in[32];

volatile int16_t adc_buffer[128];
volatile uint8_t buff_ready = 0;
volatile uint8_t buff_work = 0;
volatile uint8_t count_adc_buff = 0;

uint8_t flag_first_symbol = 0;
uint8_t length_msg = 0;

ISR(ADC_vect) {
	if(buff_work)
	{
		*(adc_buffer + count_adc_buff) = ADC;
		count_adc_buff++;
		if(count_adc_buff == 128)
		{
			buff_ready = 1;
			count_adc_buff = 0;
			buff_work = 0;
		}
	}
}

void adc_init(void)
{
	// ADC init
	//  reference voltage: supply AVCC
	//  channel ADC0
	//  clock: f_cpu/d
	//  Left-aligned result
	ADMUX  = (0 << REFS1) | (1 << REFS0) | (1 << ADLAR)
	| (0 << MUX3)  | (0 << MUX2)  | (0 << MUX1) | (0 << MUX0);
	ADCSRA = (1 << ADEN)  | (1 << ADSC)  | (1 << ADATE) | (1 << ADIE)
	| (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADCSRB = (0 << ADTS2) | (0 << ADTS1) | (0 << ADTS0); // Free running mode
}

void main_parser()
{
	if(command_in[2] == REQ_TEMP_DS18B20)
	{
		unsigned char command_out[9];
		memset(command_out,0,9);
		
		command_out[0] = 0x01;
		command_out[1] = 9;
		command_out[2] = ANS_TEMP_DS18B20;
		
		uint8_t temperatureL;
		uint8_t temperatureH;

		skipRom();
		writeByte(CMD_CONVERTTEMP);

		_delay_ms(750);

		skipRom();
		writeByte(CMD_RSCRATCHPAD);

		temperatureL = readByte();
		temperatureH = readByte();
		
		command_out[3] = temperatureH;
		command_out[4] = temperatureL;
		command_out[5] = 0;
		command_out[6] = 0;
		
		makeCRC16(command_out, 9, FALSE);
		
		for (uint8_t i = 0; i < 9; i++)
		{
			uart_putc(command_out[i]);
		}
		
	}
	else if(command_in[2] == REQ_SEACH_DS18B20)
	{
		unsigned char command_out[7];
		
		memset(command_out,0,7);
		
		command_out[0] = 0x01;
		command_out[1] = 7;
		command_out[2] = ANS_SEACH_DS18B20;
		command_out[3] = 0;
		command_out[4] = 0;
		makeCRC16(command_out, 7, FALSE);
		
		for (uint8_t i = 0; i < 7; i++)
		{
			uart_putc(command_out[i]);
		}
	}
	else if(command_in[2] == REQ_ADC_BUFFER)
	{
		if(command_in[3] == 128)
		{
			const uint8_t len_cmd_out = 256 + 5;
			
			unsigned char command_out[len_cmd_out];
			memset(command_out, 0, len_cmd_out);
			
			command_out[0] = 0x01;
			command_out[1] = 7;
			command_out[2] = ANS_ADC_BUFFER;
			
			memset(adc_buffer, 0, 256);
			
			buff_work = 1;
			
			while(!buff_ready);
			
			buff_ready = 0;
			
			memcpy(&command_out[3], adc_buffer, 256);
						
			makeCRC16(command_out, len_cmd_out, FALSE);
							
			for (uint8_t i = 0; i < len_cmd_out; i++)
			{
				uart_putc(command_out[i]);
			}
		
		}
	}
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
		main_parser();
		/*
		for (uint8_t i = 0; i < length_receive_msg; i++)
		{
			uart_putc(command_in[i]);
		}
		*/
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
	uart_puts(text);
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

inline void printTempMult(double d, uint8_t i) {
	char text[17] = "T[";
	int fs[2];
	char num[5];
	
	itoa(i, num, 10);
	strcat(text, num);
	strcat(text, "]=");
	
	explodeDoubleNumber(fs, d);
	if (d < 0) {
		strcat(text, "-");
	}
	itoa(fs[0], num, 10);
	strcat(text, num);
	strcat(text, ".");
	itoa(fs[1], num, 10);
	strcat(text, num);
	strcat(text, "'C");
	uart_puts(text);
	uart_puts(RETURN_NEWLINE);
}

double getTempMult(uint64_t ds18b20s) {
	uint8_t temperatureL;
	uint8_t temperatureH;
	double retd = 0;


	setDevice(ds18b20s);
	writeByte(CMD_CONVERTTEMP);

	_delay_ms(750);

	setDevice(ds18b20s);
	writeByte(CMD_RSCRATCHPAD);

	temperatureL = readByte();
	temperatureH = readByte();

	retd = ((temperatureH << 8) + temperatureL) * 0.0625;

	return retd;
}

int main(void)
{
	uart_init(UART_BAUD_SELECT(UART_BAUD_RATE, F_CPU));

	oneWireInit(PINC0);

	adc_init();

	sei();
	
	/*
	double temperature;
	
	const uint8_t n_ds18b20 = 2;
	uint64_t roms[n_ds18b20];
	
	searchRom(roms, n_ds18b20);
	
	for (uint8_t i = 0; i < n_ds18b20; i++)
	{
		temperature = getTempMult(roms[i]);
		printTempMult(temperature,i);
		uart_puts(RETURN_NEWLINE);
		_delay_ms(500);
	}
	*/
	
	memset(data_in, 0, 32);
	memset(command_in, 0, 32);
	
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
				memset(data_in, 0, 32);
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
