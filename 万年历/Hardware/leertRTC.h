#ifndef LEERTRTC_H
#define LEERTRTC_H

typedef struct LeertTime{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
}LeertTime;

void setRTC(void);
void setTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, int8_t zone);
LeertTime getTime(int8_t zone);

#endif
