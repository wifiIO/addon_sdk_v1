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
	"method":"schedule",
	"params":[
		[val, time, period, 1],
		...
		[val, time, period, 2,3],
	]
}

val: 亮度 0~99
time: 延时动作时间，单位s 0~0xFFFFFFFF
period: 渐变时间 单位s 0~60

示例：
开关灯 1 2 3
[
	[99/0, 0, 0, 1,2,3],
]

开关灯，3s渐变 1 2
[
	[99/0, 0, 3, 1,2],
]


10s后开关灯，3s渐变 1 2
[
	[99/0, 10, 3, 1,2],
]


*/

#define  NB_NAME "lighting"
#define BRIGHTNESS(lv) (0.11f*math.pow(1.07f,lv))	//google "y = 0.11*1.07^x"



typedef struct{
	u8_t dev;
	wifiIO_pwm_t pwm_dev;
	u8_t Vdest;	//目标值

	u32_t dT;		//延时绝对时间
	u32_t dt;		//渐变绝对时间

}timer_opt_lighting_t;

typedef struct {
	u8_t lv[4];
}lighting_opt_t;



///////////////////////////////////////////
// 该函数为定时器函数，在预定时间被lighting
// 进程调用。
///////////////////////////////////////////

int lighting_timer(nb_info_t* pnb, nb_timer_t* ptmr, void *ctx)
{
	lighting_opt_t *dev_opt = (lighting_opt_t*) api_nb.info_opt(pnb);
	timer_opt_lighting_t* opt = (timer_opt_lighting_t*)api_nb.timer_opt(ptmr);
	u32_t now = api_os.tick_now();


	if(now < opt->dT){		//等候阶段
		//继续等候
		LOG_INFO("Timer:waiting.\r\n");
		return opt->dT - now;
	}

	else if(now <= opt->dt){	//渐变阶段

		int dt, dv;

//		u16_t v = api_pwm.t_read(opt->pwm_dev);
		u16_t v = dev_opt->lv[opt->dev];	//当前值

		dt = opt->dt - now;	//相差时间
		dv = opt->Vdest - v;	//相差值


		api_console.printf(dv>0?"+":"-");

		if(dv == 0)	//已经完成
			//释放该timer
			return NB_TIMER_RELEASE;

		else	if(dt <= 100){	//时间差小于100ms

			dev_opt->lv[opt->dev] = v+dv;
			api_pwm.t_set(opt->pwm_dev, BRIGHTNESS(dev_opt->lv[opt->dev]));	//完成

			//释放该timer
			return NB_TIMER_RELEASE;
		}

		else{
			int div = 1, _dv = ABS(dv);

			//这里根据渐变时间 和 差值 计算一个步进时间和步进值

			//步进时间须大于100ms
			while(dt/div > 100 && _dv/div >= 1)div++;
			div--;

			//更新当前值

			dev_opt->lv[opt->dev] = v+dv/div;
			api_pwm.t_set(opt->pwm_dev, BRIGHTNESS(dev_opt->lv[opt->dev]));	//完成


			//指定下一次timer定时时间
			return dt/div;
		}

	}

	else{	//特殊原因，超时
		api_console.printf("#");
		LOG_INFO("Timer:%u->%u->%u.(%u)#\r\n",opt->dT,opt->dt,now, opt->Vdest);

		dev_opt->lv[opt->dev] = opt->Vdest;
		api_pwm.t_set(opt->pwm_dev, BRIGHTNESS(dev_opt->lv[opt->dev]));	//直接设置完成值


		//释放该timer
		return NB_TIMER_RELEASE;
	}		


	return NB_TIMER_RELEASE;
}

////////////////////////////////////////
//该函数在lighting进程运行，实现 timer操作
////////////////////////////////////////
int timer_adjest_safe(nb_info_t*pnb, nb_invoke_msg_t* msg)
{
	//提取命令发布者的意图
	timer_opt_lighting_t* opt = api_nb.invoke_msg_opt(msg);

	int wait = opt->dT - api_os.tick_now();	//先计算还需要等候的时钟数
	if(wait < 0)wait = 0;


	//下面安排合适的timer来处理对某个灯具的控制
	nb_timer_t* ptmr;

	//可能已经安排，该timer在等待中，定位该timer
	ptmr = api_nb.timer_by_ctx(pnb, (void*)((u32_t)opt->dev));	


	if(NULL == ptmr){	//若没有就申请新的
		LOG_INFO("lighting:tmr alloced.\r\n");
		ptmr = api_nb.timer_attach(pnb, wait, lighting_timer, (void*)((u32_t)opt->dev), sizeof(timer_opt_lighting_t));	//或者alloc
	}

	
	else{	//若有则 重新定义其参数
		LOG_INFO("lighting:tmr restarted.\r\n");
		api_nb.timer_restart(pnb, wait, lighting_timer);
	}

	//timer附带的额外数据
	timer_opt_lighting_t* tmr_opt = (timer_opt_lighting_t*)api_nb.timer_opt(ptmr);
	string.memcpy(tmr_opt, opt, sizeof(timer_opt_lighting_t));	//将需要的参数 赋值给timer

	return STATE_OK;
}







////////////////////////////////////////
//该函数上下文并非 lighting进程
//无法保证无竞争，因此使用nb框架提供的
//invoke机制，向lighting进程"注入"运行函数
////////////////////////////////////////

