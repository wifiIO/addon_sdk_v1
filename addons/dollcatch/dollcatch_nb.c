/*
	该程序演示如何使用noneblock框架 编写可以被云端或者本地浏览器控制的调光灯应用。
	将实现可以设置灯具定时动作 以及延时渐变的效果。
	本程序利用nb框架中的定时器实现 对灯具的控制，渐变效果也是利用定时器渐进调节实现。
	本程序演示了:
	1、nb框架建立和引导；
	2、通过nb框架的invoke机制 请求函数在另一个进程(上下文)中运行
	3、nb的定时器应用；
	4、pwm控制
	5、json委托接口的使用(json委托接口是httpd和otd程序默认的外部程序接口)
*/


#include "include.h"

/*
模型:

命令格式: 
{
	"method":"dollcatch.steer",
	"params":"U"\"D" \"L"\"R" \"F"\"B"
}

*/

#define  NB_NAME "dollcatch"


//这里定义了我们要使用哪些引脚 以及一些为方便而定义的宏定义

#define IO1	WIFIIO_GPIO_17
#define IO2	WIFIIO_GPIO_18
#define IO3	WIFIIO_GPIO_19
#define IO4	WIFIIO_GPIO_20
#define IO5	WIFIIO_GPIO_21
#define IO6	WIFIIO_GPIO_22


#define STEER_Y_UP()	{api_io.low(IO1);api_io.high(IO2);}
#define STEER_Y_DOWN()	{api_io.high(IO1);api_io.low(IO2);}
#define STEER_Y_MID()	{api_io.low(IO1);api_io.low(IO2);}

#define STEER_X_RIGHT()	{api_io.low(IO3);api_io.high(IO4);}
#define STEER_X_LEFT()	{api_io.high(IO3);api_io.low(IO4);}
#define STEER_X_MID()	{api_io.low(IO3);api_io.low(IO4);}

#define STEER_Z_BACK()	{api_io.low(IO5);api_io.high(IO6);}
#define STEER_Z_FRONT()	{api_io.high(IO5);api_io.low(IO6);}
#define STEER_Z_MID()	{api_io.low(IO5);api_io.low(IO6);}


#define STEER_OP_TIME_LAST_MAX 1000

typedef struct{
	char steer;
}invoke_msg_opt_dollcatch_t;




///////////////////////////////////////////
// 该函数为定时器函数，在预定时间被dollcatch
// 进程调用。
///////////////////////////////////////////

int dollcatch_timer(nb_info_t* pnb, nb_timer_t* ptmr, void *ctx)
{
	if((void*)'R' == ctx || (void*)'L' == ctx)
		STEER_X_MID()
	if((void*)'U' == ctx || (void*)'D' == ctx)
		STEER_Y_MID()
	if((void*)'F' == ctx || (void*)'B' == ctx)
		STEER_Z_MID()
	else{
		STEER_X_MID();
		STEER_Y_MID();
		STEER_Z_MID();
	}
	return NB_TIMER_RELEASE;
}

////////////////////////////////////////
//该函数在dollcatch进程运行，实现 timer操作
////////////////////////////////////////
int timer_adjest_safe(nb_info_t*pnb, nb_invoke_msg_t* msg)
{
	//提取命令发布者的意图
	invoke_msg_opt_dollcatch_t* opt = api_nb.invoke_msg_opt(msg);
	nb_timer_t* ptmr;



	if('U' == opt->steer){
		STEER_Y_UP();
	}
	else if('D' == opt->steer){
		STEER_Y_DOWN();
	}
	else if('R' == opt->steer){
		STEER_X_RIGHT();
	}
	else if('L' == opt->steer){
		STEER_X_LEFT();
	}
	else if('F' == opt->steer){
		STEER_Z_FRONT();
	}
	else if('B' == opt->steer){
		STEER_Z_BACK();
	}
	else {
		return STATE_OK;
	}


	//可能已经安排，该timer在等待中，定位该timer
	ptmr = api_nb.timer_by_ctx(pnb, (void*)((u32_t)opt->steer));	


	if(NULL == ptmr){	//若没有就申请新的
		LOG_INFO("dollcatch:tmr alloced.\r\n");
		ptmr = api_nb.timer_attach(pnb, STEER_OP_TIME_LAST_MAX, dollcatch_timer, (void*)((u32_t)opt->steer), 0);	//或者alloc
	}

	
	else{	//若有则 重新定义其参数
		LOG_INFO("dollcatch:tmr restarted.\r\n");
		api_nb.timer_restart(pnb, STEER_OP_TIME_LAST_MAX, dollcatch_timer);
	}


	return STATE_OK;
}







