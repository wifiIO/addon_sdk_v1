


#include "include.h"


//sweb模式下用户文件名路径最大长度
#define SWEB_USR_PATH_SIZE_MAX 32
//sweb模式下wait参数最大值
#define SWEB_WAIT_MS_MAX 60000
#define SWEB_DEFAULT_WAIT_MSEC 1000

//param名值对中名子字段最长
#define HTTPD_URLREQ_PARAM_NAME_SIZE_MAX 64



//serial <--> web
typedef struct backend_priv_sweb{
 	s8_t encode;
	s8_t clean;
	const char* mime;

	//用于分段报文的url解码
	char url_coded[3];
	u8_t url_coded_len;	//一定要注意每次开始初始化为0；

	u32_t body_read;		//body 
	s32_t wait;
}backend_priv_sweb_t;

//io <--> web
typedef struct cgi_info_ioweb{
	char* req_ptr;
	size_t req_len;
}cgi_info_ioweb_t;






static int serial_read_wait(int sno, u8_t* buf, u32_t len, u32_t wait)
{
	u32_t lasttime, last_len, pass = 0;

	//first try read
	last_len = api_serial.read(sno, buf, len);
	pass += last_len;

	//time limit
	lasttime = api_os.tick_now();

	while(wait > 0 && pass < len &&  api_os.tick_elapsed(lasttime) < wait)
	{
		if(last_len == 0)api_os.tick_sleep(1);
		last_len = api_serial.read(sno, (void*)(buf + pass), len - pass);
		pass += last_len;
	}

	return pass;
}




//wait is ms unit, if null, we just respond header including 200 OK
int httpd_respond_sweb_target(httpd_session_t* hd_session, const char* sweb_get_target, size_t sweb_get_target_len, const char* type)
{
	int len, ret = STATE_OK;
	u8_t buf[128];
	httpd_backend_t *h_bkend = api_httpd.sess_backend(hd_session);

	backend_priv_sweb_t* priv_sweb = (backend_priv_sweb_t*)h_bkend->priv;



	if(sweb_get_target)LOG_INFO("req sweb target:%s\r\n", sweb_get_target);


	if(priv_sweb->clean)api_serial.clean(SERIAL2);	//clean param


	
	if(sweb_get_target != NULL)
		api_serial.write(SERIAL2, sweb_get_target, sweb_get_target_len);	// send target req



	api_httpd.resp_reset(hd_session);	//start respond to usr's browser
	api_httpd.resp_setStatus(hd_session, HTTP_RSP_200_OK);

	if(NULL == type)
		type = "application/octet-stream";		//default mime type
	api_httpd.resp_setContentType(hd_session, type);	// set content-type

	if(0 == string.strcmp(type, "application/octet-stream")){
		utl.sprintf((char*)buf,"attachment; filename=%s", "SerialData.dat");
		api_httpd.resp_setHeader(hd_session, "Content-Disposition", (const char*)buf);	//invoke download
	}

	api_httpd.resp_setConnection(hd_session, BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_KEEPALIVE ));	//keepalive
	api_httpd.resp_setCache(hd_session, CACHE_CTRL_NOCACHE);	//file data from serial, no cache
	api_httpd.resp_setChunked(hd_session);	//We don't know Content-Length,so set chunked
	api_httpd.resp_headerCommit(hd_session);


	//if wait (blocked here,bad implementation ;-( )
	len = 0;
	do{
		if(len == 0 || len == sizeof(buf))		//if first time or serial caches more than buf size(last read full buf size)
			len = serial_read_wait(SERIAL2, buf, sizeof(buf), priv_sweb->wait);
		else
			len = serial_read_wait(SERIAL2, buf, sizeof(buf), 0);			//if len != sizeof(buf), indicating that last read() is likely get all the data,so we read() again here just make sure it is empty.

		ret = api_httpd.resp_write_chunk(hd_session, (const char*)buf, len);
		if(ret)goto close_exit;
	}while(len);	// last chunk must len == 0

	//flush out
	api_httpd.resp_flush(hd_session);

	//keepalive
	if(BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_KEEPALIVE ))
		return ret;

