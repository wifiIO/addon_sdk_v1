
/**
 * @file			app_stcps.c
 * @brief			串口转tcp服务器
 *	
 *	
 * @author			yizuoshe@gmail.com
*/

#include "include.h"


#define STCPS_LOADMAX_SIZE_MAX 1460


typedef struct add_stcps_info{
	u8_t pkt_buf[STCPS_LOADMAX_SIZE_MAX];
	u16_t pkt_buf_len;

	u16_t hold_time;
	nb_timer_t *tmr;

	serial_cfg_t serial_cfg;

	u32_t statistic_up_serial_bytes;
	u32_t statistic_down_serial_bytes;
}add_stcps_info_t;


typedef struct add_stcps_msg{
	u16_t len;
}add_stcps_msg_t;

typedef struct add_stcps_session{
	void *xxx;
}add_stcps_session_t;


static int single_session_send(nb_session_t*sess, void* args)
{
	tcps_info_t *tcps;
	add_stcps_info_t* stcps;

	tcps = (tcps_info_t*)api_nb.sess_owner(sess);

	stcps = (add_stcps_info_t*)api_tcps.info_opt(tcps);
	if(STATE_OK != api_tcps.sess_send((tcps_session_t*)sess, (const void*)stcps->pkt_buf, (int)stcps->pkt_buf_len))
		api_tcps.sess_abort((tcps_session_t*)sess);

	//更新定时器
	api_tcps.sess_keepalive(tcps, (tcps_session_t*)sess);

	return ITERATOR_CONTINUE;	//返回非零表示中止迭代，跳出
}

static void all_session_send(tcps_info_t* tcps)
{
	api_nb.sess_foreach((nb_info_t*)tcps, (fp_iterator)single_session_send, NULL);	
}





//hold time定时检查
static int stcps_timer_holdtime_check(nb_info_t* nb, nb_timer_t* ptmr, void* ctx)
{
	int slen = 0;
	tcps_info_t* tcps = (tcps_info_t*)nb;
	add_stcps_info_t* stcps = (add_stcps_info_t*)api_tcps.info_opt(tcps);

	//检查串口数据
	if(0 == (slen = api_serial.peek(SERIAL2))){
//		stcps->last_peek = 0;	//清空
		goto TIMER_EXIT_NULL;
	}

	//没有连接，放弃串口数据
	if(0 == api_nb.sess_count((nb_info_t*)tcps)){
		api_serial.clean(SERIAL2);
//		stcps->last_peek = 0;	//清空
		goto TIMER_EXIT_CLEAN;
	}

	//如果和上次不一样，那么说明在holdtime时间段内有数据进入，不发送
//	if(stcps->last_peek != slen){
//		stcps->last_peek = slen;
//		goto TIMER_EXIT_SKIP;
//	}

	//有数据则发送之
	do{
		//int send_len = MIN(slen, stcps->load_max);
		int send_len = MIN(slen, sizeof(stcps->pkt_buf));

		//加载
		api_serial.read(SERIAL2, stcps->pkt_buf, send_len);
		stcps->pkt_buf_len = send_len;

		//发送
		all_session_send(tcps);
		LOG_INFO("sts(spi no %u).\r\n", send_len);


		slen -= send_len;

		stcps->statistic_up_serial_bytes += send_len;
	}while(slen);


//	stcps->last_peek = slen;

//TIMER_EXIT_SKIP:
TIMER_EXIT_CLEAN:
TIMER_EXIT_NULL:

	//1小心！
	//2这是一个回调函数，返回值标识是否接续定时，
	//2如果不需要再定计时，直接返回0即可

	//重新安装
	return stcps->hold_time;

}




static int tcps_stcps_cb_enter(tcps_info_t* tcps)
{
	int ret = STATE_OK;
	add_stcps_info_t* stcps = api_tcps.info_opt(tcps);

	//开启串口
	api_serial.open(SERIAL2);
	api_serial.ctrl(SERIAL2, SERIAL_CTRL_SETUP, &(stcps->serial_cfg));	//配置


	if(stcps->hold_time > 0){
		stcps->tmr = api_nb.timer_attach((nb_info_t*)tcps, stcps->hold_time, stcps_timer_holdtime_check, NULL, 0);
		ARCH_ASSERT("",NULL != stcps->tmr);
	}

	return ret;

}




