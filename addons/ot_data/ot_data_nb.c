



#include "include.h"


#define  NB_NAME "ot_data"




typedef struct {
	u8_t temp;
	u8_t humi;
}ot_data_opt_t;





#define DHT11_INPUT api_io.init(WIFIIO_GPIO_24, IN_PULL_UP)
#define DHT11_OUTPUT api_io.init(WIFIIO_GPIO_24, OUT_OPEN_DRAIN_PULL_UP)

#define DHT11_OUT_LOW api_io.low(WIFIIO_GPIO_24)
#define DHT11_OUT_HIGH api_io.high(WIFIIO_GPIO_24)
#define DHT11_IN_DAT  api_io.get(WIFIIO_GPIO_24)



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




int ot_data_timer(nb_info_t* pnb, nb_timer_t* ptmr, void *ctx)
{
	int ret = STATE_OK;
	ot_data_opt_t *dev_opt = (ot_data_opt_t*) api_nb.info_opt(pnb);
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
		LOG_WARN("dht11:%s %d.\r\n", "read time out.", ret);
		goto exit_err;
	}

	if(chk != temp_h+temp_l+humi_h+humi_l){
		ret = STATE_ERROR;
		LOG_WARN("dht11:%s.\r\n", "checksum err.");
		goto exit_err;
	}




	if(humi_h != dev_opt->humi){
		char json[256];
		u32_t n = 0;
		n += _snprintf(json+n, sizeof(json)-n, "{\"method\":\"data\",\"params\":{\"value\":%u,\"tags\":[\"humidity\"]}}", humi_h); 
		api_ot.notify(json, n);
		dev_opt->humi = humi_h;
		LOG_INFO("ot_data: humidity update %u.\r\n", dev_opt->humi);
	}

	if(temp_h != dev_opt->temp){
		char json[256];
		u32_t n = 0;
		n += _snprintf(json+n, sizeof(json)-n, "{\"method\":\"data\",\"params\":{\"value\":%u,\"tags\":[\"temperature\"]}}", temp_h); 
		api_ot.notify(json, n);
		dev_opt->temp =  temp_h;
		LOG_INFO("ot_data: temperature update %u.\r\n", dev_opt->temp);
	}



exit_err:
	return 1000;	//每秒检查一次

	return NB_TIMER_RELEASE;
}






////////////////////
//三个nb框架调用回调
////////////////////


	///////////////////////////
	//进入nb消息循环之前被调用
	///////////////////////////
 int enter(nb_info_t* pnb)
 {

	DHT11_OUTPUT;	//初始化引脚
	api_tim.tim6_init(100000);	//初始化定时器6 10us为单位

	api_nb.timer_attach(pnb, 1000, ot_data_timer, NULL, 0);

	//////////////////////////////////////////////////////
	//返回STATE_OK之外的值，将不进入消息循环，直接退出程序
	//////////////////////////////////////////////////////
 	return STATE_OK;
 }
 
	///////////////////////////
	//nb接收到退出消息时被调用
	///////////////////////////
 int before_exit(nb_info_t* pnb)
 {
 	return STATE_OK;
 }
 
	///////////////////////////
	//退出nb消息循环之后被调用
	///////////////////////////
 int exit(nb_info_t* pnb)
 {
 	return STATE_OK;
 }

////////////////////
//回调调用组合成结构
////////////////////

static const nb_if_t nb_ot_data_if = {enter, before_exit, exit};




////////////////////////////////////////
//每一个addon都有main，该函数在加载后被运行
// 这里适合:
// addon自身运行环境检查；
// 初始化相关数据；

//若返回 非 ADDON_LOADER_GRANTED 将导致函数返回后
//被卸载

//该函数的运行上下文 是wifiIO进程
////////////////////////////////////////

int main(int argc, char* argv[])
{
	nb_info_t* pnb;

	if(NULL != api_nb.find(NB_NAME)){		//判断服务是否已经运行
		LOG_WARN("main:Service exist,booting abort.\r\n");
		goto err_exit;
	}


	////////////////////////////////////////
	//这里我们建立一个nb框架程序的进程，
	//名称为NB_NAME 
	////////////////////////////////////////


	if(NULL == (pnb = api_nb.info_alloc(sizeof(ot_data_opt_t))))	//申请nb结构
		goto err_exit;

	ot_data_opt_t *opt = (ot_data_opt_t*) api_nb.info_opt(pnb);
	opt->temp= 0;
	opt->humi = 0;

	api_nb.info_preset(pnb, NB_NAME, &nb_ot_data_if);	//填充结构

	if(STATE_OK != api_nb.start(pnb)){		//引导进程
		LOG_WARN("main:Start error.\r\n");
		api_nb.info_free(pnb);
		goto err_exit;
	}

	LOG_INFO("main:Service starting...\r\n");


	return ADDON_LOADER_GRANTED;
err_exit:
	return ADDON_LOADER_ABORT;
}






