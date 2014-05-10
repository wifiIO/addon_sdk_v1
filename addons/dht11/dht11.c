
/**
 * @file			dht11.c
 * @brief			温湿度传感器DHT11测试程序
 *	驱动DHT11，并且提供读取的通用接口。
 *	
 * @author			yizuoshe@gmail.com
*/




#include "include.h"

/*
模型:

命令格式: 
{
	"method":"dht11.read",
	"params":{}
}
返回:
{
	"result":{"humidity":xx, "temperature":xxx},
}


*/

#define DHT11_DATA	WIFIIO_GPIO_01
#define DHT11_INPUT api_io.init(DHT11_DATA, IN_PULL_UP)
#define DHT11_OUTPUT api_io.init(DHT11_DATA, OUT_OPEN_DRAIN_PULL_UP)

#define DHT11_OUT_LOW api_io.low(DHT11_DATA)
#define DHT11_OUT_HIGH api_io.high(DHT11_DATA)
#define DHT11_IN_DAT  api_io.get(DHT11_DATA)



////////////////////////////////////////
//细节的时序流程
////////////////////////////////////////




//拉低18ms 拉高40us 等候dht11拉低 启动采样时序
int dht11_start(void)
{
	DHT11_OUT_LOW;
	api_tim.tim6_poll(2000);		//drive low for 2000x10us > 18ms
	DHT11_OUT_HIGH;
	api_tim.tim6_poll(4);		//40us
	DHT11_INPUT;
	if(0 != DHT11_IN_DAT)return -100;		//dht11 should resp low


	api_tim.tim6_start(10);		//start cnt to 100us, DHT11 keep low 80us max
	while(!DHT11_IN_DAT && !api_tim.tim6_is_timeout());	//wait to high
	if(api_tim.tim6_is_timeout())return -101;


	api_tim.tim6_start(10);		//start cnt to 100us, DHT11 keep high 80us max
	while(DHT11_IN_DAT && !api_tim.tim6_is_timeout());	//wait to low
	if(api_tim.tim6_is_timeout())return -102;

	return STATE_OK;
}


int dht11_read_byte(u8_t* dat)
{
	int i;
	u8_t d = 0;
	for(i = 0; i < 8; i++){
		d <<= 1;

		api_tim.tim6_start(8);		//start cnt to 80us, DHT11 keep low 50us max
		while(!DHT11_IN_DAT && !api_tim.tim6_is_timeout());	//wait to high
		if(api_tim.tim6_is_timeout())return -103;

		api_tim.tim6_start(10);
		while(DHT11_IN_DAT && !api_tim.tim6_is_timeout());	//wait to low
		if(api_tim.tim6_is_timeout())return -104;

		//0:high 28us; 1: high 70us
		if(api_tim.tim6_count() < 5){}	//high 26~28us means 0
		else	d += 1;	//high 70us means 1
	}

	if(dat)*dat = d;
	return STATE_OK;
}

void dht11_stop(void)
{
	DHT11_OUTPUT;
	DHT11_OUT_HIGH;
}

////////////////////////////////////////
//委托接口的定义规则:
// JSON_DELEGATE() 宏为函数添加后缀名称
// __ADDON_EXPORT__ 宏将该符号定义为暴露的
// 参数形式 上 可以输入一个json串，带有一个
// 执行完成后的回调 及其附带上下文
//
// 该函数的运行上下文 是调用该委托接口的进程
// httpd、otd 都可以调用该接口
// 接口名称为 dht11.read
//{
//	"method":"dht11.read",
//	"params":{}
//}

//{
//	"result":{"humidity":xx, "temperature":xxx},
//}
////////////////////////////////////////

int JSON_RPC(read)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;

	LOG_INFO("DELEGATE dht11.read.\r\n");


/*
{
	"method":"dht11.read",
	"params":{}
}
*/

	//----温度高8位== U8T_data_H------
	//----温度低8位== U8T_data_L------
	//----湿度高8位== U8RH_data_H-----
	//----湿度低8位== U8RH_data_L-----
	//----校验 8位 == U8checkdata-----
	u8_t temp_h, temp_l, humi_h, humi_l, chk;


	if(STATE_OK != (ret = dht11_start()) ||
		STATE_OK != (ret = dht11_read_byte(&humi_h))||
		STATE_OK != (ret = dht11_read_byte(&humi_l))||
		STATE_OK != (ret = dht11_read_byte(&temp_h))||
		STATE_OK != (ret = dht11_read_byte(&temp_l))||
		STATE_OK != (ret = dht11_read_byte(&chk)))
		{}


	dht11_stop();

	if(STATE_OK != ret){
		err_msg = "read time out.";
		LOG_WARN("dht11:%s %d.\r\n", err_msg, ret);
		goto exit_err;
	}

	if(chk != temp_h+temp_l+humi_h+humi_l){
		ret = STATE_ERROR;
		err_msg = "checksum err.";
		LOG_WARN("dht11:%s.\r\n", err_msg);
		goto exit_err;
	}


	if(ack){
		char json[128];
		u32_t n = 0;
		n += _snprintf(json+n, sizeof(json)-n, "{\"result\":{\"temperature\":%u.%u,\"humidity\":%u.%u}}",temp_h, temp_l, humi_h, humi_l); 
		jsmn_node_t jn = {json, n, NULL};
		ack(&jn, ctx);	//这里对json请求 回应json
	}
	return ret;


//exit_safe:
	if(ack)
		jsmn.delegate_ack_result(ack, ctx, ret);	
	return ret;

exit_err:
	if(ack)
		jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}




////////////////////////////////////////
//每一个addon都有main，该函数在加载后被运行
//若返回 非 ADDON_LOADER_GRANTED 将导致main函数返回后addon被卸载
////////////////////////////////////////

int main(int argc, char* argv[])
{
	DHT11_OUTPUT;	//初始化引脚
	api_tim.tim6_init(100000);	//初始化定时器6 10us为单位

	LOG_INFO("main: DHT11 starting...\r\n");

	return ADDON_LOADER_GRANTED;	//保持addon在ram中
//err_exit:
//	return ADDON_LOADER_ABORT;
}