static int tcps_stcps_cb_exit(tcps_info_t* tcps)
{

	//关闭串口
	api_serial.close(SERIAL2);


	return STATE_OK;
}


static int tcps_stcps_cb_recv(tcps_info_t* tcps, tcps_session_t* t_session)
{
	int ret = STATE_OK;
	net_pkt_t* pkt;
	add_stcps_info_t* stcps = (add_stcps_info_t*)api_tcps.info_opt(tcps);

	ret = api_tcps.sess_recv(t_session, &pkt);
	if(ret != STATE_OK){
		LOG_WARN("stcps: recv err(%d).\r\n", ret);
		return ret;
	}

	do{
		//net_addr_t  *paddr;
		//u16_t peer_port;
		u16_t pkt_len = 0;
		u8_t *pkt_ptr = NULL; 

		if(STATE_OK != api_net.pkt_data(pkt, (void**)&pkt_ptr, &pkt_len) || pkt_len == 0)//
			break;

		//paddr = api_net.pkt_faddr(pkt);
		//peer_port = api_net.pkt_fport(pkt);


		if(pkt_len != 0){
			int rlen = pkt_len;
			//串口发送
			do{
				int n;
				n = api_serial.write(SERIAL2,  (const void*)pkt_ptr, rlen);
				rlen -= n; pkt_ptr += n;
				if(rlen)api_os.tick_sleep(1);
			}while(rlen);

			LOG_INFO("stc(ni so %u).\r\n", pkt_len);

			stcps->statistic_down_serial_bytes += pkt_len;
		}



	}while(api_net.pkt_next(pkt) >= 0);

	api_net.pkt_free(pkt);


	return ret;
}



static const tcps_if_t tcps_stcps_if = {
		tcps_stcps_cb_enter,
		tcps_stcps_cb_exit,
		NULL, //tcps_stcps_cb_connect,
		NULL, //tcps_stcps_cb_disconnect,
		tcps_stcps_cb_recv
	};










/*
json 转 serial config

{
		"baud":115200,		//数值型 1200~115200
		"bits":"8bit",		//"8bit" "9bit"
		"sbits":"0.5bit",	//"0.5bit" "1bit" "1.5bit" "2bit"
		"parity":"none",	//"none" "odd" "even"
		"rs485":false
}

*/

static int serial_json_cfg(serial_cfg_t* cfg, const char* js, size_t js_len, jsmntok_t* tk)
{
	char str[32];
	u32_t u32_tmp;

	if(STATE_OK != jsmn.key2val_uint(js, tk, "baud", &cfg->baudrate)){
		LOG_WARN("Default baudrate.\r\n");
		cfg->baudrate = 115200;	//default
	}

	if(STATE_OK != jsmn.key2val_str(js, tk, "bits", str,  sizeof(str), (size_t*)&u32_tmp)){
		LOG_WARN("Default bits.\r\n");
		cfg->bits = 0;
	}
	else{
		if(str[0] == '9')cfg->bits = 1; else cfg->bits = 0;
	}

	if(STATE_OK != jsmn.key2val_str(js, tk, "sbits", str,  sizeof(str), (size_t*)&u32_tmp)){
		LOG_WARN("Default sbits.\r\n");
		cfg->sbits = 1;
	}
	else{
		 //"0.5bit" "1bit" "1.5bit" "2bit"
		if(0 == _strncmp(str, "0.5bit", u32_tmp))
			cfg->sbits = 0;
		else if(0 == _strncmp(str, "1.5bit", u32_tmp))
			cfg->sbits = 2;
		else if(0 == _strncmp(str, "2bit", u32_tmp))
			cfg->sbits = 3;
		else 	cfg->sbits = 1;
	}

	if(STATE_OK != jsmn.key2val_str(js, tk, "parity", str,  sizeof(str), (size_t*)&u32_tmp)){
		LOG_WARN("Default parity.\r\n");
		cfg->Parity = 0;
	}
	else{
		 //"none" "even" "odd"
		if(0 == _strncmp(str, "odd", u32_tmp))
			cfg->Parity = 1;
		else if(0 == _strncmp(str, "even", u32_tmp))
			cfg->Parity = 2;
		else 	cfg->Parity = 0;
	}

	if(STATE_OK != jsmn.key2val_bool(js, tk, "rs485", (u8_t*)&u32_tmp)){
		LOG_WARN("Default rs485.\r\n");
		cfg->option = 0;
	}
	else{
		if(TRUE == u32_tmp)cfg->option = 1; else cfg->option = 0;
	}

	return STATE_OK;
}



