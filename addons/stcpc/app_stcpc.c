
/**
 * @file			app_stcpc.c
 * @brief			串口转tcp客户端
 *	
 *	
 * @author			dy@wifi.io
*/


#include "include.h"



#define STCPC_LOADMAX_SIZE_MAX 1460

#define DOMAIN_SIZE_MAX 64


typedef struct add_stcpc_info{

	char domain[DOMAIN_SIZE_MAX];
	u16_t peer_port;
	net_addr_t peer_ip;
	u8_t pkt_buf[STCPC_LOADMAX_SIZE_MAX];
	//u16_t pkt_buf_len;

	u16_t hold_time;
	nb_timer_t *tmr;

	serial_cfg_t serial_cfg;

	u32_t statistic_up_serial_bytes;
	u32_t statistic_down_serial_bytes;

}add_stcpc_info_t;



static int  tcpc_timer_resolve_connect(tcpc_info_t* tcpc, void* ctx)
{
	int ret = STATE_OK;
	add_stcpc_info_t* stcpc;
	stcpc = (add_stcpc_info_t*)api_tcpc.info_opt(tcpc);

	//如果当前还没有进入连接流程，那么尝试
	if(TCPC_STATE_IDLE == api_tcpc.state(tcpc)){

		//如果ip为0，解析domain的地址
		if(stcpc->peer_ip.s_addr == 0){
			ret = api_net.name2ip(stcpc->domain, &(stcpc->peer_ip));

			if(STATE_OK != ret){
				LOG_WARN("stcpc: Domain %s resolve failed.\r\n", stcpc->domain);
				goto retry;
			}
			LOG_INFO("stcpc: Domain %s resolved to %s.\r\n", stcpc->domain, api_net.ntoa(&stcpc->peer_ip));
		}


		//请求连接
		api_tcpc.req_conn(tcpc, stcpc->peer_ip.s_addr, stcpc->peer_port);
		LOG_INFO("stcpc: Try connecting to %s.\r\n", api_net.ntoa(&stcpc->peer_ip));


		//请求连接后，这里准备再来是因为 系统引导初期可能netif还没有up，第一次try可能没有效果
		//但如果 状态为 TCPC_STATE_CONNECTING，那么就不用再try了，直接等待CONN_OK 或者 CONN_ERR 事件即可
		retry:
			return 3000;	// 3秒后再来

	}
	
	//如果已经进入连接流程
	else
		//这里返回0 标识不再接续定时
		return 0;

}



//hold time定时检查
static int stcpc_timer_serial_check(nb_info_t* nb, nb_timer_t* ptmr, void* ctx)
{
	int slen = 0;
	tcpc_info_t* tcpc = (tcpc_info_t*)nb;
	add_stcpc_info_t* stcpc = (add_stcpc_info_t*)api_tcpc.info_opt(tcpc);

	//检查串口数据
	if(0 == (slen = api_serial.peek(SERIAL2))){
		//stcpc->last_peek = 0;	//清空
		goto TIMER_EXIT_NULL;
	}

	//没有连接，放弃串口数据，并且放弃之后的定时检查
	if(TCPC_STATE_CONNECTED != api_tcpc.state(tcpc)){
		api_serial.clean(SERIAL2);
		//stcpc->last_peek = 0;	//清空
		goto TIMER_EXIT_STOP;
	}

	//如果和上次不一样，那么说明在holdtime时间段内有数据进入，不发送
	//if(stcpc->last_peek != slen){
	//	stcpc->last_peek = slen;
	//	goto TIMER_EXIT_SKIP;
	//}

	//有数据则发送之
	do{
		int send_len = MIN(slen, sizeof(stcpc->pkt_buf));

		//加载
		api_serial.read(SERIAL2, stcpc->pkt_buf, send_len);

		//发送
		api_tcpc.send(tcpc, stcpc->pkt_buf, send_len);
		LOG_INFO("stc(spi no %u).\r\n", send_len);

		slen -= send_len;

		stcpc->statistic_up_serial_bytes += send_len;
	}while(slen);



	//stcpc->last_peek = slen;

//TIMER_EXIT_SKIP:
TIMER_EXIT_NULL:

	//1小心！
	//2这是一个回调函数，返回值标识是否接续定时，
	//2如果不需要再定计时，直接返回0即可

	//重新安装
	return stcpc->hold_time;

TIMER_EXIT_STOP:
	return 0;

}






/*
//串口接收消息源
static int stcpc_serial_event_src(void* sdev, void*ctx, int len)
{
	int ret = STATE_OK;
	tcpc_info_t * tcpc =  (tcpc_info_t*)ctx;
	add_stcpc_info_t* stcpc = (add_stcpc_info_t*)api_tcpc.info_opt(tcpc);
	nb_msg_t* msg = stcpc->serial_rx_msg;

	add_stcpc_msg_t *stcpc_msg = api_nb.msg_opt(msg);
	stcpc_msg->len = len;


	ret = api_nb.msg_send((nb_info_t *)tcpc, msg, TRUE);
	//ARCH_ASSERT("",ret == STATE_OK);

	return ret;
}
*/



