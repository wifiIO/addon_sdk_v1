
/**
 * @file			dollcatch.c
 * @brief			wifiIO抓娃娃机程序
 *	这个是wifi.io体验区抓娃娃机的addon源代码，http://wifi.io/demos.php
 *	wifiIO模组通过六个IO口控制六组继电器实现对原先三个双刀三掷开关的替代，实现了将娃娃机改造成为云端操控的目的
 * @author			dy@wifi.io
*/



#include "include.h"

#define IO1	WIFIIO_GPIO_17
#define IO2	WIFIIO_GPIO_18
#define IO3	WIFIIO_GPIO_19
#define IO4	WIFIIO_GPIO_20
#define IO5	WIFIIO_GPIO_21
#define IO6	WIFIIO_GPIO_22

#define STEER_Y_UP()	{api_io.low(IO1);api_io.high(IO2);}
#define STEER_Y_DN()	{api_io.high(IO1);api_io.low(IO2);}
#define STEER_Y_MD()	{api_io.low(IO1);api_io.low(IO2);}
#define STEER_X_RT()	{api_io.low(IO3);api_io.high(IO4);}
#define STEER_X_LF()	{api_io.high(IO3);api_io.low(IO4);}
#define STEER_X_MD()	{api_io.low(IO3);api_io.low(IO4);}
#define STEER_Z_BK()	{api_io.low(IO5);api_io.high(IO6);}
#define STEER_Z_FT()	{api_io.high(IO5);api_io.low(IO6);}
#define STEER_Z_MD()	{api_io.low(IO5);api_io.low(IO6);}

char new_cmd = '\0', cmd_flag = 0;
u8_t U_times = 0, D_times = 0;

u32_t last_U_tick = 0, last_D_tick = 0;
/*
命令格式: 
{
	"method":"dollcatch.steer",
	"params":"U"\"D" \"L"\"R" \"F"\"B"
}
*/
int __ADDON_EXPORT__ JSON_DELEGATE(steer)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	char steer[4];

	//下面从json中提取命令字字符串
	if(JSMN_STRING != pjn->tkn->type || 
	     STATE_OK != jsmn.tkn2val_str(pjn->js, pjn->tkn, steer, sizeof(steer), NULL)){	//pjn->tkn 指向json中params字段的名值对，使用tkn2val_str()从中提取string类型的value
		ret = STATE_ERROR;	err_msg = "Params error."; 
		LOG_WARN("dollcatch:%s.\r\n", err_msg);
		goto exit_err;
	}

	if('U' == steer[0] || 'D' == steer[0] || 'R' == steer[0] || 'L' == steer[0] || 'F' == steer[0] || 'B' == steer[0]){
		new_cmd = steer[0];	//更新命令字，第一个字符就是命令
		cmd_flag = 1;




//	计次限制
		if('U' == new_cmd){
			if(0 == U_times){	//第一次上升
				U_times ++;
			}
			else{	//一直上升
				U_times ++;
				if(U_times > 3)	//超过2s 就忽略新命令
					cmd_flag = 0;	
			}
		}
		else
			U_times = 0;


		if('D' == new_cmd){
			if(0 == D_times){	//第一次下降
				D_times++;
			}
			else{	//一直下降
				D_times++;
				if(D_times > 3)	//超过2s 就忽略新命令
					cmd_flag = 0;
			}
		}
		else
			D_times = 0;



/*	计时限制
		if('U' == new_cmd){
			if(0 == last_U_tick){	//第一次上升
				last_U_tick = api_os.tick_now();
			}
			else{	//一直上升
				if(api_os.tick_elapsed(last_U_tick) > 2000)	//超过2s 就忽略新命令
					cmd_flag = 0;	
			}
		}
		else
			last_U_tick = 0;


		if('D' == new_cmd){
			if(0 == last_D_tick){	//第一次下降
				last_D_tick = api_os.tick_now();
			}
			else{	//一直下降
				if(api_os.tick_elapsed(last_D_tick) > 2000)	//超过2s 就忽略新命令
					cmd_flag = 0;
			}
		}
		else
			last_D_tick = 0;

*/

	}
	else {
		ret = STATE_ERROR;
		err_msg = "Params invalid.";
		LOG_WARN("dollcatch:%s.\r\n", err_msg);
		goto exit_err;
	}

	if(ack)jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}

int main(int argc, char* argv[])
{
	int delay = 1000;
	api_io.init(WIFIIO_GPIO_17, OUT_PUSH_PULL);
	api_io.init(WIFIIO_GPIO_18, OUT_PUSH_PULL);
	api_io.init(WIFIIO_GPIO_19, OUT_PUSH_PULL);
	api_io.init(WIFIIO_GPIO_20, OUT_PUSH_PULL);
	api_io.init(WIFIIO_GPIO_21, OUT_PUSH_PULL);
	api_io.init(WIFIIO_GPIO_22, OUT_PUSH_PULL);


	while(1){
		STEER_Y_MD();			//三个方向的继电器同时关闭
		STEER_X_MD();
		STEER_Z_MD();
		delay = 100;	//延时100ms

		if(cmd_flag){	//有命令才处理
			LOG_INFO("dollcatch.steer %c.\r\n", new_cmd);
			switch(new_cmd){
				case 'U':	STEER_Y_UP();break;
				case 'D':	STEER_Y_DN();break;
				case 'R':	STEER_X_RT();break;
				case 'L':	STEER_X_LF();break;
				case 'F':	STEER_Z_FT();break;
				case 'B':	STEER_Z_BK();break;
				default:break;
			}
			delay = 1000;	//延时1s
			cmd_flag = 0;	//该命令被消费掉
		}
		api_os.tick_sleep(delay);		//命令执行1s，空闲等候100ms
	}
	return ADDON_LOADER_ABORT;	//若程序结束 就无需驻留内存
}

