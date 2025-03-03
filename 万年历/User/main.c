#include "stm32f10x.h"// Device header
#include "Delay.h"
#include "OLED.h"
#include "leertHardware.h"
#include "leertRTC.h"

//状态代号
typedef enum{
	OFF, NORMAL, YEAR, MONTH, DAY, HOUR, MINUTE, SECOND 
}Status;

//每个月份所含有的天数
uint8_t dayNumOfMonth[12]= {31,28,31,30,31,30,31,31,30,31,30,31};

Status status = OFF;//表示当前状态的旗变量

LeertTime t;//时间

uint8_t chgDir = 1;//指示LED灯珠由亮变暗(0)或由暗变亮(1)的变量
int duty = 0;//指示LED灯珠亮度(PWM占空比)的变量(合法取值为0到1000)

/**
 * @brief	设置TIM1定时器中断(周期为1秒)
 */
void Timer1_Init(){
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE); //时钟使能

	TIM_TimeBaseStructure.TIM_Period = 10000; //设置自动重装载寄存器周期值
	TIM_TimeBaseStructure.TIM_Prescaler =(7200-1);//设置预分频值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //设置时钟分割
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;//向上计数模式
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;//重复计数设置
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure); //参数初始化
	TIM_ClearFlag(TIM1, TIM_FLAG_Update);//清中断标志位

	TIM_ITConfig(      //使能或者失能指定的TIM中断
		TIM1,            //TIM1
		TIM_IT_Update  | //TIM 更新中断源
		TIM_IT_Trigger,  //TIM 触发中断源 
		ENABLE  	     //使能
    );
	
	//设置优先级
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;  
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;//先占优先级0级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;  	   //从优先级0级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure); 

	TIM_Cmd(TIM1, ENABLE);  //使能TIMx外设
}

void TIM1_UP_IRQHandler(){
	if(TIM_GetITStatus(TIM1, TIM_IT_Update) == SET){
		chgDir = 1 - chgDir;
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}

void EXTI4_IRQHandler(void){
	if(EXTI_GetFlagStatus(EXTI_Line4) == SET && digitalRead('A', 4) == 0){
		if(status == NORMAL){//普通状态下，按下按钮，进入时间调节模式
			status = YEAR;
		}
		EXTI_ClearITPendingBit(EXTI_Line4);
	}
}

/**
 * @brief	判断是否是闰年
 * @retval	是闰年则返回1，否则返回0
 */
uint8_t isLeapYear(uint16_t year){
	if(!(year%400))return 1;
	if(!(year%100))return 0;
	return(!(year%4));
}

/**
 * @brief	将时间显示到OLED
 */
void showTime(void){
	t = getTime(0);
	OLED_ShowNum(2,6,t.year,4);
	OLED_ShowNum(2,11,t.month,2);
	OLED_ShowNum(2,14,t.day,2);
	OLED_ShowNum(3,6,t.hour,2);
	OLED_ShowNum(3,9,t.minute,2);
	OLED_ShowNum(3,12,t.second,2);
}

/**
 * @brief	编码器模块初始化
 * @note	定时器未开启，需要手动调用TIM_Cmd(TIM3, ENABLE)开启
 */
void Encode_Init(void){
	pinMode('A', GPIO_Pin_6 | GPIO_Pin_7, GPIO_Mode_IPU);

	//初始化时基单元
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 0xFFFF;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 3;//分频器设置为4分频
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

	TIM_ICInitTypeDef TIM_ICInitStructure;
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInitStructure.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);

	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
}

/**
 * @brief	获取编码器计数
 * @retval	编码器当前计数
 * @note	本函数可能根据时间的上下限，对Counter进行写入
 */
int16_t Get_Encoder(void){
	int16_t temp = TIM_GetCounter(TIM3);
	switch (status)
	{
	case YEAR:
		if(temp > 9999){
			TIM_SetCounter(TIM3, 0);
			temp = 0;
		}else if(temp < 0){
			TIM_SetCounter(TIM3, 9999);
			temp = 9999;
		}
		break;

	case MONTH:
		if(temp > 12){
			TIM_SetCounter(TIM3, 1);
			temp = 1;
		}else if(temp < 1){
			TIM_SetCounter(TIM3, 12);
			temp = 12;
		}
		break;

	case DAY:
		uint8_t maxDayNum;
		if(t.month == 2 && isLeapYear(t.year))maxDayNum = 29;
		else maxDayNum = dayNumOfMonth[t.month - 1];
		if(temp > maxDayNum){
			TIM_SetCounter(TIM3, 1);
			temp = 1;
		}else if(temp < 1){
			TIM_SetCounter(TIM3, maxDayNum);
			temp = maxDayNum;
		}
		break;
	
	case HOUR:
		if(temp > 23){
			TIM_SetCounter(TIM3, 0);
			temp = 0;
		}else if(temp < 0){
			TIM_SetCounter(TIM3, 23);
			temp = 23;
		}
		break;
	
	case MINUTE:
	case SECOND:
		if(temp > 59){
			TIM_SetCounter(TIM3, 0);
			temp = 0;
		}else if(temp < 0){
			TIM_SetCounter(TIM3, 59);
			temp = 59;
		}
		break;
	
	default:
		break;
	}
	return temp;
}

