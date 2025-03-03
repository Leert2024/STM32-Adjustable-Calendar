#include "stm32f10x.h"                  // Device header
#include <time.h>
#include "leertRTC.h"

/**
* @brief	初始化实时时钟
*/
void setRTC(void){
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);//开启备用电源

	//如果备用电源没有断电过，不需要重新初始化
	if(BKP_ReadBackupRegister(BKP_DR1) == 0xA5A5){
		RTC_WaitForSynchro();
		RTC_WaitForLastTask();
		return;
	}

	RCC_LSEConfig(RCC_LSE_ON);//开启外部晶振
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) != SET);//等待
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	RCC_RTCCLKCmd(ENABLE);
	RTC_WaitForSynchro();
	RTC_WaitForLastTask();//对RTC进行过写入操作后，需等待其操作完成

	RTC_SetPrescaler(32768 - 1);//设置预分频器
	RTC_WaitForLastTask();

	RTC_SetCounter(0);//时间暂设为1970年1月1日00:00:00
	RTC_WaitForLastTask();
}

/**
* @brief	设置系统时间
* @param	year 年份
* @param	month 月份
* @param	day 日期
* @param	hour 小时
* @param	minute 分钟
* @param	second 秒
* @param	zone 当前时区较伦敦时间提前的小时数(若落后则为负值)
*/
void setTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, int8_t zone){
	struct tm time_date;//含有日期与时间信息的结构体
	time_date.tm_year = year - 1900;
	time_date.tm_mon = month - 1;
	time_date.tm_mday = day;
	time_date.tm_hour = hour;
	time_date.tm_min = minute;
	time_date.tm_sec = second;
	RTC_SetCounter(mktime(&time_date) - zone*3600);
	RTC_WaitForLastTask();//等待写入完成
	BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);//在BKP寄存器中做标记
}

/**
* @brief	获取当前系统时间
* @param	zone 当前时区较伦敦时间提前的小时数(若落后则为负值)
* @retval	一个代表当前系统时间的LeertTime对象
*/
LeertTime getTime(int8_t zone){
	time_t time_cnt = RTC_GetCounter();//获取当前时间戳
	time_cnt += zone*3600;
	LeertTime result;
	struct tm time_date = *localtime(&time_cnt);
	result.year = time_date.tm_year + 1900;
	result.month = time_date.tm_mon + 1;
	result.day = time_date.tm_mday;
	result.hour = time_date.tm_hour;
	result.minute = time_date.tm_min;
	result.second = time_date.tm_sec;

	return result;
}
