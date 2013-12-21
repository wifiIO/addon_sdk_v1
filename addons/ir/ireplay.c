
/**
 * @file			ireplay.c
 * @brief			红外重放，该演示采用硬件定时器计时的方式测量脉宽，可以使用终端(uart1)查看结果
 *					另外，重放过程使用硬PWM输出38K载波，通断方式输出遥控编码
 *
 *	
 * @author			dy@wifi.io
*/




#include "include.h"


#define IR_INPUT	WIFIIO_GPIO_01
#define IR_OUTPUT	WIFIIO_GPIO_03
#define IR_OUTPUT_PWM WIFIIO_PWM_09CH1

#define IR_SET_INPUT api_io.init(IR_INPUT, IN_PULL_UP)
#define IR_SET_OUTPUT api_io.init(IR_OUTPUT, OUT_OPEN_DRAIN_PULL_UP)

#define IR_IN_DAT  api_io.get(IR_INPUT)


#define IR_OUT_LOW api_pwm.t_set( IR_OUTPUT_PWM, 120)
#define IR_OUT_HIGH api_pwm.t_set( IR_OUTPUT_PWM, 0)





u16_t ir_high_elapsed(void)
{
	api_tim.tim6_start(65535);
	while(IR_IN_DAT && !api_tim.tim6_is_timeout()){};
	return api_tim.tim6_count();
}

u16_t ir_low_elapsed(void)
{
	api_tim.tim6_start(65535);
	while(!IR_IN_DAT && !api_tim.tim6_is_timeout()){};
	return api_tim.tim6_count();
}

void ir_replay(u16_t ts[], size_t ts_len)
{
	int i;
	IR_OUT_HIGH;

	for(i = 0; i < ts_len; i++){
		if(i%2) IR_OUT_HIGH;
		else IR_OUT_LOW;

		api_tim.tim6_poll(ts[i]);
	}


	IR_OUT_HIGH;
}

////////////////////////////////////////
//每一个addon都有main，该函数在加载后被运行
//若返回 非 ADDON_LOADER_GRANTED 将导致main函数返回后addon被卸载
////////////////////////////////////////

int main(int argc, char* argv[])
{
	int i;
	u16_t ts[1024];
	size_t ts_len = 0;

	IR_SET_INPUT;	//初始化引脚
	api_tim.tim6_init(1000000);	//初始化定时器6 1us为单位
//	api_pwm.init(IR_OUTPUT_PWM, 6, 263, 130);	//0.1us 8/26.3us 
	api_pwm.init(IR_OUTPUT_PWM, 6, 462, 150);	//道理上 pwm输出38K 应该是26.3us 但是 实际测试 这个 46.2更加灵敏一些，使用者可以对比测试一下。有示波器的朋友 也可以测量一下

	api_pwm.start(IR_OUTPUT_PWM);
	IR_OUT_HIGH;


	LOG_INFO("IREPLAY: record starting...\r\n");

	while(1){
		u16_t t = 0;
		ts_len = 0;

		//一直等待低电平
		while(IR_IN_DAT){};

/*
		//先等待引导码
		if((t = ir_low_elapsed()) < 1000)	//有效低电平小于 4500us
			continue;	//再等
		else if(t > 10000)	//不像是引导码 有效低电平大于 10ms
			continue;	//再等
*/

		t = ir_low_elapsed();
		ts[ts_len++] = t;

/*
		if((t = ir_high_elapsed()) < 1000)	//有效低电平小于 4000us
			continue;	//再等
		else if(t > 10000)	//不像是引导码 有效低电平大于 10ms
			continue;	//再等
*/
		t = ir_high_elapsed();


		//api_console.printf();

	//读取编码
		while(1){
			ts[ts_len++] = t;
			if((t = ir_low_elapsed()) > 2000)	//有效电平小于 1ms
				break;
			else if(t < 100)
				break;

			ts[ts_len++] = t;
			if((t = ir_high_elapsed()) > 2000)	//有效电平小于 1ms
				break;
			else if(t < 100)
				break;

			if(ts_len > NELEMENTS(ts)-10)	//容器有限
				break;
		}


		if(ts_len < 10)continue;	//太短了

		api_console.printf("\r\n");

		for(i = 0; i < ts_len; i++)
			api_console.printf("%d,", i%2?((int)ts[i]):-((int)ts[i]));

		api_console.printf("\r\n");

		_tick_sleep(5000);


		for(i = 0; i < 10; i++){
			ir_replay(ts, ts_len);
			_tick_sleep(20);
		}


		api_console.printf("\r\n");
	}

//	return ADDON_LOADER_GRANTED;	//保持addon在ram中
//err_exit:
	return ADDON_LOADER_ABORT;
}




