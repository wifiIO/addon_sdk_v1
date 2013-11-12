
/**
 * @file			tcpc.c
 * @brief			tcp客户端演示例程
 *	
 *	
 * @author			dy@wifi.io
*/


#include "include.h"


#define DOMAIN_SIZE_MAX 64

//这里定义一个用户应用程序的数据结构，之所以尾缀加一个opt
//通过	api_tcpc.info_alloc(sizeof(tcpc_opt_t)) 函数开辟空间
//通过	api_tcpc.info_opt(tcpc_inf)  获取指针


typedef struct demo_tcpc_opt{
	char domain[DOMAIN_SIZE_MAX];
	u16_t peer_port;
	net_addr_t peer_ip;
//	u8_t pkt_buf[STCPC_LOADMAX_SIZE_MAX];	//发送缓存
	//u16_t pkt_buf_len;

}demo_tcpc_opt_t;



static int  tcpc_timer_resolve_connect(tcpc_info_t* tcpc, void* ctx)
{
	int ret = STATE_OK;
	demo_tcpc_opt_t* demotcpc;
	demotcpc = (demo_tcpc_opt_t*)api_tcpc.info_opt(tcpc);

	//如果当前还没有进入连接流程，那么尝试
	if(TCPC_STATE_IDLE == api_tcpc.state(tcpc)){

		//如果ip为0，解析domain的地址
		if(demotcpc->peer_ip.s_addr == 0){
			ret = api_net.name2ip(demotcpc->domain, &(demotcpc->peer_ip));

			if(STATE_OK != ret){
				LOG_WARN("demotcpc: Domain %s resolve failed.\r\n", demotcpc->domain);
				goto retry;
			}
			LOG_INFO("demotcpc: Domain %s resolved to %s.\r\n", demotcpc->domain, api_net.ntoa(&demotcpc->peer_ip));
		}


		//请求连接
		api_tcpc.req_conn(tcpc, demotcpc->peer_ip.s_addr, demotcpc->peer_port);
		LOG_INFO("demotcpc: Try connecting to %s.\r\n", api_net.ntoa(&demotcpc->peer_ip));


		//请求连接后，这里准备再来是因为 系统引导初期可能netif还没有up，第一次try可能没有效果
		//但如果 状态为 TCPC_STATE_CONNECTING，那么就不用再try了，直接等待CONN_OK 或者 CONN_ERR 事件即可
		retry:
			return 3000;	// 3秒后再来

	}
	
	//如果已经进入连接流程
	else
		//这里返回0 标识不再接续定时
		return NB_TIMER_RELEASE;

}

static int tcpc_timer_timeout(nb_info_t* pnb, nb_timer_t* ptmr, void *ctx)
{
	tcpc_info_t* tcpc = (tcpc_info_t*)pnb;

	if(TCPC_STATE_CONNECTED == api_tcpc.state(tcpc)){
		api_tcpc.req_disconn(tcpc);	//主动关闭连接
	}
	return NB_TIMER_RELEASE;
}


//设置 解析域名 和连接目标的 定时器 并使之立即执行

static int tcpc_demotcpc_cb_enter(tcpc_info_t* tcpc)
{
//	demo_tcpc_opt_t* demotcpc = api_tcpc.info_opt(tcpc);

	LOG_INFO("demotcpc: In tcpc enter().\r\n");

#define NO_WAIT 0
	//域名解析 连接目标，在nb开始运转后执行
	api_nb.timer_attach((nb_info_t*)tcpc, 0, (fp_nb_timer_handler)tcpc_timer_resolve_connect, NULL, NO_WAIT);

	return STATE_OK;

}




static int tcpc_demotcpc_cb_exit(tcpc_info_t* tcpc)
{
	//demo_tcpc_opt_t* demotcpc = api_tcpc.info_opt(tcpc);
	LOG_INFO("demotcpc: In tcpc exit().\r\n");

	return STATE_OK;
}


#define TIMER_CTX_MAGIC_KEEPALIVE	(void*)0x9527ABCD	//通过ctx 标记
#define TIMEOUT_MS_KEEPALIVE	3000

static int tcpc_demotcpc_cb_recv(tcpc_info_t* tcpc)
{
	int ret = STATE_OK;
	net_pkt_t* pkt;
//	demo_tcpc_opt_t* demotcpc = api_tcpc.info_opt(tcpc);

	ret = api_tcpc.recv(tcpc, &pkt);
	if(ret != STATE_OK){
		LOG_WARN("demotcpc: recv err(%d).\r\n",ret);
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
			//消费掉 pkt_ptr pkt_len
			LOG_INFO("Income %d.\r\n", pkt_len);
		}
	}while(api_net.pkt_next(pkt) >= 0);

	api_net.pkt_free(pkt);



	//定时器续时间
	nb_timer_t* ptmr = api_nb.timer_by_ctx((nb_info_t*)tcpc, TIMER_CTX_MAGIC_KEEPALIVE);
	if(NULL == ptmr){
		LOG_WARN("Timer invalid.\r\n");
		return STATE_ERROR;}
	api_nb.timer_restart((nb_info_t*)tcpc, TIMEOUT_MS_KEEPALIVE, ptmr);


	return ret;
}



