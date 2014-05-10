#lighting: 8组PWM控制


##概述

这个例子是演示控制8组PWM。同时演示了如何处理比较复杂的json请求。


##接口说明

	{
		"method":"lighting.schedule",
		"params":[
			[val, delay, halfcycle_period, halfcycle_num, 1],
			...
			[val, delay, halfcycle_period, halfcycle_num, 2, 3],
		]
	}

* val:	0~99
* delay:	unsigned 32bits	当前延时动作秒数
* period:	半周期时间
* num:	半周期数量（奇数最终状态反转，偶数最终回归原状态）
* 编号：8组PWM通过编号，可以任意组合指定

****

更多细节请参考源代码。

20131006
问题和建议请email: yizuoshe@gmail.com 

