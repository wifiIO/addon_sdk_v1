#SerialTCP(Client): 实现TCP客户端与串口的交互程序


##概述

Stcpc是一个传统的串口转网络（tcp客户端模式）的应用程序实现。

模块会主动连接预设置的tcp服务器，建立连接后，将串口数据和TCP目标进行数据交换。

本示例演示了基于tcp客户端框架的程序设计方法，复杂json文件的解析过程 以及 串口详细的操作方法。

##如何使用

1. 安装stcpc.add插件到模块（如果从云端部署，请确定以stcpc名称实施，这将保证程序被安装在“/app/stcpc”目录下）；
2. 上传index.html、stcpc.js 和 cfg.json文件到"/app/stcpc"目录；
3. 在其他平台上运行tcp服务器程序，为模块接入做好准备；
4. 打开 模块应用主页 http://192.168.1.xxx/app/stcpc/index.htm 
到配置页面设置相应参数（例如，目标IP或者域名）。
5. 运行模块上stcpc应用（通过云端或者本地）；
6. 尝试从TCP服务端或者串口侧向连接输入数据，观察对方输出。

 *更详细的配置内容，请查看应用主页中“使用说明”的选项卡。*

##备注
cfg.json作为本应用程序的配置文件存在，其由同目录下的index.html和stcpc.js 生成和管理。
stcpc.add运行初需要读取“/app/stcpc/cfg.json”（看代码main函数），如果该文件不存在或者格式不正确，stcpc都无法正常工作。



##依赖
无


****
更多细节请参考源代码。

20131006
问题和建议请email: yizuoshe@gmail.com 