//连接成功
int tcpc_demotcpc_cb_conn_ok(tcpc_info_t* tcpc)
{
	int ret = STATE_OK;
	demo_tcpc_opt_t* demotcpc = api_tcpc.info_opt(tcpc);

	LOG_INFO("[demotcpc]:Connected to [%s:%u].\r\n", api_net.ntoa(&(demotcpc->peer_ip)), demotcpc->peer_port);

	//添加keepalive定时器
	nb_timer_t* ptmr;
	if(NULL == (ptmr = api_nb.timer_by_ctx((nb_info_t*)tcpc, TIMER_CTX_MAGIC_KEEPALIVE))){	//若没有就申请新的
		ptmr = api_nb.timer_attach((nb_info_t*)tcpc, TIMEOUT_MS_KEEPALIVE, tcpc_timer_timeout, TIMER_CTX_MAGIC_KEEPALIVE, 0);	// 3s没有新收入就断开连接
	}
	else
		api_nb.timer_restart((nb_info_t*)tcpc, TIMEOUT_MS_KEEPALIVE, ptmr);


	//发送
	api_tcpc.send(tcpc, "hello!", sizeof("hello!")-1);


	return ret;
}


//连接失败
int tcpc_demotcpc_cb_conn_err(tcpc_info_t* tcpc)
{
	int ret = STATE_OK;
	demo_tcpc_opt_t* demotcpc = api_tcpc.info_opt(tcpc);

	LOG_INFO("[demotcpc]:Connect failed.\r\n");

	//Set IP to 0, invoke new DNS resolve.
	if(demotcpc->domain[0] != '\0')
		 demotcpc->peer_ip.s_addr = 0;

	// 3秒后重新引导连接过程
	api_nb.timer_attach((nb_info_t*)tcpc, 3000, (fp_nb_timer_handler)tcpc_timer_resolve_connect, NULL, 0);

	return ret;
}

//连接断开
int tcpc_demotcpc_cb_disconn(tcpc_info_t* tcpc)
{
	//demo_tcpc_opt_t* demotcpc = api_tcpc.info_opt(tcpc);

	LOG_INFO("[demotcpc]:Disconnect.\r\n");


	//如果需要，可以重新引导连接过程
	//api_nb.timer_attach((nb_info_t*)tcpc, 3000, (fp_nb_timer_handler)tcpc_timer_resolve_connect, NULL, 0);
	//也可以退出程序
	//api_nb.req_exit((nb_info_t *) tcpc);

	return STATE_OK;
}

static const tcpc_if_t tcpc_demotcpc_if = {
	tcpc_demotcpc_cb_enter ,
	tcpc_demotcpc_cb_exit ,
	tcpc_demotcpc_cb_conn_ok ,
	tcpc_demotcpc_cb_conn_err ,
	tcpc_demotcpc_cb_disconn ,
	tcpc_demotcpc_cb_recv
};














int main(int argc, char* argv[])
{

	tcpc_info_t* tcpc_inf = NULL;
	demo_tcpc_opt_t* demotcpc;


	tcpc_inf = api_tcpc.info_alloc(sizeof(demo_tcpc_opt_t));	//新建tcpc框架 并且开辟用户定义的数据结构
	if(NULL == tcpc_inf)
		goto load_abort;

	demotcpc = api_tcpc.info_opt(tcpc_inf);	//提取用户数据结构指针
	_memset(demotcpc, 0, sizeof(demotcpc));

	//填充结构体
	_strcpy(demotcpc->domain, "wifi.io");
	demotcpc->peer_port = 80;

	
	api_tcpc.info_preset(tcpc_inf, "demotcpc", 0, (tcpc_if_t*)&tcpc_demotcpc_if);	//设置tcpc框架

	//框架开始消息处理
	if(STATE_OK != api_tcpc.start(tcpc_inf))
		goto load_abort;

	//到这里表示程序运行完毕需要退出，因此也就没有保留在ram中的必要了
	LOG_INFO("demotcpc: exiting...\r\n");
	return ADDON_LOADER_ABORT;	//ADDON_LOADER_GRANTED;

load_abort:
	LOG_INFO("demotcpc: aborting...\r\n");

	if(NULL != tcpc_inf)
		api_tcpc.info_free(tcpc_inf);

	return ADDON_LOADER_ABORT;
}



