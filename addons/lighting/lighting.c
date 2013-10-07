
/**
 * @file			lighting.c
 * @brief			wifiIO驱动led的程序
 *	
 *	
 * @author			dy@wifi.io
*/





#include "include.h"

#define BRIGHTNESS(lv) (0.11f*math.pow(1.07f,lv))	//google "y = 0.11*1.07^x"

#define RGB1_Z WIFIIO_PWM_05CH1	//IO1
#define RGB1_R WIFIIO_PWM_05CH2	//IO2
#define RGB1_G WIFIIO_PWM_09CH1	//IO3
#define RGB1_B WIFIIO_PWM_09CH2	//IO4

#define RGB2_Z WIFIIO_PWM_04CH1	//IO17
#define RGB2_R WIFIIO_PWM_04CH2	//IO18
#define RGB2_G WIFIIO_PWM_12CH1	//IO19
#define RGB2_B WIFIIO_PWM_12CH2	//IO20


#define LT01 WIFIIO_GPIO_01
#define LT02 WIFIIO_GPIO_02
#define LT03 WIFIIO_GPIO_03
#define LT04 WIFIIO_GPIO_04

#define LT05 WIFIIO_GPIO_17
#define LT06 WIFIIO_GPIO_18
#define LT07 WIFIIO_GPIO_19
#define LT08 WIFIIO_GPIO_20


char id[16];



typedef struct {
	u32_t delay;	//延时(秒)

	wifiIO_pwm_t pwm_dev;
	u8_t Vfrom;	//起始值
	u8_t Vdest;	//目标值
	u8_t halfcycle;	//半周期 秒
	u16_t hc_num;	//半周期数量



	//迭代相关的参数
	u8_t iter_Vhigh;	//大值
	u8_t iter_Vlow;	//小值
	float iter_Vdelta;	//变化值
	float iter_Vcurr;	//当前值
	u32_t iter_change_num;	//变化迭代周期(100ms)
	u32_t iter_delay_num;	//等候迭代周期(100ms)
}pwm_dev_t;

pwm_dev_t dev[8] = {
		{0, WIFIIO_PWM_05CH1, 0, 0, 0, 0, 0, 0},
		{0, WIFIIO_PWM_05CH2, 0, 0, 0, 0, 0, 0},
		{0, WIFIIO_PWM_09CH1, 0, 0, 0, 0, 0, 0},
		{0, WIFIIO_PWM_09CH2, 0, 0, 0, 0, 0, 0},
		{0, WIFIIO_PWM_04CH1, 0, 0, 0, 0, 0, 0},
		{0, WIFIIO_PWM_04CH2, 0, 0, 0, 0, 0, 0},
		{0, WIFIIO_PWM_12CH1, 0, 0, 0, 0, 0, 0},
		{0, WIFIIO_PWM_12CH2, 0, 0, 0, 0, 0, 0},
};

//根据设置计算迭代变化(将所有 iter_ 开头的成员赋值)
void dev_iterate_calculate(pwm_dev_t* dev)
{
	u32_t n;
	float sign = 1.0f;
	//这里 from 和 dest 需要不一样，否则不继续
	if(dev->Vfrom > dev->Vdest){
		dev->iter_Vhigh = (float)dev->Vfrom;
		dev->iter_Vlow = (float)dev->Vdest;
		sign = -1.0f;
	}
	else if(dev->Vfrom < dev->Vdest){
		dev->iter_Vhigh = (float)dev->Vdest;
		dev->iter_Vlow = (float)dev->Vfrom;
		sign = 1.0f;
	}
	else
		return;

	if(dev->halfcycle == 0)
		dev->halfcycle = 1;

	dev->iter_delay_num = dev->delay *10;
	n = dev->halfcycle * 10;		//半周期用秒计数，迭代周期是100ms

	dev->iter_Vdelta = sign * (dev->iter_Vhigh - dev->iter_Vlow)/(float)n;
	dev->iter_change_num = n*dev->hc_num;
	dev->iter_Vcurr = 1.0f*dev->Vfrom;




}


void dev_iterate(pwm_dev_t* dev)
{
	if(dev->iter_delay_num > 0){	//延时阶段
		dev->iter_delay_num--;
		return;
	}

/*
	{
	//	double tmp = 0.0f;
		int i;
		for(i = 0; i < 100; i++){
			dev->iter_Vcurr += dev->iter_Vdelta;
			api_console.printf("tmp:%d \r\n", (int)dev->iter_Vcurr);
		}

	}

*/

	if(dev->iter_change_num > 0){	//如果还有迭代数量 
		u8_t v;

		dev->iter_Vcurr += dev->iter_Vdelta;		//根据当前值计算下一个值

		if(dev->iter_Vcurr > dev->iter_Vhigh || dev->iter_Vcurr < dev->iter_Vlow)
			dev->iter_Vdelta = -dev->iter_Vdelta;

		v = (u8_t)dev->iter_Vcurr;
		if(v > dev->iter_Vhigh)		v = dev->iter_Vhigh;
		if(v < dev->iter_Vlow)		v = dev->iter_Vlow;

		api_pwm.t_set(dev->pwm_dev, BRIGHTNESS(v));
		dev->Vfrom = v;	//更新当前值

		dev->iter_change_num--;

	}
}







