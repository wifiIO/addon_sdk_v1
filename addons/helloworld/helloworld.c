#include "include.h"



/*
{
	"method":"hello.onoff",
	"params":"on"/"off"
}
*/
//非常典型的一个“委托接口”函数，可以被外部程序通过 json rpc 方式调用

int JSON_RPC(onoff)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	

//__ADDON_EXPORT__ 是gcc的编译属性 添加该宏 使本函数可以被给其他的addon调用
// JSON_DELEGATE 宏仅仅为本函数添加一个 后缀， onoff -> onoff_JsDlgt 实际被暴露的函数名称

{
	int ret = STATE_OK;
	char* err_msg = NULL;
	char onoff_msg[32];

	//下面从json中提取 param 字符串
	if(JSMN_STRING != pjn->tkn->type || 
	     STATE_OK != jsmn.tkn2val_str(pjn->js, pjn->tkn, onoff_msg, sizeof(onoff_msg), NULL)){	//pjn->tkn 指向json中params字段的名值对，使用tkn2val_str()从中提取string类型的value

		ret = STATE_ERROR;	
		err_msg = "Params error."; 
		LOG_WARN("mirror:%s.\r\n", err_msg);
		goto exit_err;
	}


	//这里正确的得到了字符串，放置在 onoff_msg 中
	if(0 == _strcmp("on", onoff_msg)){
		api_io.low(WIFIIO_GPIO_01);
		api_io.low(WIFIIO_GPIO_02);
	}

	else if(0 == _strcmp("off", onoff_msg)){
		api_io.high(WIFIIO_GPIO_01);
		api_io.high(WIFIIO_GPIO_02);
	}


	if(ack)jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}





int main(int argc, char* argv[])
{
	int n;
	api_io.init(WIFIIO_GPIO_01, OUT_PUSH_PULL);
	api_io.init(WIFIIO_GPIO_02, OUT_PUSH_PULL);
	api_io.low(WIFIIO_GPIO_01);
	api_io.high(WIFIIO_GPIO_02);

	n = 0;
	while(n++ < 10)
	{
		api_os.tick_sleep(1000);

		api_io.toggle(WIFIIO_GPIO_01);
		api_io.toggle(WIFIIO_GPIO_02);

	}

	return ADDON_LOADER_GRANTED;

//err_exit:
	return ADDON_LOADER_ABORT;		//release 
}