/**
 * @brief	进入调节时间状态
 */
void changeTime(void){
	//呼吸灯状态停止变化并熄灭
	duty = 0;
	TIM_Cmd(TIM1, DISABLE);
	analogModify('A', 0, 0);

	//清屏并显示提示
	OLED_Clear();
	OLED_ShowString(2,1,"TIME");
	OLED_ShowString(3,1,"ADJUST");
	Delay_ms(1000);
	OLED_Clear();

	TIM_Cmd(TIM3, ENABLE);//开始读取编码器

	//调节年份
	TIM_SetCounter(TIM3, t.year);//编码器计数设为当前显示的年份
	OLED_ShowString(2,1,"year  ");
	while(!getKey('A', 4, 0)){
		OLED_ShowNum(3, 1, Get_Encoder(), 4);
	}
	t.year = Get_Encoder();
	status = MONTH;

	//调节月份
	TIM_SetCounter(TIM3, t.month);//编码器计数设为当前显示的月份
	OLED_ShowString(2,1,"month ");
	while(!getKey('A', 4, 0)){
		OLED_ShowString(3, 3, "  ");
		OLED_ShowNum(3, 1, Get_Encoder(), 2);
	}
	t.month = Get_Encoder();
	status = DAY;

	//调节日期
	TIM_SetCounter(TIM3, t.day);//编码器计数设为当前显示的日期
	OLED_ShowString(2,1,"day   ");
	while(!getKey('A', 4, 0)){
		OLED_ShowNum(3, 1, Get_Encoder(), 2);
	}
	t.day = Get_Encoder();
	status = HOUR;

	//调节小时
	TIM_SetCounter(TIM3, t.hour);//编码器计数设为当前显示的小时数
	OLED_ShowString(2,1,"hour  ");
	while(!getKey('A', 4, 0)){
		OLED_ShowNum(3, 1, Get_Encoder(), 2);
	}
	t.hour = Get_Encoder();
	status = MINUTE;

	//调节分钟
	TIM_SetCounter(TIM3, t.minute);//编码器计数设为当前显示的分钟数
	OLED_ShowString(2,1,"minute");
	while(!getKey('A', 4, 0)){
		OLED_ShowNum(3, 1, Get_Encoder(), 2);
	}
	t.minute = Get_Encoder();
	status = SECOND;

	//调节秒
	TIM_SetCounter(TIM3, t.second);//编码器计数设为当前显示的秒数
	OLED_ShowString(2,1,"second");
	while(!getKey('A', 4, 0)){
		OLED_ShowNum(3, 1, Get_Encoder(), 2);
	}
	t.second = Get_Encoder();

	setTime(t.year, t.month, t.day, t.hour, t.minute, t.second, 0);//写入时间戳
	TIM_Cmd(TIM3, DISABLE);//失能时钟，停止读取编码器

	//时间显示功能继续运行
	OLED_ShowString(1,1,"calendar   ");
	OLED_ShowString(2,1,"date     .  .  ");
	OLED_ShowString(3,1,"time   :  :  ");

	//呼吸灯继续工作
	TIM_Cmd(TIM1, ENABLE);
	chgDir = 1;
	TIM_SetCounter(TIM1, 0);//强制从头开始计时周期，防止LED突然亮起

	status = NORMAL;
}

int main(void){
	OLED_Init();
	OLED_ShowString(1,1,"calendar   ");
	OLED_ShowString(2,1,"date     .  .  ");
	OLED_ShowString(3,1,"time   :  :  ");

	Encode_Init();//旋转编码器模块初始化

	setRTC();//初始化实时时钟

	//设置按键检测
	setInterruptGroup(1);
	pinMode('A', GPIO_Pin_4, GPIO_Mode_IPU);
	setEXTI('A', 4, EXTI_Trigger_Falling, 0, 0);

	//设置普通状态下的呼吸灯
	pinMode('A', GPIO_Pin_0, GPIO_Mode_AF_PP);
	Timer1_Init();
	analogWrite('A', 0, 1000, duty);

	status = NORMAL;
	while(1){
		if(status == OFF)continue;

		else if(status == NORMAL){
			showTime();
			if(chgDir == 1 && duty <= 990){
				duty += 10;
				analogModify('A', 0, duty);
			}
			else if(chgDir == 0 && duty >= 10){
				duty -= 10;
				analogModify('A', 0, duty);
			}
			Delay_ms(10);
		}

		else if(status == YEAR){
			changeTime();
		}
	}
}
