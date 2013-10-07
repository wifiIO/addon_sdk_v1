/**
 * @file			step_motor.c
 * @brief			wifiIO驱动步进电机
 *	驱动电机型号是28BYJ-48
 *	淘宝有售: http://item.taobao.com/item.htm?id=6895887673
 * @author			dy@wifi.io
*/


#include "include.h"

#define STEPMOTOR_DRIVE_D1 WIFIIO_GPIO_01
#define STEPMOTOR_DRIVE_D2 WIFIIO_GPIO_02
#define STEPMOTOR_DRIVE_D3 WIFIIO_GPIO_03
#define STEPMOTOR_DRIVE_D4 WIFIIO_GPIO_04

/*
模型:

命令格式: 
{
	"method":"stepmotor.go",
	"params":{
		"speed": 500,
		"clockwise": true,
		"steps":4096
	}
}

speed: 速度
clockwise: 是否顺时针
steps: 运动步数

*/

#define  NB_NAME "stepmotor"



typedef struct{
	u8_t clockwise;
	u16_t wait;		//延时
	u32_t steps;
}timer_opt_stepmotor_t;

typedef struct {
	u8_t phase;
}stepmotor_opt_t;




// 预编码 参考:
//http://www.ichanging.org/uln2003-to-drive-relay-and-motor.html
const u8_t phase_bit_map[8] = { 1, 3, 2, 6, 4, 12, 8, 9};	//

static void phase_activate(u8_t phase)
{
	if(phase_bit_map[phase] & 0x01)api_io.high(STEPMOTOR_DRIVE_D1);
	else api_io.low(STEPMOTOR_DRIVE_D1);

	if(phase_bit_map[phase] & 0x02)api_io.high(STEPMOTOR_DRIVE_D2);
	else api_io.low(STEPMOTOR_DRIVE_D2);

	if(phase_bit_map[phase] & 0x04)api_io.high(STEPMOTOR_DRIVE_D3);
	else api_io.low(STEPMOTOR_DRIVE_D3);

	if(phase_bit_map[phase] & 0x08)api_io.high(STEPMOTOR_DRIVE_D4);
	else api_io.low(STEPMOTOR_DRIVE_D4);
}

static void phase_deactivate(void)
{
	api_io.low(STEPMOTOR_DRIVE_D1);
	api_io.low(STEPMOTOR_DRIVE_D2);
	api_io.low(STEPMOTOR_DRIVE_D3);
	api_io.low(STEPMOTOR_DRIVE_D4);
}

static u8_t phase_next(u8_t phase, u8_t clockwise)
{
	phase += sizeof(phase_bit_map);
	if(clockwise) phase++;
	else 	phase--;
	phase = phase % sizeof(phase_bit_map);

	phase_activate(phase);

	return phase;
}




///////////////////////////////////////////
// 该函数为定时器函数，在预定时间被stepmotor
// 进程调用。
///////////////////////////////////////////

static int stepmotor_timer(nb_info_t* pnb, nb_timer_t* ptmr, void *ctx)
{
	stepmotor_opt_t *motor_opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);
	timer_opt_stepmotor_t* tmr_opt = (timer_opt_stepmotor_t*)api_nb.timer_opt(ptmr);


	motor_opt->phase = phase_next(motor_opt->phase, tmr_opt->clockwise);

	tmr_opt->steps --;

	if(tmr_opt->steps == 0 ){
		phase_deactivate();
		return NB_TIMER_RELEASE;
	}
	else
		return tmr_opt->wait;


}

////////////////////////////////////////
//该函数在stepmotor进程运行，实现 timer操作
////////////////////////////////////////
static int invoke_set_step_timer(nb_info_t*pnb, nb_invoke_msg_t* msg)
{
	//提取命令发布者的意图
	timer_opt_stepmotor_t* opt = api_nb.invoke_msg_opt(msg);


	//下面安排合适的timer来处理对某个灯具的控制
	nb_timer_t* ptmr;

	//可能已经安排，该timer在等待中，定位该timer
	ptmr = api_nb.timer_by_ctx(pnb, NULL);	

	if(NULL == ptmr){	//若没有就申请新的
		LOG_INFO("stepmotor:tmr alloced.\r\n");
		ptmr = api_nb.timer_attach(pnb, opt->wait, stepmotor_timer, NULL, sizeof(timer_opt_stepmotor_t));	//或者alloc
	}
	else{	//若有则 重新定义其参数
		LOG_INFO("stepmotor:tmr restarted.\r\n");
		api_nb.timer_restart(pnb, opt->wait, stepmotor_timer);
	}

	//timer附带的额外数据
	timer_opt_stepmotor_t* tmr_opt = (timer_opt_stepmotor_t*)api_nb.timer_opt(ptmr);
	string.memcpy(tmr_opt, opt, sizeof(timer_opt_stepmotor_t));	//将需要的参数 赋值给timer

	return STATE_OK;
}







////////////////////////////////////////
//该函数上下文并非 stepmotor进程
//无法保证无竞争，因此使用nb框架提供的
//invoke机制，向stepmotor进程"注入"运行函数
////////////////////////////////////////

