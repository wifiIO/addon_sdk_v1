



#include "include.h"
#include "../addlite_lcdi2c/lcdi2c.h"

////////////////////////////////////////
// 本项目测试 lcdi2c addon 的功能
////////////////////////////////////////

#define PCF8574_Address 0x4E //PCF8574 I2C地址





////////////////////////////////////////
//委托接口的定义规则:
// JSON_DELEGATE() 宏为函数添加后缀名称
// __ADDON_EXPORT__ 宏将该符号定义为暴露的
// 参数形式 上 可以输入一个json串，带有一个
// 执行完成后的回调 及其附带上下文
//
// 该函数的运行上下文 是调用该委托接口的进程
// httpd、otd 都可以调用该接口
// 接口名称为 max7219.draw
//{
//	"method":"lcd.print",
//	"params":"0123456789ABCDEF"	// 8bytes = 16hexs
//}
////////////////////////////////////////

int __ADDON_EXPORT__ JSON_DELEGATE(print)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	char str[64];

	LOG_INFO("DELEGATE lcdi2c.print.\r\n");


/*
{
	"method":"lcd.print",
	"params":"0123456789ABCDEF"	// 8bytes = 16hexs
}
*/
	//pjn->tkn 正是 "0123456789ABCDEF"
	if(NULL == pjn->tkn || JSMN_STRING != pjn->tkn->type ||
	 STATE_OK != jsmn.tkn2val_str(pjn->js, pjn->tkn, str, sizeof(str), NULL)){		//判断输入json是否合法
		ret = STATE_ERROR;
		err_msg = "param error.";
		LOG_WARN("lcdi2c:%s.\r\n", err_msg);
		goto exit_err;
	}

	LOG_INFO("lcdi2c: %s \r\n", str);

	api_lcd_i2c.clear();
	api_lcd_i2c.print(str);


//exit_safe:
	if(ack)
		jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)
		jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}



//httpd接口 输出 json
/*
GET http://192.168.1.105/logic/wifiIO/invoke?target=lcdi2c.status

{"state":loaded}

*/


int __ADDON_EXPORT__ JSON_FACTORY(status)(char*arg, int len, fp_consumer_generic consumer, void *ctx)
{
	char buf[32];
	int n, size = sizeof(buf);


	n = 0;
	n += utl.snprintf(buf + n, size-n, "{\"state\":\"loaded\"}");
	return consumer(ctx, (u8_t*)buf, n);
}






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
	LOG_INFO("main:lcdi2c test...\r\n");


	if(NULL == api_addon.find("lcdi2c", 0)){
		LOG_WARN("lcdi2c test: addon\"lcdi2c\" should be loaded first.\r\n");
		goto err_exit;
	}

	api_lcd_i2c.init(WIFIIO_GPIO_01 ,WIFIIO_GPIO_02, PCF8574_Address ,16, 2);
	api_lcd_i2c.backlight();
//	api_lcd_i2c.autoscroll();

	api_lcd_i2c.print("This wifi.io !!!!");




	return ADDON_LOADER_GRANTED;
err_exit:
	return ADDON_LOADER_ABORT;	//运行完毕即退出
}








