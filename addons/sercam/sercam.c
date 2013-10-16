

/**
 * @file			sercam.c
 * @brief			驱动串口摄像头
 *	
 *	
 * @author			dy@wifi.io
*/


//Thanks to: https://github.com/adafruit/Adafruit-VC0706-Serial-Camera-Library



#include "include.h"

#define BOOL u8_t

#define VC0706_RESET	0x26
#define VC0706_GEN_VERSION 0x11
#define VC0706_READ_FBUF 0x32
#define VC0706_GET_FBUF_LEN 0x34
#define VC0706_FBUF_CTRL 0x36
#define VC0706_DOWNSIZE_CTRL 0x54
#define VC0706_DOWNSIZE_STATUS 0x55
#define VC0706_READ_DATA 0x30
#define VC0706_WRITE_DATA 0x31
#define VC0706_COMM_MOTION_CTRL 0x37
#define VC0706_COMM_MOTION_STATUS 0x38
#define VC0706_COMM_MOTION_DETECTED 0x39
#define VC0706_MOTION_CTRL 0x42
#define VC0706_MOTION_STATUS 0x43
#define VC0706_TVOUT_CTRL 0x44
#define VC0706_OSD_ADD_CHAR 0x45

#define VC0706_STOPCURRENTFRAME 0x0
#define VC0706_STOPNEXTFRAME 0x1
#define VC0706_RESUMEFRAME 0x3
#define VC0706_STEPFRAME 0x2

#define VC0706_640x480 0x00
#define VC0706_320x240 0x11
#define VC0706_160x120 0x22

#define VC0706_MOTIONCONTROL 0x0
#define VC0706_UARTMOTION 0x01
#define VC0706_ACTIVATEMOTION 0x01

#define VC0706_SET_ZOOM 0x52
#define VC0706_GET_ZOOM 0x53

#define CAMERABUFFSIZ 100
#define CAMERADELAY 3000	//10




u8_t serialNum = 0;
u8_t camerabuff[CAMERABUFFSIZ+1];
u8_t bufferLen = 0;
u16_t frameptr = 0;



/**************** low level commands */


static BOOL verifyResponse(u8_t command) {
	if ((camerabuff[0] != 0x76) ||
		(camerabuff[1] != serialNum) ||
		(camerabuff[2] != command) ||
		(camerabuff[3] != 0x0))
		return FALSE;
	return TRUE;
	
}


static int serial_read_wait(int sno, u8_t* buf, u32_t len, u32_t wait)
{
	u32_t lasttime, last_len, pass = 0;

	//first try read
	last_len = api_serial.read(sno, buf, len);
	pass += last_len;

	//time limit
	lasttime = api_os.tick_now();

	while(wait > 0 && pass < len && api_os.tick_elapsed(lasttime) < wait)
	{
		if(last_len == 0)api_os.tick_sleep(1);
		last_len = api_serial.read(sno, (void*)(buf + pass), len - pass);
		pass += last_len;
	}

	return pass;
}

//指示总长度和最长时间 将消费者送到工厂
static int factory_serial_read_len_wait(int sno, u32_t len, u32_t wait, fp_consumer_generic fp, void* ctx)
{
	u32_t lasttime, last_len, pass = 0;
	u8_t buf[512];

	//first try read
	last_len = api_serial.read(sno, buf, MIN(sizeof(buf), len));
	if(last_len > 0 && STATE_OK != fp(ctx, buf, last_len))	//if data, consume
		goto safe_exit;
	pass += last_len;

	//time limit
	lasttime = api_os.tick_now();

	while(wait > 0 && pass < len && api_os.tick_elapsed(lasttime) < wait)
	{
		if(last_len == 0)
			api_os.tick_sleep(50);	//11.5Kbytes/second  40ms means 460bytes 
		last_len = api_serial.read(sno, buf, MIN(sizeof(buf), len - pass));
		if(last_len > 0 && STATE_OK != fp(ctx, buf, last_len))	//if data, consume
			break; //goto safe_exit;

		pass += last_len;
	}

safe_exit:
	return pass;
}