close_exit:

	//finish this request (not session), this will cause "xxx_body" and "xxx_end" not be called.
	api_httpd.resp_finish(hd_session);
	api_httpd.sess_abort(hd_session);
	return ret;
}



#define SWEB_URLENCODED 20 
//表示浏览器请求以 URL方式交互数据

//serial data -> response
//POST data or GET data
int httpd_respond_swebdata(httpd_session_t* hd_session, const char* type)
{
	int len, ret = STATE_OK;
	u8_t buf[64];
	httpd_backend_t *h_bkend = api_httpd.sess_backend(hd_session);
	backend_priv_sweb_t* priv_sweb = (backend_priv_sweb_t*)h_bkend->priv;


	//clean serial recv buf
	if(priv_sweb->clean)
		api_serial.clean(SERIAL2);


	//respond to usr's browser
	api_httpd.resp_reset(hd_session);
	//status
	api_httpd.resp_setStatus(hd_session, HTTP_RSP_200_OK);

	//if not upload via multipart
	if(!BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_MULTIPART)){

		LOG_INFO("req sweb data via ajax\r\n");

		//if indicated content-type
		if(type)
			api_httpd.resp_setContentType(hd_session, type);
		//find out by self
		else
			api_httpd.resp_setContentType(hd_session, "text/html");

		//chunked
		api_httpd.resp_setChunked(hd_session);

		//connection
		api_httpd.resp_setConnection(hd_session, BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_KEEPALIVE ));

		//no cache
		api_httpd.resp_setCache(hd_session, CACHE_CTRL_NOCACHE);
		
		api_httpd.resp_headerCommit(hd_session);



		//if wait (blocked here,bad implementation ;-( )
		len = 0;
		do{
			//if first time or serial caches more than buf size(last read full buf size)
			if(len == 0 || len == sizeof(buf))
				len = serial_read_wait(SERIAL2, buf, sizeof(buf), priv_sweb->wait);
			else
				//if len != sizeof(buf), indicating that last read() is likely get all the data,so we read() again here just make sure it is empty.
				len = serial_read_wait(SERIAL2, buf, sizeof(buf), 0);




			if(len && priv_sweb->encode == SWEB_URLENCODED){
				char url_buf[64];
				int inlen = len, outlen;
				while(inlen > 0 && ret == STATE_OK){
					outlen = utl.url_enc(&buf[len - inlen], &inlen, url_buf, sizeof(url_buf));
					ret = api_httpd.resp_write_chunk(hd_session, url_buf, outlen);
				}
			}
			else
				ret = api_httpd.resp_write_chunk(hd_session, (const char*)buf, len);

			if(ret)goto close_exit;
		}while(len);
		// last chunk must len == 0

	}



	//若为file upload ，接收数据，而且已经直接返回OK提示
	else if(BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_MULTIPART) && priv_sweb->body_read > 0){
		LOG_INFO("total:%d bytes", priv_sweb->body_read);
		api_httpd.resp_html(hd_session, HTTP_RSP_200_OK, (char*)html_ok_back, BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_KEEPALIVE ));
	}

	//若wait参数为空也不是file upload (AJAX请求)，直接返回状态200
	else{
		api_httpd.resp_setChunked(hd_session);
		api_httpd.resp_headerCommit(hd_session);
		api_httpd.resp_write_chunk(hd_session, NULL, 0);
	}

	//flush out
	api_httpd.resp_flush(hd_session);

	//keepalive
	if(BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_KEEPALIVE ))
		return ret;

close_exit:

	//finish this request (not session), this will cause "xxx_body" and "xxx_end" not be called.
	api_httpd.resp_finish(hd_session);
	api_httpd.sess_abort(hd_session);
	return ret;

}