/*
{
	"method":"lighting.schedule",
	"params":[
		[val, delay, halfcycle_period, halfcycle_num, 1],
		...
		[val, delay, halfcycle_period, halfcycle_num, 2, 3],
	]
}
val:	0~99
delay:	unsigned 32bits	当前延时动作秒数 2592000 max (1 month)
period:	半周期时间(s) 255max
num:	半周期数量（奇数最终状态反转，偶数最终回归原状态）1024 max
最后是编号，8组PWM通过编号，可以任意组合指定

*/
int JSON_RPC(schedule)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	
{
	int ret = STATE_OK;
	char* err_msg = NULL;

//	LOG_INFO("DELEGATE lighting.schedule.\r\n");

	jsmntok_t* jt = pjn->tkn;	//指向命令字数组[[],..[]]

	if(JSMN_ARRAY != jt->type){
		ret = STATE_ERROR;
		err_msg = "Cmd array needed.";
		LOG_WARN("lighting:%s.\r\n", err_msg);
		goto exit_err;
	}

	int cmd_num = jt->size;
	LOG_INFO("%u cmds.\r\n", cmd_num);

	jt += 1; 	//skip [
	while(cmd_num--){
		int dev_num = 0;
		jsmntok_t* j = jt;
		u8_t val,halfcycle;
		u16_t hc_num;
		u32_t delay;
	
		if(JSMN_ARRAY != j->type || j->size <= 4){
			ret = STATE_PARAM;
			err_msg = "Cmd invalid.";
			LOG_WARN("lighting:%s. %u %u\r\n", err_msg, j->type, j->size);
			goto exit_err;
		}
		dev_num = j->size - 4;	//根据数组单元数量 得到pwm控制单元的数量
		j += 1;	//skip [


		jsmn.tkn2val_u8(pjn->js, j, &val);	//第1个单元是 val
		j++;
		jsmn.tkn2val_uint(pjn->js, j, &delay);	//第2个单元是 delay
		j++;
		jsmn.tkn2val_u8(pjn->js, j, &halfcycle);	//第3个单元是 halfcycle
		j++;
		jsmn.tkn2val_u16(pjn->js, j, &hc_num);	//第4个单元是 hc_num
		j++;


		//下面对每个pwm发布命令
		while(dev_num--){
			u8_t dev_id;
			jsmn.tkn2val_u8(pjn->js, j, &dev_id);
			dev_id -= 1;

			dev[dev_id].delay = delay;
			dev[dev_id].Vdest = val;
			dev[dev_id].hc_num = hc_num;
			dev[dev_id].halfcycle = halfcycle;

			LOG_INFO("delay:%u val:%u hc:%u hcn:%u Vnow:%u\r\n", delay, val, halfcycle, hc_num, dev[dev_id].Vfrom);
			dev_iterate_calculate(&dev[dev_id]);
			j++;
		}

		jt = jsmn.next(jt);	//因为每个单位不是primitive对象，因此不能使用+1来切换
	}
	

//exit_safe:
	if(ack)
		jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)
		jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;


	
}





int main(int argc, char* argv[])
{
	int i;
 	 api_pwm.init(WIFIIO_PWM_05CH1, 600, 100, 0);
 	 api_pwm.init(WIFIIO_PWM_05CH2, 600, 100, 0);
 	 api_pwm.init(WIFIIO_PWM_09CH1, 600, 100, 0);
 	 api_pwm.init(WIFIIO_PWM_09CH2, 600, 100, 0);
 	 api_pwm.init(WIFIIO_PWM_04CH1, 600, 100, 0);
 	 api_pwm.init(WIFIIO_PWM_04CH2, 600, 100, 0);
 	 api_pwm.init(WIFIIO_PWM_12CH1, 600, 100, 0);
 	 api_pwm.init(WIFIIO_PWM_12CH2, 600, 100, 0);


	 api_pwm.start(WIFIIO_PWM_05CH1);
	 api_pwm.start(WIFIIO_PWM_05CH2);
	 api_pwm.start(WIFIIO_PWM_09CH1);
	 api_pwm.start(WIFIIO_PWM_09CH2);
	 api_pwm.start(WIFIIO_PWM_04CH1);
	 api_pwm.start(WIFIIO_PWM_04CH2);
	 api_pwm.start(WIFIIO_PWM_12CH1);
	 api_pwm.start(WIFIIO_PWM_12CH2);



/*
	char* js_buf = NULL;
	size_t js_len;
	jsmntok_t tk[16];


	//读入配置json文件	2048bytes max
	js_buf = api_fs.read_alloc("/app/lighting/cfg.json", 2048, &js_len, 0);
	if(NULL == js_buf){
		goto load_abort;
	}


	//parse json
	do{
		jsmn_parser psr;
		jsmn.init(&psr);
		ret = jsmn.parse(&psr, js_buf, js_len, tk, NELEMENTS(tk));
		if(JSMN_SUCCESS != ret){
			LOG_WARN("parse err(%d).\r\n", ret);
			goto load_abort;
		}
		tk[psr.toknext].type= JSMN_INVALID;	//标识尾部
	}while(0);


	//id
	if(STATE_OK != jsmn.key2val_str(js_buf, tk, "id",  id, sizeof(id), NULL)){
		LOG_INFO("Can not get ID, abort.\r\n");
		goto load_abort;
	}

	_free(js_buf);
	js_buf = NULL;


	if(0 == _memcmp(id+3, "A01", 3)){
		thread_a01();
	}
	else if(0 == _memcmp(id+3, "A02", 3)){
		thread_a02();
	}
	else if(0 == _memcmp(id+3, "A03", 3)){
		thread_a03();
	}
	else if(0 == _memcmp(id+3, "A04", 3)){
		thread_a04();
	}
	else if(0 == _memcmp(id+3, "A05", 3)){
		thread_a05();
	}

*/


	while(1){
		for(i = 0; i < NELEMENTS(dev); i++)
			dev_iterate(&dev[i]);

		api_os.tick_sleep(100);	//100ms
	}


	return ADDON_LOADER_ABORT;

/*
load_abort:
	LOG_INFO("cfg err: aborting...\r\n");


	if(NULL != js_buf)
		_free(js_buf);
	return ADDON_LOADER_ABORT;
*/
}