static u8_t readResponse(u8_t numbytes, u16_t timeout) {

	bufferLen = serial_read_wait(SERIAL2, camerabuff, numbytes, timeout);
	return bufferLen;
}



static void sendCommand(u8_t cmd, u8_t args[], u8_t argn) 
{
	u8_t buf[4] = {0x56, serialNum, cmd};
	api_serial.write(SERIAL2, buf, 3);
	api_serial.write(SERIAL2, args, argn);
}


static BOOL runCommand(u8_t cmd, u8_t *args, u8_t argn, u8_t resplen, BOOL flushflag) 
{
	int i;
	// flush out anything in the buffer?
	if (flushflag) {
		while(readResponse(100, 5)){}; 
	}

	sendCommand(cmd, args, argn);


	if((i = readResponse(resplen, 200)) != resplen){
		LOG_WARN("runCommand: resplen:%d (expect %d).\r\n", i, resplen);
		return FALSE;
	}


	if (!verifyResponse(cmd)){
		LOG_WARN("runCommand: resp err:\r\n");
		api_console.hexdump((char*)camerabuff, resplen, 0);
		return FALSE;
	}
	return TRUE;
}





BOOL reset() {
	u8_t args[] = {0x0};
	return runCommand(VC0706_RESET, args, 1, 5, TRUE);
}

BOOL motionDetected() {
	if (readResponse(4, 200) != 4) {
		return FALSE;
	}
	if (!verifyResponse(VC0706_COMM_MOTION_DETECTED))
		return FALSE;

	return TRUE;
}

u8_t setMotionStatus(u8_t x, u8_t d1, u8_t d2) {
	u8_t args[] = {0x03, x, d1, d2};
	return runCommand(VC0706_MOTION_CTRL, args, sizeof(args), 5, TRUE);
}

u8_t getMotionStatus(u8_t x) {
	u8_t args[] = {0x01, x};
	return runCommand(VC0706_MOTION_STATUS, args, sizeof(args), 5, TRUE);
}


BOOL setMotionDetect(BOOL flag) {
	if (!setMotionStatus(VC0706_MOTIONCONTROL, 
			VC0706_UARTMOTION, VC0706_ACTIVATEMOTION))
		return FALSE;

	u8_t args[] = {0x01, flag};
	return runCommand(VC0706_COMM_MOTION_CTRL, args, sizeof(args), 5, TRUE);
}



BOOL getMotionDetect(void) {
	u8_t args[] = {0x0};
	if (!runCommand(VC0706_COMM_MOTION_STATUS, args, 1, 6, TRUE))
		return FALSE;

	return camerabuff[5];
}

u8_t getImageSize() {
	u8_t args[] = {0x4, 0x4, 0x1, 0x00, 0x19};
	if (!runCommand(VC0706_READ_DATA, args, sizeof(args), 6, TRUE))
		return -1;

	return camerabuff[5];
}

BOOL setImageSize(u8_t x) {
	u8_t args[] = {0x05, 0x04, 0x01, 0x00, 0x19, x};

	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5, TRUE);
}

/****************** downsize image control */

u8_t getDownsize(void) {
	u8_t args[] = {0x0};
	if (!runCommand(VC0706_DOWNSIZE_STATUS, args, 1, 6, TRUE)) 
		return -1;

	return camerabuff[5];
}

BOOL setDownsize(u8_t newsize) {
	u8_t args[] = {0x01, newsize};

	return runCommand(VC0706_DOWNSIZE_CTRL, args, 2, 5, TRUE);
}

/***************** other high level commands */

char * getVersion(void) {
	u8_t args[] = {0x00};

	sendCommand(VC0706_GEN_VERSION, args, 1);
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200)) 
		return NULL;

	camerabuff[bufferLen] = 0;	// end it!
	return (char *)camerabuff;	// return it!
}


/****************** high level photo comamnds */