//This is called when httpd just parsed http header part.
int serial_hdr(httpd_session_t* hd_session)
{
	httpd_backend_t *h_bkend = api_httpd.sess_backend(hd_session);
	char *param = BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_HAS_PARAM)? &(h_bkend->req[h_bkend->param_start]) : NULL;

	h_bkend->priv = stdlib.malloc(sizeof(backend_priv_sweb_t));
	string.memset(h_bkend->priv, 0, sizeof(backend_priv_sweb_t));


	backend_priv_sweb_t* priv_sweb = (backend_priv_sweb_t*)h_bkend->priv;
	int param_val;
	int param_str_len;
	char* param_str_val;
	char str[8];

	//当指明urlenc=true
	if(	NULL != (param_str_val = api_httpd.params_get_string(param, "urlenc", &param_str_len)) &&
		0 == string.memcmp("true", param_str_val, param_str_len))
		priv_sweb->encode = SWEB_URLENCODED;
	else
		priv_sweb->encode = 0;
	

	//获取wait参数
	if(0 < (param_val = api_httpd.params_get_uint(param, "wait"))){
		if(param_val > SWEB_WAIT_MS_MAX) param_val = SWEB_WAIT_MS_MAX;	//若大于允许值
	}
	else 
		param_val = 0;	//若无指定，设置为0
	priv_sweb->wait = param_val;


	//当指明clean=true
	if(	NULL != (param_str_val = api_httpd.params_get_string(param, "clean", &param_str_len)) &&
		0 == string.memcmp("true", param_str_val, param_str_len))
		priv_sweb->clean = 1;
	else
		priv_sweb->clean = 0;


	//求取mime type
	if(NULL != (param_str_val = api_httpd.params_get_string(param, "mime", &param_str_len)) && param_str_len < sizeof(str)){
		string.memcpy(str, param_str_val, param_str_len); str[param_str_len] = '\0';
		priv_sweb->mime = api_httpd.tag2mime((const char*)str);
	}
	else
		priv_sweb->mime = api_httpd.tag2mime("htm");		//缺省是text



	//if GET, respond serial data as file resources.
	if(HTTP_METHOD_GET(h_bkend->req_flag) == HTTP_REQ_FLAG_GET){

		//当指明target=xxx
		if(NULL != (param_str_val = api_httpd.params_get_string(param, "target", &param_str_len)) && param_str_len > 0 )
			httpd_respond_sweb_target(hd_session, param_str_val, param_str_len, priv_sweb->mime);
		else
			httpd_respond_sweb_target(hd_session, NULL, 0, priv_sweb->mime);			//if not include "target" param, this maybe simplest data req, set mime type to req indicating, so usr browser will not prompt "download" msgbox

		goto request_finish;
	}


	//if POST, respond serial data via ajax
	else if(HTTP_METHOD_GET(h_bkend->req_flag) == HTTP_REQ_FLAG_POST){

		if(h_bkend->Content_Length <= 0){		//if usr issued a zero-length post, this must be AJAX req, and there will no on_body() called,we'll finish it here.
			httpd_respond_swebdata(hd_session, priv_sweb->mime);

			goto request_finish;
		}

	}

	return STATE_OK;


request_finish:
	if(NULL != h_bkend->priv){	//MUST free priv before "api_httpd.resp_finish"
		_free(h_bkend->priv); 
		h_bkend->priv = NULL;
	}

	//finish this request (not session), this will cause "serial_body" and "serial_end" not be called.
	api_httpd.resp_finish(hd_session);

	return STATE_OK;
}

//This is called when httpd encount http body part, it may be called several times (when POST long data).When GET this'll not be called.
int serial_body(httpd_session_t* hd_session, char* ptr, int len)
{
	httpd_backend_t *h_bkend = api_httpd.sess_backend(hd_session);
	backend_priv_sweb_t* priv_sweb = (backend_priv_sweb_t*)h_bkend->priv;


	if(HTTP_METHOD_GET(h_bkend->req_flag) == HTTP_REQ_FLAG_POST){

		//multi-part
		if(BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_MULTIPART)){}	//skip

		//urlencoded
		else if(BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_URLCODED)){}	//skip

		//if have no "Content-type"head field, it's ajax req
		else{
			size_t send_len = len;
			if(priv_sweb->encode == SWEB_URLENCODED)
				send_len = utl.url_dec_ctx(ptr, len, (u8_t*)ptr, len, priv_sweb->url_coded, &(priv_sweb->url_coded_len));			//if req urlencoded.decode it.

			//send
			api_serial.write(SERIAL2, ptr, send_len);
		}
	}

	priv_sweb->body_read += len;

	return STATE_OK;
}


