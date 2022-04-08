#ifndef CONFIG_H_
#define CONFIG_H_


#define F_CPU              2000000UL
#define ONE_WIRE_PORT      PORTC
#define ONE_WIRE_DDR       DDRC
#define ONE_WIRE_PIN       PINC

#define REQ_TEMP_DS18B20 0x18
#define ANS_TEMP_DS18B20 0x19

#define REQ_SEACH_DS18B20 0x20
#define ANS_SEACH_DS18B20 0x21

#define REQ_ADC_BUFFER 0x22
#define ANS_ADC_BUFFER 0x23

#endif /* CONFIG_H_ */