void OSD(u8_t x, u8_t y, char *str) {
	int i;
	if (_strlen(str) > 14) { str[13] = 0; }

	u8_t args[17] = {_strlen(str), _strlen(str)-1, (y & 0xF) | ((x & 0x3) << 4)};

	for(i=0; i<_strlen(str); i++) {
		char c = str[i];
		if ((c >= '0') && (c <= '9')) {
			str[i] -= '0';
		} else if ((c >= 'A') && (c <= 'Z')) {
			str[i] -= 'A';
			str[i] += 10;
		} else if ((c >= 'a') && (c <= 'z')) {
			str[i] -= 'a';
			str[i] += 36;
		}

		args[3+i] = str[i];
	}

	runCommand(VC0706_OSD_ADD_CHAR, args, _strlen(str)+3, 5, TRUE);
	//printBuff();
}

BOOL setCompression(u8_t c) {
	u8_t args[] = {0x5, 0x1, 0x1, 0x12, 0x04, c};
	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5, TRUE);
}

u8_t getCompression(void) {
	u8_t args[] = {0x4, 0x1, 0x1, 0x12, 0x04};
	runCommand(VC0706_READ_DATA, args, sizeof(args), 6, TRUE);
	//printBuff();
	return camerabuff[5];
}

BOOL setPTZ(u16_t wz, u16_t hz, u16_t pan, u16_t tilt) {
	u8_t args[] = {0x08, wz >> 8, wz, hz >> 8, wz, pan>>8, pan, tilt>>8, tilt};
	return (!runCommand(VC0706_SET_ZOOM, args, sizeof(args), 5, TRUE));
}

/*
BOOL getPTZ(u16_t &w, u16_t &h, u16_t &wz, u16_t &hz, u16_t &pan, u16_t &tilt) {
	u8_t args[] = {0x0};
	if (!runCommand(VC0706_GET_ZOOM, args, sizeof(args), 16, TRUE))
		return FALSE;
	//printBuff();

	w = camerabuff[5];
	w <<= 8;
	w |= camerabuff[6];

	h = camerabuff[7];
	h <<= 8;
	h |= camerabuff[8];

	wz = camerabuff[9];
	wz <<= 8;
	wz |= camerabuff[10];

	hz = camerabuff[11];
	hz <<= 8;
	hz |= camerabuff[12];

	pan = camerabuff[13];
	pan <<= 8;
	pan |= camerabuff[14];

	tilt = camerabuff[15];
	tilt <<= 8;
	tilt |= camerabuff[16];

	return TRUE;
}

*/

BOOL cameraFrameBuffCtrl(u8_t command) {
	u8_t args[] = {0x1, command};
	return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5, TRUE);
}

BOOL takePicture() {
	frameptr = 0;
	return cameraFrameBuffCtrl(VC0706_STOPCURRENTFRAME); 
}

BOOL resumeVideo() {
	return cameraFrameBuffCtrl(VC0706_RESUMEFRAME); 
}

BOOL TVon() {
	u8_t args[] = {0x1, 0x1};
	return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5, TRUE);
}
BOOL TVoff() {
	u8_t args[] = {0x1, 0x0};
	return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5, TRUE);
}

u32_t frameLength(void) {
	u8_t args[] = {0x01, 0x00};
	if (!runCommand(VC0706_GET_FBUF_LEN, args, sizeof(args), 9, TRUE))
		return 0;

	u32_t len;
	len = camerabuff[5];
	len <<= 8;
	len |= camerabuff[6];
	len <<= 8;
	len |= camerabuff[7];
	len <<= 8;
	len |= camerabuff[8];

	return len;
}


u8_t available(void) {
	return bufferLen;
}


u8_t* readPicture(u32_t n) {
	u8_t args[] = {0x0C, 0x0, 		//帧类型 0:当前帧 1:下一帧
					0x0A,			//操作方式 0x0F(DMA)
					0, 0, frameptr >> 8, frameptr & 0xFF,	//起始地址 4字节 
					0, 0, BYTE2(n), BYTE1(n), 	//数据长度 4字节
					BYTE2(CAMERADELAY), BYTE1(CAMERADELAY)	//延时时间 2字节
					};

	if(!runCommand(VC0706_READ_FBUF, args, sizeof(args), 5, FALSE))
		return FALSE;

	// read into the buffer PACKETLEN!
	if(readResponse(n+5, CAMERADELAY) == 0) 
		return FALSE;

	frameptr += n;
	return camerabuff;
}