static int stepmotor_movement_arrange(nb_info_t* pnb, u32_t steps, u8_t clockwise, u16_t speed)
{

	LOG_INFO("DELEGATE: steps:%u speed:%u %s.\r\n", steps, speed, clockwise?"clockwise":"anti_clockwise");

	//申请一次invoke过程需要的数据(消息)
	nb_invoke_msg_t* msg = api_nb.invoke_msg_alloc(invoke_set_step_timer, sizeof(timer_opt_stepmotor_t), NB_MSG_DELETE_ONCE_USED);
	timer_opt_stepmotor_t* opt = api_nb.invoke_msg_opt(msg);

	//填写相应信息
	opt->clockwise = clockwise;
	opt->steps = steps;
	opt->wait = 1000/speed;
	if(opt->wait < 1)
		opt->wait = 1;

	if(opt->wait > 1000)
		opt->wait = 1000;

	//发送给stepmotor进程 使其运行 invoke_set_step_timer 这个函数
	api_nb.invoke_start(pnb , msg);
	return STATE_OK;
}






////////////////////////////////////////
// 该函数的运行上下文 是调用该委托接口的进程
// httpd、otd 都可以调用该接口
// 接口名称为 stepmotor.go，类如:
//{
//	"method":"stepmotor.go",
//	"params":{
//		"speed": 500,
//		"clockwise": true,
//		"steps":4096
//	}
//}
////////////////////////////////////////

int JSON_RPC(go)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	nb_info_t* pnb;
	if(NULL == (pnb = api_nb.find(NB_NAME))){		//判断服务是否已经运行
		ret = STATE_ERROR;
		err_msg = "Service unavailable.";
		LOG_WARN("stepmotor:%s.\r\n", err_msg);
		goto exit_err;
	}

	LOG_INFO("DELEGATE stepmotor.go.\r\n");


/*
	{
		"speed": xxx,
		"clockwise": true,
		"steps":1234
	}
*/

	u8_t clockwise = 0;
	u16_t speed = 1;
	u32_t steps = 0;
	
	if(STATE_OK != jsmn.key2val_uint(pjn->js, pjn->tkn, "steps", &steps) ||
		STATE_OK != jsmn.key2val_bool(pjn->js, pjn->tkn, "clockwise", &clockwise) ||
		STATE_OK != jsmn.key2val_u16(pjn->js, pjn->tkn, "speed", &speed)	){

		ret = STATE_ERROR;
		err_msg = "params error .";
		LOG_WARN("stepmotor:%s.\r\n", err_msg);
		goto exit_err;
	}

	
	stepmotor_movement_arrange(pnb, steps, clockwise, speed);


//exit_safe:
	if(ack)
		jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)
		jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}


////////////////////
//三个nb框架调用回调
////////////////////


	///////////////////////////
	//进入nb消息循环之前被调用
	///////////////////////////
static  int enter(nb_info_t* pnb)
 {
//	stepmotor_opt_t *opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);

	 api_io.init(STEPMOTOR_DRIVE_D1, OUT_PUSH_PULL);
	 api_io.init(STEPMOTOR_DRIVE_D2, OUT_PUSH_PULL);
	 api_io.init(STEPMOTOR_DRIVE_D3, OUT_PUSH_PULL);
	 api_io.init(STEPMOTOR_DRIVE_D4, OUT_PUSH_PULL);

	phase_deactivate();
	//phase_activate(phase_bit_map[opt->phase]);

	LOG_INFO("stepmotor:phase initialized.\r\n");

	//////////////////////////////////////////////////////
	//返回STATE_OK之外的值，将不进入消息循环，直接退出程序
	//////////////////////////////////////////////////////
 	return STATE_OK;
 }
 
	///////////////////////////
	//nb接收到退出消息时被调用
	///////////////////////////
static  int before_exit(nb_info_t* pnb)
 {
 	return STATE_OK;
 }
 
	///////////////////////////
	//退出nb消息循环之后被调用
	///////////////////////////
static  int exit(nb_info_t* pnb)
 {
 	return STATE_OK;
 }

////////////////////
//回调调用组合成结构
////////////////////

static const nb_if_t nb_stepmotor_if = {enter, before_exit, exit};





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


	if(NULL == (pnb = api_nb.info_alloc(sizeof(stepmotor_opt_t))))	//申请nb结构
		goto err_exit;

	stepmotor_opt_t *opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);
	opt->phase = 0;

	api_nb.info_preset(pnb, NB_NAME, &nb_stepmotor_if);	//填充结构

	if(STATE_OK != api_nb.start(pnb)){		//引导进程
		LOG_WARN("main:Start error.\r\n");
		api_nb.info_free(pnb);
		goto err_exit;
	}

	LOG_INFO("main:Service starting...\r\n");


	return ADDON_LOADER_ABORT;	//若到这里说明nb框架完全结束了，不需要驻留
//	return ADDON_LOADER_GRANTED;
err_exit:
	return ADDON_LOADER_ABORT;
}




//httpd接口  输出 json
/*
GET http://192.168.1.105/logic/wifiIO/invoke?target=stepmotor.status

{"state":loaded}

*/


int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	nb_info_t* pnb;
	char buf[32];
	int n, size = sizeof(buf);

	if(NULL == (pnb = api_nb.find(NB_NAME))){		//判断服务是否已经运行
		goto exit_noload;
	}

	stepmotor_opt_t *opt = (stepmotor_opt_t*) api_nb.info_opt(pnb);


	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"loaded\", \"phase\":%u}", opt->phase);
	return  consumer(ctx, (u8_t*)buf, n);

exit_noload:
	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"not loaded\"}");

	return  consumer(ctx, (u8_t*)buf, n);
}