//若没有设置peer_ip，域名解析
static int tcpc_stcpc_cb_enter(tcpc_info_t* tcpc)
{
	add_stcpc_info_t* stcpc = api_tcpc.info_opt(tcpc);
	//避开加载脚本
	//api_os.tick_sleep(1000);


	//开启串口
	api_serial.open(SERIAL2);
	api_serial.ctrl(SERIAL2, SERIAL_CTRL_SETUP, &(stcpc->serial_cfg));	//配置


	//域名解析 连接目标，在nb开始运转后执行
	api_nb.timer_attach((nb_info_t*)tcpc, 0, (fp_nb_timer_handler)tcpc_timer_resolve_connect, NULL, 0);


	return STATE_OK;

}




static int tcpc_stcpc_cb_exit(tcpc_info_t* tcpc)
{

	//add_stcpc_info_t* stcpc = api_tcpc.info_opt(tcpc);

	api_serial.close(SERIAL2);

	//if(stcpc->serial_rx_msg)
	//	nb_msg_free(stcpc->serial_rx_msg);

	//api_nb.hdlr_unset((nb_info_t*)tcpc, MSG_TYPE_STCPC_SERIAL);

	return STATE_OK;
}



static int tcpc_stcpc_cb_recv(tcpc_info_t* tcpc)
{
	int ret = STATE_OK;
	net_pkt_t* pkt;
	add_stcpc_info_t* stcpc = api_tcpc.info_opt(tcpc);

	ret = api_tcpc.recv(tcpc, &pkt);
	if(ret != STATE_OK){
		LOG_WARN("stcpc: recv err(%d).\r\n",ret);
		return ret;
	}

	do{
//		net_addr_t  *paddr;
//		u16_t peer_port;

		u16_t pkt_len = 0;
		u8_t *pkt_ptr = NULL; 
		if(STATE_OK != api_net.pkt_data(pkt, (void**)&pkt_ptr, &pkt_len) || pkt_len == 0)//
			break;

//		paddr = api_net.pkt_faddr(pkt);
//		peer_port = api_net.pkt_fport(pkt);

		if(pkt_len != 0){
			int rlen = pkt_len;
			//串口发送
			do{
				int n;
				n = api_serial.write(SERIAL2,  (const void*)pkt_ptr, rlen);
				rlen -= n; pkt_ptr += n;
				if(rlen)api_os.tick_sleep(1);
			}while(rlen);

			stcpc->statistic_down_serial_bytes += pkt_len;

			LOG_INFO("stc(ni so %u).\r\n", pkt_len);
		}
	}while(api_net.pkt_next(pkt) >= 0);

	api_net.pkt_free(pkt);

	return ret;
}


//连接成功
int tcpc_stcpc_cb_conn_ok(tcpc_info_t* tcpc)
{
	int ret = STATE_OK;
	add_stcpc_info_t* stcpc = api_tcpc.info_opt(tcpc);

	LOG_INFO("[stcpc]:Connected to [%s:%u].\r\n", api_net.ntoa(&(stcpc->peer_ip)), stcpc->peer_port);


	//如果时间阈值有设置而且大于0，安装串口检测定时器
	if(stcpc->hold_time > 0){
		stcpc->tmr = api_nb.timer_attach((nb_info_t*)tcpc, stcpc->hold_time, stcpc_timer_serial_check, NULL, 0);
		ARCH_ASSERT("",NULL != stcpc->tmr);
	}

	return ret;
}


//连接失败
int tcpc_stcpc_cb_conn_err(tcpc_info_t* tcpc)
{
	int ret = STATE_OK;
	add_stcpc_info_t* stcpc = api_tcpc.info_opt(tcpc);

	LOG_INFO("[stcpc]:Connect failed.\r\n");

	//If gives domain, may be IP changes. Set IP to 0, this will cause new DNS resolve.
	if(stcpc->domain[0] != '\0')
		 stcpc->peer_ip.s_addr = 0;

	// 3秒后重新引导连接过程
	api_nb.timer_attach((nb_info_t*)tcpc, 3000, (fp_nb_timer_handler)tcpc_timer_resolve_connect, NULL, 0);

	return ret;
}

//连接断开
int tcpc_stcpc_cb_disconn(tcpc_info_t* tcpc)
{
	//add_stcpc_info_t* stcpc = api_tcpc.info_opt(tcpc);

	LOG_INFO("[stcpc]:Peer disconnect.\r\n");

	// 3秒后重新引导连接过程
	api_nb.timer_attach((nb_info_t*)tcpc, 3000, (fp_nb_timer_handler)tcpc_timer_resolve_connect, NULL, 0);

	return STATE_OK;
}