BOOL jpeg_read_start(u32_t pos, u32_t n) {
	u8_t args[] = {0x0C, 0x0, 		//帧类型 0:当前帧 1:下一帧
					0x0A,			//操作方式 0x0F(DMA)
					0, 0, pos >> 8, pos & 0xFF,	//起始地址 4字节 
					0, 0, BYTE2(n), BYTE1(n), 	//数据长度 4字节
					BYTE2(CAMERADELAY), BYTE1(CAMERADELAY)	//延时时间 2字节
					};

	if(!runCommand(VC0706_READ_FBUF, args, sizeof(args), 5, TRUE))
		return FALSE;

	return TRUE;
}

BOOL jpeg_read_finsh(void) {	//在图片数据发还之后，尾部还会带有一个命令回复

	if(readResponse(5, 200) != 5){
		return FALSE;
	}

	if (!verifyResponse(VC0706_READ_FBUF)){
		LOG_WARN("runCommand: resp err:\r\n");
		api_console.hexdump((char*)camerabuff, 16, 0);
		return FALSE;
	}

	return TRUE;
}





//This is called when httpd just parsed http header part.
int snapshot_hdr(httpd_session_t* hd_session)
{
	httpd_backend_t *h_bkend = api_httpd.sess_backend(hd_session);
//	char *param = BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_HAS_PARAM)? &(h_bkend->req[h_bkend->param_start]) : NULL;
	int jpeg_len, n, ret;
//	backend_priv_scam_t* priv_scam = _malloc(sizeof(backend_priv_scam_t));	//malloc app related structure
//	_memset(priv_scam, 0, sizeof(backend_priv_scam_t));	
//	h_bkend->priv = priv_scam;

	//Must GET
	if(HTTP_METHOD_GET(h_bkend->req_flag) != HTTP_REQ_FLAG_GET){
		goto request_finish;
	}


	ret = STATE_OK;
	if(!takePicture())
		ret = STATE_ERROR;

	//_tick_sleep();
	jpeg_len = (int)frameLength();
	LOG_INFO("Jpeg len:%d\r\n", jpeg_len);

	if(STATE_OK != ret){	//返回服务器故障
		LOG_WARN("Failed to snap!");
		api_httpd.resp_reset(hd_session);	//start respond to usr's browser
		api_httpd.resp_setStatus(hd_session, HTTP_RSP_500_SERVER_ERR);
		api_httpd.resp_setChunked(hd_session);
		api_httpd.resp_headerCommit(hd_session);
		api_httpd.resp_write_chunk(hd_session, NULL, 0);
		goto request_finish;
	}


	api_httpd.resp_reset(hd_session);	//start respond to usr's browser
	api_httpd.resp_setStatus(hd_session, HTTP_RSP_200_OK);
	api_httpd.resp_setContentType(hd_session, "image/jpeg");	// set content-type
	api_httpd.resp_setConnection(hd_session, BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_KEEPALIVE ));	//keepalive
	api_httpd.resp_setCache(hd_session, CACHE_CTRL_NOCACHE);	//file data from serial, no cache
//	api_httpd.resp_setContentLength(hd_session, jpeg_len);
	api_httpd.resp_setChunked(hd_session);	//We don't use Content-Length,so set chunked
	api_httpd.resp_headerCommit(hd_session);

	//send jpeg
	n = 0;