/* 配置文件cfg.json
{	"enable":true,	//可选
	"net":{"lport":55555}, 
	"serial":{
		"baud":115200,		//数值型 1200~115200
		"bits":"8bit",		//"8bit" "9bit"
		"sbits":"0.5bit",	//"0.5bit" "1bit" "1.5bit" "2bit"
		"parity":"none",	//"none" "odd" "even"
		"rs485":false
	},
	"holdtime":100
}

*/

int main(int argc, char* argv[])
{
	int ret = STATE_OK;
	char* js_buf = NULL;
	size_t js_len;

	tcps_info_t* tcps_inf = NULL;
	add_stcps_info_t* stcps;

	jsmntok_t tk[32];	//180+ bytes


	//读入配置json文件	2048bytes max
	js_buf = api_fs.read_alloc("/app/stcps/cfg.json", 2048, &js_len, 0);
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





	u8_t u8_tmp;

	//enable
	if(STATE_OK == jsmn.key2val_bool(js_buf, tk, "enable",  &u8_tmp) && FALSE == u8_tmp){
		LOG_INFO("stcps disabled, abort.\r\n");
		goto load_abort;
	}


	//下面分配资源 并配置之

	tcps_inf = api_tcps.info_alloc(sizeof(add_stcps_info_t));
	if(NULL == tcps_inf)
		goto load_abort;
	stcps = api_tcps.info_opt(tcps_inf);

	u16_t local_port;
	jsmntok_t *jt;

	//找到net对象
	jt = jsmn.key_value(js_buf, tk, "net");
	if(STATE_OK != jsmn.key2val_u16(js_buf, jt, "lport", &local_port))
		goto load_abort;

	//找到serial对象
	jt = jsmn.key_value(js_buf, tk, "serial");
	serial_json_cfg(&(stcps->serial_cfg), js_buf, js_len, jt);	//json -> serial_cfg_t


	//找到holdtime参数
	if(STATE_OK != jsmn.key2val_u16(js_buf, tk, "holdtime", &(stcps->hold_time)))
		goto load_abort;


	//json解析完毕 释放之
	if(js_buf){
		_free(js_buf);
		js_buf = NULL;
	}

	LOG_INFO("stcps: cfg file parsed...\r\n");


	stcps->pkt_buf_len = 0;



	//上面全部是填充结构体的代码

	//最多3个连接 60s保活
 	api_tcps.info_preset(tcps_inf, "stcps", 3, 60000, local_port, sizeof(add_stcps_session_t), (tcps_if_t*)&tcps_stcps_if);	//设置tcps框架

	//框架建立消息处理，不再退出
	if(STATE_OK != api_tcps.start(tcps_inf))
		goto load_abort;

	return ADDON_LOADER_GRANTED;


load_abort:
	LOG_INFO("stcps: aborting...\r\n");


	if(NULL != tcps_inf)
		api_tcps.info_free(tcps_inf);


	if(NULL != js_buf)
		_free(js_buf);
	return ADDON_LOADER_ABORT;
}






//httpd接口  输出 json
/*

	u32_t statistic_up_serial_bytes;
	u32_t statistic_down_serial_bytes;

GET http://192.168.1.105/logic/wifiIO/invoke?target=stcps.stcps_status

{
	"up_byte":yyyyyyyyyy,
	"dn_byte":nnnnnnnnnn
	"connection":0~3
}

*/

int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	char buf[100];
	int n, size = sizeof(buf);

	tcps_info_t* tcps_inf;
	add_stcps_info_t* stcps;

	tcps_inf = (tcps_info_t*)api_nb.find("stcps");

	//找到nb框架程序
	if(tcps_inf){
		stcps = api_tcps.info_opt(tcps_inf);

		n = 0;
		n += utl.snprintf(buf + n, size-n, "{\"up_byte\":%u,\"dn_byte\":%u,\"connection\":%u}", 
					stcps->statistic_up_serial_bytes, stcps->statistic_down_serial_bytes, api_nb.sess_count((nb_info_t*)tcps_inf));
	}else{
		n = 0;
		n += utl.snprintf(buf + n, size-n, "{\"err\":\"not running.\"}");
	}
	return  consumer(ctx, (u8_t*)buf, n);
}















