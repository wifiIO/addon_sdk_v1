
/**
 * @file			max7219.c
 * @brief			驱动max7219的点阵模块
 *					淘宝有售：http://item.taobao.com/item.htm?spm=0.0.0.0.4mlPfZ&id=22058276355
 * @author			dy@wifi.io
*/




#include "include.h"

/*
Command format should be:

{
	"method":"max7219.draw",
	"params":"3c42a581a599423c"	// 8bytes = 16hexs
}
*/


/*
MAX7219 Pin connection:
CLK: WIFIIO_GPIO_01
DIN: WIFIIO_GPIO_02
CS: WIFIIO_GPIO_03

CS: 片选端。该端为低电平时串行数据被载入移位寄存器。连续数据的后16位在cs 端的上升沿时被锁定。
CLK: 时钟序列输入端。最大速率为10MHz.在时钟的上升沿，数据移入内部移位寄存器。只有当cs 端为低电平时时钟输入才有效。 
DIN: 串行数据输入端口。在时钟上升沿时数据被载入内部的16位寄存器。
*/



#define MAX7219_CS WIFIIO_GPIO_14
#define MAX7219_DAT WIFIIO_GPIO_13
#define MAX7219_CLK WIFIIO_GPIO_15


#define MAX7219_CS_LOW api_io.low(WIFIIO_GPIO_14)
#define MAX7219_CS_HIGH api_io.high(WIFIIO_GPIO_14)

#define MAX7219_DAT_LOW api_io.low(WIFIIO_GPIO_13)
#define MAX7219_DAT_HIGH api_io.high(WIFIIO_GPIO_13)

#define MAX7219_CLK_LOW api_io.low(WIFIIO_GPIO_15)
#define MAX7219_CLK_HIGH api_io.high(WIFIIO_GPIO_15)



////////////////////////////////////////
//Detailed interact flow 
////////////////////////////////////////
void max7219_write_byte(u8_t b)
{
	u8_t n;
	for(n = 0; n < 8; n++){
		MAX7219_CLK_LOW;
		if(b&0x80)
			MAX7219_DAT_HIGH;
		else
			MAX7219_DAT_LOW;
		b <<= 1;
		MAX7219_CLK_HIGH;
	}
}

void max7219_write(u8_t address,u8_t dat)
{ 
	MAX7219_CS_LOW;
	max7219_write_byte(address);
	max7219_write_byte(dat);
	MAX7219_CS_HIGH;    
}

void max7219_init(void)
{
	max7219_write(0x09, 0x00);       //encode: BCD
	max7219_write(0x0a, 0x01);       //Brightness
	max7219_write(0x0b, 0x07);       //扫描界限；8个数码管显示
	max7219_write(0x0c, 0x01);       //掉电模式：0，普通模式：1
	max7219_write(0x0f, 0x00);       //显示测试：1；测试结束，正常显示：0
}




/*
////////////////////////////////////////
Json delegate interface are mostly used for callbacks, and is supported by both Httpd & OTd.

So if you want your code be called from web browser or from cloud(OTd),you write code in the form below.

int JSON_RPC(xxx)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx){}

httpd will receive POST request like:
{
	"method":"max7219.draw",
	"params":"3c42a581a599423c"	// 8bytes = 16hexs
}
////////////////////////////////////////
*/

int JSON_RPC(draw)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	u8_t bits[8], n;

	LOG_INFO("DELEGATE max7219.draw.\r\n");



/*
{
	"method":"draw",
	"params":"0123456789ABCDEF"	// 8bytes = 16hexs
}

*/
//	pjn->js point to very begining of json string.
//	pjn->tkn is the token object which points to json element "0123456789ABCDEF"
//	Google "jsmn" (super lightweight json parser) for detailed info.

	if(NULL == pjn->tkn || 
	    JSMN_STRING != pjn->tkn->type ||											//must be string
	    STATE_OK != jsmn.tkn2val_xbuf(pjn->js, pjn->tkn, bits, sizeof(bits), NULL)){		//parse string into binary array
		ret = STATE_ERROR;
		err_msg = "param error.";
		LOG_WARN("max7219:%s.\r\n", err_msg);
		goto exit_err;
	}

	char str[64];
	utl.snprintf_hex(str, sizeof(str), bits, sizeof(bits), 0x80|' ');	//Just print to debug console, 0x80 means up case,' 'means seperate by space
	LOG_INFO("max7219: %s \r\n", str);

	for(n = 0; n < sizeof(bits); n++)
		max7219_write(n+1, bits[n]);	//write to 7219 buf



//exit_safe:
	if(ack)
		jsmn.delegate_ack_result(ack, ctx, ret);	//just reurn {"result":0}
	return ret;

exit_err:
	if(ack)
		jsmn.delegate_ack_err(ack, ctx, ret, err_msg);	//return {"error":{"code":xxx, "message":"yyyy"}}
	return ret;
}




////////////////////////////////////////
//There is a main function in every addon, which will be called just after addon be loaded.
////////////////////////////////////////

int main(int argc, char* argv[])
{
	
	int n;
	 api_io.init(MAX7219_CS, OUT_PUSH_PULL);	//初始化引脚
	 api_io.init(MAX7219_DAT, OUT_PUSH_PULL);
	 api_io.init(MAX7219_CLK, OUT_PUSH_PULL);

	MAX7219_CS_HIGH;	//三个引脚设置为高电平
	MAX7219_CLK_HIGH;
	MAX7219_DAT_HIGH;

	max7219_init();


	u8_t bits[8] = {0x3c, 0x42, 0xa5, 0x81, 0xa5, 0x99, 0x42, 0x3c};	//:-)

	// {0x3c, 0x42, 0x99, 0xa9, 0xa9, 0x9d, 0x42, 0x3c}; //symbol @

	for(n = 0; n < sizeof(bits); n++)
		max7219_write(n+1, bits[n]);	//write to 7219 buf




	LOG_INFO("main: MAX7219 starting...\r\n");


	return ADDON_LOADER_GRANTED;
//err_exit:
//	return ADDON_LOADER_ABORT;
}







// This is a generic factory&consumer interface,which is designed for httpd only.


/*
GET http://192.168.1.xxx/logic/wifiIO/invoke?target=max7219.status
{"state":loaded}
*/

int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	char buf[32];
	int n, size = sizeof(buf);


	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"loaded\"}");
	return  consumer(ctx, (u8_t*)buf, n);
}