//This is called when httpd parser finish whole http req.
int  serial_end(httpd_session_t* hd_session)
{
	httpd_backend_t *h_bkend = api_httpd.sess_backend(hd_session);
	backend_priv_sweb_t* priv_sweb  = (backend_priv_sweb_t*)h_bkend->priv;



	httpd_respond_swebdata(hd_session, priv_sweb->mime);

	if(NULL != h_bkend->priv){
		_free(h_bkend->priv);
		h_bkend->priv = NULL;	//MUST free priv before "api_httpd.resp_finish"
	}

	api_httpd.resp_finish(hd_session);	//finish this request (not session), this will cause "serial_body" and "serial_end" not be called.

	return STATE_OK;

}


int __ADDON_EXPORT__ HTTPD_BACKEND_LOGIC(serial)(httpd_backend_logic_stage_t stage, 
									httpd_session_t* hd_session,
									char* ptr, int len)
{
	switch(stage){
	case HTTPD_REQUEST_ON_HEADER:
		return serial_hdr(hd_session);
		break;
	case HTTPD_REQUEST_ON_BODY:
		return serial_body(hd_session, ptr, len);
		break;
	case HTTPD_REQUEST_ON_COMPLETE:
		return serial_end(hd_session);
		break;
	};
	return STATE_OK;
}









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
		cfg->baudrate = 9600;	//default
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
	"serial":{
		"baud":115200,		//数值型 1200~115200
		"bits":"8bit",		//"8bit" "9bit"
		"sbits":"0.5bit",	//"0.5bit" "1bit" "1.5bit" "2bit"
		"parity":"none",	//"none" "odd" "even"
		"rs485":false
	}
}
*/


int main(int argc, char* argv[])
{
	httpd_info_t* httpd;
	int n;

	//尝试读取配置文件
	char* js_buf = NULL;
	size_t js_len;
	jsmntok_t tk[32];	//180+ bytes




	n = 3;
	while(n > 0){
		httpd = (httpd_info_t*)api_nb.find("httpd");	//初始加载 可能httpd还没引导起来
		if(httpd == NULL){
			api_os.tick_sleep(1000);
			n --;
		}else
			break;
	}
	if(n == 0){	//如果没有找到httpd服务，那么不用加载当前的addon了
		LOG_WARN("sweb: httpd service unavailable,abort.\r\n");
		goto load_abort;
	}



	js_buf = api_fs.read_alloc("/app/sweb/cfg.json", 2048, &js_len, 0);	//读入配置json文件	2048bytes max
	if(NULL == js_buf){
		goto load_abort;
	}


	u8_t u8_tmp;
	if(STATE_OK == jsmn.key2val_bool(js_buf, tk, "enable",  &u8_tmp) && FALSE == u8_tmp){
		LOG_INFO("sweb disabled, abort.\r\n");
		goto load_abort;
	}

	serial_cfg_t serial_cfg;
	jsmntok_t *jt = jsmn.key_value(js_buf, tk, "serial");
	serial_json_cfg(&serial_cfg, js_buf, js_len, jt);	//json -> serial_cfg_t


	//step2: init serial device
	api_serial.open(SERIAL2);
	api_serial.ctrl(SERIAL2, SERIAL_CTRL_SETUP, &serial_cfg);	//配置

	//step3: important! return 0 indicate keep addon in ram
	return ADDON_LOADER_GRANTED;

load_abort:

	LOG_INFO("addload: aborting...\r\n");


	if(NULL != js_buf)
		_free(js_buf);
	return ADDON_LOADER_ABORT;

}





//httpd接口  输出 json
/*
//该addon没有状态维持，仅仅反馈加载状态。
GET http://192.168.1.105/logic/wifiIO/invoke?target=sweb.sweb_status

{"state":loaded}

*/


int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	char buf[48];
	int n, size = sizeof(buf);


	addon_info_t* pself = api_addon.find("sweb", 0);

	n = 0;
	if(pself)
		n += utl.snprintf(buf + n, size-n, "{\"state\":\"loaded\"}");
	else
		n += utl.snprintf(buf + n, size-n, "{\"state\":\"not loaded\"}");

	return  consumer(ctx, (u8_t*)buf, n);
}





