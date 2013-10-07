#第一届硬件黑客马拉松冠军作品: memo mirror


##概述

比赛细节就不臭屁了，见连接：

http://www.csdn.net/article/2013-08-25/2816687-xiaomi-HAXLR8on-in-shenzhen

http://www.csdn.net/article/2013-08-28/2816722-Hardware-Hackathon-Documentary

http://wifi.io/blog.php?id=523abac3a4c32057613ee44c

用wifiIO模组做了一个可以显示天气信息和留言的镜子（memo mirror），并且可以将镜子前是否有人通知到手机侧。
代码中包含了 驱动一块P6全彩单元板 和 红外发射和接收模块的内容。
 
![P10_led_panel](../../addons_img/led_panel_p6rgb.jpg?raw=true)

可以在下面买到：

http://item.taobao.com/item.htm?id=8478666858


##接线说明

下图说明了单元板的引脚定义：

![P10_led_panel](../../addons_img/led_panel_p6rgb_if.jpg?raw=true)

代码首部定义了相应引脚的宏定义，按照定义内容一一连接即可。
不包括 电源和地，一共需要13个引脚。




关于单元板更多的说明，可以参考连接，非常详尽的材料：

http://learn.adafruit.com/32x16-32x32-rgb-led-matrix?view=all



****

更多细节请参考源代码。

20131006
问题和建议请email: dy@wifi.io 