/*
	while(jpeg_len > 0){
		u8_t* ptr = NULL;
		n = MIN(32, jpeg_len);
		ptr = readPicture(n);
		jpeg_len -= n;

		if(NULL == ptr || 
			STATE_OK != api_httpd.resp_write_chunk(hd_session, (const char*)ptr, n))
			goto close_exit;
	};	// last chunk must len == 0
*/
	ret = jpeg_read_start(0, jpeg_len);
	LOG_INFO("jpeg_read_start: %d\r\n", ret);

	n = factory_serial_read_len_wait(SERIAL2, jpeg_len, 5000, (fp_consumer_generic)(api_httpd.resp_write_chunk), (void*)hd_session);

	if(n < jpeg_len){	//error may happen
		LOG_WARN("jpeg_read %d bytes(need %d)\r\n", n, jpeg_len);
		goto close_exit;
	}

	api_httpd.resp_write_chunk(hd_session, NULL, 0);	//finish http chunk
	api_httpd.resp_flush(hd_session);

	jpeg_read_finsh();

	resumeVideo();

	//finish this request (not session), this will cause "xxx_body" and "xxx_end" not be called.
	api_httpd.resp_finish(hd_session);
	return STATE_OK;

request_finish:
	if(NULL != h_bkend->priv){	//MUST free priv before "api_httpd.resp_finish"
		_free(h_bkend->priv);
		h_bkend->priv = NULL;
	}
	//finish this request (not session), this will cause "xxx_body" and "xxx_end" not be called.
	api_httpd.resp_finish(hd_session);
	return STATE_OK;

close_exit:
	//finish this request (not session), this will cause "xxx_body" and "xxx_end" not be called.
	api_httpd.resp_finish(hd_session);
	api_httpd.sess_abort(hd_session);
	return STATE_OK;

}

int snapshot_body(httpd_session_t* hd_session, char* ptr, int len){return STATE_OK;}//This is called when httpd encount http body part, it may be called several times (when POST long data).When GET this'll not be called.
int snapshot_end(httpd_session_t* hd_session){return STATE_OK;}//This is called when httpd parser finish whole http req.




/*
//This is called when httpd just parsed http header part.
int setup_hdr(httpd_session_t* hd_session)
{
	httpd_backend_t *h_bkend = api_httpd.sess_backend(hd_session);
	char *param = BIT_IS_SET(h_bkend->req_flag, HTTP_REQ_FLAG_HAS_PARAM)? &(h_bkend->req[h_bkend->param_start]) : NULL;

//	backend_priv_scam_t* priv_scam = _malloc(sizeof(backend_priv_scam_t));	//malloc app related structure
//	_memset(priv_scam, 0, sizeof(backend_priv_scam_t));	
//	h_bkend->priv = priv_scam;

	int param_val;
	int param_str_len;
	char* param_str_val;
	char str[8];





	//必须指明 op 参数
	if(	NULL == (param_str_val = api_httpd.params_get_string(param, "op", &param_str_len))	){

	}
	else{

		if(0 == string.memcmp("snapshot", param_str_val, param_str_len)){

		}
		else if(0 == string.memcmp("set_res", param_str_val, param_str_len)){

			//必须指定res参数
			if(	NULL == (param_str_val = api_httpd.params_get_string(param, "res", &param_str_len))){
				
			}

			else{

				if(0 == string.memcmp("vga", param_str_val, param_str_len)){

				}
				else if(0 == string.memcmp("qvga", param_str_val, param_str_len)){

				}

				//没有其他选择
				else{


				}

			}


		}
		else if(0 == string.memcmp("set_comp", param_str_val, param_str_len)){
			//必须要comp参数
			if(0 < (param_val = api_httpd.params_get_uint(param, "comp"))){
				if(param_val > SWEB_WAIT_MS_MAX) 
					param_val = SWEB_WAIT_MS_MAX;	//若大于允许值
			}
			else 
				param_val = 0;	//若无指定，设置为0

		}


	}


	//Must GET
	if(HTTP_METHOD_GET(h_bkend->req_flag) != HTTP_REQ_FLAG_GET){
		goto request_finish;
	}



	return STATE_OK;


request_finish:
	if(NULL != h_bkend->priv){	//MUST free priv before "api_httpd.resp_finish"
		_free(h_bkend->priv); 
		h_bkend->priv = NULL;
	}

	//finish this request (not session), this will cause "snapshot_body" and "snapshot_end" not be called.
	api_httpd.resp_finish(hd_session);

	return STATE_OK;
}

int setup_body(httpd_session_t* hd_session, char* ptr, int len){return STATE_OK;}//This is called when httpd encount http body part, it may be called several times (when POST long data).When GET this'll not be called.
int setup_end(httpd_session_t* hd_session){return STATE_OK;}//This is called when httpd parser finish whole http req.


//http://192.168.x.xx/logic/sercam/setup?res=vga&comp=123
//设置分辨率: res=vga/qvga
//设置压缩率: comp=123
int __ADDON_EXPORT__ HTTPD_BACKEND_LOGIC(setup)(httpd_backend_logic_stage_t stage, 
									httpd_session_t* hd_session,
									char* ptr, int len)
{
	switch(stage){
	case HTTPD_REQUEST_ON_HEADER:
		return setup_hdr(hd_session);
		break;
	case HTTPD_REQUEST_ON_BODY:
		return setup_body(hd_session, ptr, len);
		break;
	case HTTPD_REQUEST_ON_COMPLETE:
		return setup_end(hd_session);
		break;
	};
	return STATE_OK;
}



*/