static const tcpc_if_t tcpc_stcpc_if = {
	tcpc_stcpc_cb_enter ,
	tcpc_stcpc_cb_exit ,
	tcpc_stcpc_cb_conn_ok ,
	tcpc_stcpc_cb_conn_err ,
	tcpc_stcpc_cb_disconn ,
	tcpc_stcpc_cb_recv
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


/*
{	"enable":true,	//可选
	"net":{
		"domain":"",
		"dip":"",
		"dport":0,
		"lport":55555
	}, 
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

	tcpc_info_t* tcpc_inf = NULL;
	add_stcpc_info_t* stcpc;

	jsmntok_t tk[32];	//180+ bytes


	//读入配置json文件	2048bytes max
	js_buf = api_fs.read_alloc("/app/stcpc/cfg.json", 2048, &js_len, 0);
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
		LOG_INFO("stcpc disabled, abort.\r\n");
		goto load_abort;
	}


	//下面分配资源 并配置之

	tcpc_inf = api_tcpc.info_alloc(sizeof(add_stcpc_info_t));
	if(NULL == tcpc_inf)goto load_abort;
	stcpc = api_tcpc.info_opt(tcpc_inf);

	u16_t local_port;
	jsmntok_t *jt;

	//找到net对象
	jt = jsmn.key_value(js_buf, tk, "net");

	if(STATE_OK !=	jsmn.key2val_str(js_buf, jt, "domain", stcpc->domain, sizeof(stcpc->domain), NULL))
		goto load_abort;
	if(STATE_OK != jsmn.key2val_ipv4(js_buf, jt, "dip", &(stcpc->peer_ip.s_addr)))
		goto load_abort;
	if(STATE_OK != jsmn.key2val_u16(js_buf, jt, "dport", &(stcpc->peer_port)))
		goto load_abort;
	if(STATE_OK != jsmn.key2val_u16(js_buf, jt, "lport", &local_port))
		goto load_abort;


	jt = jsmn.key_value(js_buf, tk, "serial");
	serial_json_cfg(&(stcpc->serial_cfg), js_buf, js_len, jt);	//json -> serial_cfg_t


	if(STATE_OK != jsmn.key2val_u16(js_buf, tk, "holdtime", &(stcpc->hold_time)))
		goto load_abort;


	//json解析完毕 释放之
	if(js_buf){
		_free(js_buf);
		js_buf = NULL;
	}


	LOG_INFO("stcpc: cfg file parsed...\r\n");


	//上面全部是填充结构体的代码
	api_tcpc.info_preset(tcpc_inf, "stcpc", local_port, (tcpc_if_t*)&tcpc_stcpc_if);	//设置tcpc框架

	//框架建立消息处理，不再退出
	if(STATE_OK != api_tcpc.start(tcpc_inf))
		goto load_abort;

	return ADDON_LOADER_GRANTED;



load_abort:
	LOG_INFO("stcpc: aborting...\r\n");


	if(NULL != tcpc_inf)
		api_tcpc.info_free(tcpc_inf);


	if(NULL != js_buf)
		_free(js_buf);
	return ADDON_LOADER_ABORT;
}


//httpd接口  输出 json
/*

GET http://192.168.1.105/logic/wifiIO/invoke?target=stcpc.stcpc_status

{
	"up_byte":yyyyyyyyyy,
	"dn_byte":nnnnnnnnnn,
	"state":"connecting...",
	"ip":"192.168.120.124"	//dest
}

*/
int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	char buf[128];
	int n, size = sizeof(buf);

	tcpc_info_t* tcpc_inf;
	add_stcpc_info_t* stcpc;

	tcpc_inf = (tcpc_info_t*)api_nb.find("stcpc");

	//找到nb框架程序
	if(tcpc_inf){
		char* str;
		stcpc = api_tcpc.info_opt(tcpc_inf);
		switch(api_tcpc.state(tcpc_inf)){
		case TCPC_STATE_IDLE:
			str = "Resolving...";
			break;
		case TCPC_STATE_CONNECTING:
			str = "Connecting...";
			break;
		case TCPC_STATE_CONNECTED:
			str = "Connected";
			break;
		default:
			str = "Panic!";
			break;
		}

		n = 0;
		n += utl.snprintf(buf + n, size-n, "{\"up_byte\":%u,\"dn_byte\":%u,\"state\":\"%s\",\"ip\":\"", 
					stcpc->statistic_up_serial_bytes, stcpc->statistic_down_serial_bytes, str);
		n += api_net.ntoa_buf(&(stcpc->peer_ip), buf + n);	//IP
		n += utl.snprintf(buf + n, size-n, "\"}");

	}else{
		n = 0;
		n += utl.snprintf(buf + n, size-n, "{\"err\":\"not running.\"}");
	}

	return  consumer(ctx, (u8_t*)buf, n);
}