////////////////////////////////////////
//该函数上下文并非 dollcatch进程
//无法保证无竞争，因此使用nb框架提供的
//invoke机制，向dollcatch进程"注入"运行函数
////////////////////////////////////////

int dollcatch_steer_arrange(nb_info_t* pnb, char steer)
{

	LOG_INFO("DELEGATE:%c.\r\n",steer);

	//申请一次invoke过程需要的数据(消息)
	nb_invoke_msg_t* msg = api_nb.invoke_msg_alloc(timer_adjest_safe, sizeof(invoke_msg_opt_dollcatch_t), NB_MSG_DELETE_ONCE_USED);
	invoke_msg_opt_dollcatch_t* opt = api_nb.invoke_msg_opt(msg);

	opt->steer = steer;

	api_nb.invoke_start(pnb , msg);
	return STATE_OK;
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
// 接口名称为 dollcatch.steer
////////////////////////////////////////

int __ADDON_EXPORT__ JSON_DELEGATE(steer)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	nb_info_t* pnb;
	if(NULL == (pnb = api_nb.find(NB_NAME))){		//判断服务是否已经运行
		ret = STATE_ERROR;
		err_msg = "Service unavailable.";
		LOG_WARN("dollcatch:%s.\r\n", err_msg);
		goto exit_err;
	}

	LOG_INFO("DELEGATE dollcatch.steer.\r\n");


	char steer[4];
	if(JSMN_STRING != pjn->tkn->type || 
	     STATE_OK != jsmn.tkn2val_str(pjn->js, pjn->tkn, steer, sizeof(steer), NULL)){
		ret = STATE_ERROR;
		err_msg = "Params error.";
		LOG_WARN("dollcatch:%s.\r\n", err_msg);
		goto exit_err;
	}


	if('U' == steer[0] || 'D' == steer[0] || 'R' == steer[0] || 'L' == steer[0] || 'F' == steer[0] || 'B' == steer[0]){
		dollcatch_steer_arrange(pnb, steer[0]);
	}
	else {
		ret = STATE_ERROR;
		err_msg = "Params invalid.";
		LOG_WARN("dollcatch:%s.\r\n", err_msg);
		goto exit_err;
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


////////////////////
//三个nb框架调用回调
////////////////////


	///////////////////////////
	//进入nb消息循环之前被调用
	///////////////////////////
 int enter(nb_info_t* pnb)
 {
	 api_io.init(WIFIIO_GPIO_17, OUT_PUSH_PULL);
	 api_io.init(WIFIIO_GPIO_18, OUT_PUSH_PULL);
	 api_io.init(WIFIIO_GPIO_19, OUT_PUSH_PULL);
	 api_io.init(WIFIIO_GPIO_20, OUT_PUSH_PULL);
	 api_io.init(WIFIIO_GPIO_21, OUT_PUSH_PULL);
	 api_io.init(WIFIIO_GPIO_22, OUT_PUSH_PULL);

	 api_io.low(WIFIIO_GPIO_17);
	 api_io.low(WIFIIO_GPIO_18);
	 api_io.low(WIFIIO_GPIO_19);
	 api_io.low(WIFIIO_GPIO_20);
	 api_io.low(WIFIIO_GPIO_21);
	 api_io.low(WIFIIO_GPIO_22);

	LOG_INFO("dollcatch:IO initialized.\r\n");

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

static const nb_if_t nb_dollcatch_if = {enter, before_exit, exit};





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


	if(NULL == (pnb = api_nb.info_alloc(0)))	//申请nb结构
		goto err_exit;

	api_nb.info_preset(pnb, NB_NAME, &nb_dollcatch_if);	//填充结构


	if(STATE_OK != api_nb.start(pnb)){
		LOG_WARN("main:Start error.\r\n");
		api_nb.info_free(pnb);
		goto err_exit;
	}

	LOG_INFO("main:Service starting...\r\n");


	return ADDON_LOADER_GRANTED;
err_exit:
	return ADDON_LOADER_ABORT;
}




