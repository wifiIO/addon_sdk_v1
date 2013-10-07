#DHT11: 如何驱动通用型温湿度传感器


##概述

下面的例子要实现对DHT11模块的驱动，并且实现一个主动读取的接口。

DHT11的器件知识：

http://www.adafruit.com/datasheets/DHT11-chinese.pdf


##使用说明

DHT11只需要占用一个IO口，代码头部宏定义中定义了使用到的IO引脚，将5V(或3.3V)、GND 和 Dat 引脚连接好后即可。


###编译、部署和运行

在云端登录自己的账户，确定设备在线后，可以使用编译器来编译上面的代码，部署时确定名称为“otdata”，然后 运行之。
只要连接正常，模块就可以正常读取传感器的数据了。代码中每秒钟都会去读取一次，只有数据变化时才会组织json串提交数据给云端（这时LED灯会闪烁）。

###代码说明

代码中主要包括两个部分，一是对DHT11的驱动代码，二是读取接口的代码。

JSON_RPC委托接口，在示例 helloworld 中有说明，请移步阅读。

###本地控制

在“我的设备”界面找到模块本地的IP地址，访问模块主页。在“应用程序”->“委托接口测试” 填入：

	{
		"method":"dht11.read",
		"params":{}
	}

点击发送，便可以看到如下的JSON串返回。

	{
		"result":{"humidity":xx, "temperature":xxx},
	}

![dht11](../../addons_img/read_DHT11.jpg?raw=true)

这个演示了通过模块的http服务实现JSON RPC的调用。


###远程控制

登录wifi.io网站，“文档资源” -> “调试openAPI”  ，按照说明文字，首先获取Token，之后选择 “向设备发送指令”的API接口。
填入类如：

	{
		"token":"xxxx", //获取的token
		"did":xx, //你的设备编码
		"method":"dht11.read",
		"params":{}
	}

**注意，注释部分需要删除**

提交后便可以看到JSON串返回。




****
更多细节请参考源代码。

20131006
问题和建议请email: dy@wifi.io 

