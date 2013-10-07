#LED点阵屏: 如何驱动P10单元板


##概述

 LED点阵屏已经深入到人们的生活，LED面板也被标准化和单元化。
 
 P10单元板是点阵间距为1cm的单色LED板模块。
 
 本例演示如何驱动这种LED板。
 
 ![P10_led_panel](../../addons_img/led_panel_p10.jpg?raw=true)

 


##使用说明

P10单元板为32x16的分辨率。
wifiIO使用6个引脚实现对单元板的驱动，如下图：
![P10_led_panel](../../addons_img/led_panel_p10_if12.jpg?raw=true)

A B  OE STB CLK DAT(R)

点阵竖向分为4个panel，每个panel 4个点高度，Panel由A、B引脚指定；
单一panel内，使用STB信号切换每一行，一行内横向32个CLK节拍，同时输出DAT设置每一点数据。


本程序运行起来时便可以看到点阵上显示方框和圆形。

****

更多细节请参考源代码。

20131006
问题和建议请email: dy@wifi.io 

