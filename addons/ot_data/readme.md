#OT_data: 如何提交信息到云端（上行）


##概述

下面的例子要实现对DHT11模块的驱动，并且将其采集到的温湿度数据提交到云端。

![dht11](../../addons_img/dht11.jpg?raw=true)

补一下器件知识先：

http://www.adafruit.com/datasheets/DHT11-chinese.pdf

##使用说明

DHT11只需要占用一个IO口，代码头部宏定义中定义了使用到的IO引脚，将5V(或3.3V)、GND 和 Dat 引脚连接好后即可。

代码中主要包括两个部分，一是对DHT11的驱动代码，二是向云端发送的代码。


###编译、部署和运行

在云端登录自己的账户，确定设备在线后，可以使用编译器来编译上面的代码，部署时确定名称为“otdata”，然后 运行之。
只要连接正常，模块就可以正常读取传感器的数据了。代码中每秒钟都会去读取一次，只有数据变化时才会组织json串提交数据给云端（这时LED灯会闪烁）。

###代码说明

	n += _snprintf(json+n, sizeof(json)-n, "{\"method\":\"data\",\"params\":{\"value\":%u,\"tags\":[\"humidity\"]}}", humi_h); 	//可以尝试添加新的tag
	api_ot.notify(json, n);

首先构造了一个json串，类似于：

	{
	  "method":"data",
	  "params":{
	    "value":xx,
	    "tags":["humidity"]
	  }
	}

可见，和下行控制一样，上行请求服务也是使用了 JSON RPC的形式，目标字段是 data，参数采用了 value和tags两个字段来描述。

而提交给云端仅仅就是调用 api_ot.notify(json, n)。

###我的数据

登录wifi.io网站后，确定能够正常在网站“我的设备”栏目下看到是在线状态。
程序运行起来后（数据提交时LED灯会闪烁），模块的数据应该就提交的云端数据库了。登录网站，通过“个人中心”->“我的数据”，搜索“humidity”，你便可以看到自己的数据了。

wifi.io提供的data接口服务是一个基于tag的数据存储服务，设备提交的数据都是由标签关联的，所以，在addon代码中，你还可以添加其他的标签，类如"dev15"表示第15号设备，在“我的数据”界面，你需要使用“humidity”和“dev15”两个关键词才能定位到相关数据曲线了。
这种方式可以很简单的实现数据关联，利于后期检索查询。


****
wifi.io还提供了转发服务forward，用户可以使用类似下面的代码，将一个json串POST到任意指定地址去。

	int sms(void)
	{
		char json[256];
		u32_t n = 0;
		n += _snprintf(json+n, sizeof(json)-n, 
			"{\"method\":\"forward\",
			  \"params\":{
				\"url\":\"http://api.wifi.io/sms/send.php\",
				\"content\":{\"tel\":136710567xx,\"msg\":\"发条短信骚扰一下~！\"}
				}
			 }"); 
		api_ot.notify(json, n);
		return STATE_OK;
	}

上面的例子，就可以将json：{"tel":136710567xx,"msg":"发条短信骚扰一下~！"} POST给http://api.wifi.io/sms/send.php。

通过这个接口，就可以实现模块和你自己的服务通讯啦。




更多细节请参考源代码。

20131006
问题和建议请email: dy@wifi.io 