int lighting_pwm_arrange(nb_info_t* pnb, int dev, u8_t val, u32_t time, u8_t period)
{

	LOG_INFO("DELEGATE:dev%u val:%u,time:%u,period:%u.\r\n",dev,val,time,period);

	//申请一次invoke过程需要的数据(消息)
	nb_invoke_msg_t* msg = api_nb.invoke_msg_alloc(timer_adjest_safe, sizeof(timer_opt_lighting_t), NB_MSG_DELETE_ONCE_USED);
	timer_opt_lighting_t* opt = api_nb.invoke_msg_opt(msg);

	//填写相应信息
	opt->dev = dev;
	if(0 == dev){
		opt->pwm_dev = WIFIIO_PWM_05CH1;
	}
	else	if(1 == dev){
		opt->pwm_dev = WIFIIO_PWM_05CH2;
	}
	else	if(2 == dev){
		opt->pwm_dev = WIFIIO_PWM_09CH1;
	}
	else	if(3 == dev){
		opt->pwm_dev = WIFIIO_PWM_09CH2;
	}
	opt->Vdest = val;

	u32_t now = api_os.tick_now();
	opt->dT = now + time*1000;
	opt->dt = opt->dT + period*1000;

	//发送给lighting进程 使其运行 timer_adjest_safe 这个函数
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
// 接口名称为 lighting.schedule
////////////////////////////////////////

int JSON_RPC(schedule)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	nb_info_t* pnb;
	if(NULL == (pnb = api_nb.find(NB_NAME))){		//判断服务是否已经运行
		ret = STATE_ERROR;
		err_msg = "Service unavailable.";
		LOG_WARN("lighting:%s.\r\n", err_msg);
		goto exit_err;
	}

	LOG_INFO("DELEGATE lighting.schedule.\r\n");


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
		u8_t val,period;
		u32_t time;
	
		if(JSMN_ARRAY != j->type || j->size <= 3){
			ret = STATE_PARAM;
			err_msg = "Cmd invalid.";
			LOG_WARN("lighting:%s. %u %u\r\n", err_msg, j->type, j->size);
			goto exit_err;
		}
		dev_num = j->size - 3;	//根据数组单元数量 得到pwm控制单元的数量
		j += 1;	//skip [


		jsmn.tkn2val_u8(pjn->js, j, &val);	//第1个单元是 val
		j++;
		jsmn.tkn2val_uint(pjn->js, j, &time);	//第2个单元是 time
		j++;
		jsmn.tkn2val_u8(pjn->js, j, &period);	//第3个单元是 period
		j++;


		//下面对每个pwm发布命令
		while(dev_num--){
			u8_t dev;
			jsmn.tkn2val_u8(pjn->js, j, &dev);
			lighting_pwm_arrange(pnb, dev, val, time, period);
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


////////////////////
//三个nb框架调用回调
////////////////////


	///////////////////////////
	//进入nb消息循环之前被调用
	///////////////////////////
 int enter(nb_info_t* pnb)
 {
	 api_pwm.init(WIFIIO_PWM_05CH1, 600, 100, 0);
	 api_pwm.init(WIFIIO_PWM_05CH2, 600, 100, 0);
	 api_pwm.init(WIFIIO_PWM_09CH1, 600, 100, 0);
	 api_pwm.init(WIFIIO_PWM_09CH2, 600, 100, 0);


	lighting_opt_t *opt = (lighting_opt_t*) api_nb.info_opt(pnb);

	 api_pwm.init(WIFIIO_PWM_05CH1, 600, 100, BRIGHTNESS(opt->lv[0]));
	 api_pwm.init(WIFIIO_PWM_05CH2, 600, 100, BRIGHTNESS(opt->lv[1]));
	 api_pwm.init(WIFIIO_PWM_09CH1, 600, 100, BRIGHTNESS(opt->lv[2]));
	 api_pwm.init(WIFIIO_PWM_09CH2, 600, 100, BRIGHTNESS(opt->lv[3]));

	 api_pwm.start(WIFIIO_PWM_05CH1);
	 api_pwm.start(WIFIIO_PWM_05CH2);
	 api_pwm.start(WIFIIO_PWM_09CH1);
	 api_pwm.start(WIFIIO_PWM_09CH2);
	LOG_INFO("lighting:PWM initialized.\r\n");

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

static const nb_if_t nb_lighting_if = {enter, before_exit, exit};





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


	if(NULL == (pnb = api_nb.info_alloc(sizeof(lighting_opt_t))))	//申请nb结构
		goto err_exit;

	lighting_opt_t *opt = (lighting_opt_t*) api_nb.info_opt(pnb);
	opt->lv[0] = 0;
	opt->lv[1] = 0;
	opt->lv[2] = 0;
	opt->lv[3] = 0;


	api_nb.info_preset(pnb, NB_NAME, &nb_lighting_if);	//填充结构


	if(STATE_OK != api_nb.start(pnb)){		//进入框架
		LOG_WARN("main:Start error.\r\n");
		api_nb.info_free(pnb);
		goto err_exit;
	}

	LOG_INFO("main:Service starting...\r\n");


	return ADDON_LOADER_GRANTED;
err_exit:
	return ADDON_LOADER_ABORT;
}




//httpd接口  输出 json
/*
GET http://192.168.1.105/logic/wifiIO/invoke?target=lighting.status

{"state":loaded}

*/


int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	nb_info_t* pnb;
	char buf[128];
	int n, size = sizeof(buf);

	if(NULL == (pnb = api_nb.find(NB_NAME))){		//判断服务是否已经运行
		goto exit_noload;
	}

	lighting_opt_t *opt = (lighting_opt_t*) api_nb.info_opt(pnb);


	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"loaded\",\"r\":%u,\"g\":%u,\"b\":%u}", opt->lv[1], opt->lv[2], opt->lv[3]);
	return  consumer(ctx, (u8_t*)buf, n);

exit_noload:
	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"not loaded\"}");

	return  consumer(ctx, (u8_t*)buf, n);
}