//http://192.168.x.xx/logic/sercam/snapshot	//读取图片
int __ADDON_EXPORT__ HTTPD_BACKEND_LOGIC(snapshot)(httpd_backend_logic_stage_t stage, 
									httpd_session_t* hd_session,
									char* ptr, int len)
{
	LOG_INFO("serial camera: in backend logic.\r\n");

	switch(stage){
	case HTTPD_REQUEST_ON_HEADER:
		return snapshot_hdr(hd_session);
		break;
	case HTTPD_REQUEST_ON_BODY:
		return snapshot_body(hd_session, ptr, len);
		break;
	case HTTPD_REQUEST_ON_COMPLETE:
		return snapshot_end(hd_session);
		break;
	};
	return STATE_OK;
}









int main(int argc, char* argv[])
{
	httpd_info_t* httpd;
	int n;
	serial_cfg_t serial_cfg = {
		.baudrate = 115200,	//这里要更改为模块的波特率
		.bits = 0,		/*0:8bits; 1:9bits; */
		.sbits = 1,		/*0:0.5Bit; 1:1Bits; 2:1.5Bits; 3:2Bits;*/
		.Parity = 0,	/*0:No; 1:Odd; 2:Even*/
		.option = 0		/*0:None; 1:RS485 */
	};

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
		LOG_WARN("scam: httpd service unavailable,abort.\r\n");
		goto load_abort;
	}


	api_serial.open(SERIAL2);
	api_serial.ctrl(SERIAL2, SERIAL_CTRL_SETUP, &serial_cfg);	//配置


	if (0 == reset()){
		LOG_WARN("No camera found.\r\n");
		api_serial.close(SERIAL2);
		goto load_abort;
	}
	LOG_INFO("Camera Found.\r\n");


	char *reply = getVersion();
	if (reply == NULL) {
		LOG_WARN("Failed to get version.\r\n");
		api_serial.close(SERIAL2);
		goto load_abort;
	}

	LOG_INFO("Serial Camera Ver: %s\r\n", reply);

	_tick_sleep(500);

	//VC0706_320x240
	//VC0706_160x120
/*
	if (FALSE == setImageSize(VC0706_640x480)){
		LOG_WARN("Resolution set failed.\r\n");
		api_serial.close(SERIAL2);
		goto load_abort;
	}
	LOG_INFO("Resolution set.\r\n");
*/


	if (FALSE == setCompression(0x80)){
		LOG_WARN("Compression set failed.\r\n");
		api_serial.close(SERIAL2);
		goto load_abort;
	}
	LOG_INFO("Compression set.\r\n");



	//return 0 indicating keep addon in ram
	return ADDON_LOADER_GRANTED;

load_abort:

	LOG_INFO("addload: aborting...\r\n");

	return ADDON_LOADER_ABORT;

}



