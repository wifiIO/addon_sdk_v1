sweb使用说明
==================

概要
----
sweb插件用来提供httpd对串口控制的支持。sweb安装运行后，可以对http服务发布GET或者POST方式访问模块串口。

前者可以直接在浏览器地址栏里输入，即刻获取回应。而后者需要使用javascript编写相应脚本通过AJAX方式获取数据；前者简单方便，可以应用在wap浏览器上；后者功能灵活强大，利于数据上下交换。

快速测试
--------
1. 安装sweb.add插件到模块，确定其已经被加载；
2. 可以短接模块串口2收发引脚；
3. 在浏览器地址栏输入，例如：
http://192.168.x.x/logic/sweb/serial?target=hello&wait=1000
回车，一秒钟后，便可以在浏览器窗口看到发出但是被回传的字符串“hello”。

该测试演示了通过GET方式与模块进行数据交换。

更多测试请参考应用的相应页面。

使用说明
--------

**用GET方式与串口数据交换**

向模块http服务器发出GET请求，目标是：

http://192.168.1.x/logic/sweb/serial?target=hello&wait=1000

支持参数字段如下：

* target: 在读取串口数据之前，模块发送该字符串到串口。省略该参数，则仅仅从串口buf中读取一次数据。
* urlenc: 定义数据交换是否使用url编码方式，可选 true/false，示例：urlenc=true，省略该参数缺省是不用到url编码；（关于url编解码，请看尾部说明）
在GET方式中 开启该选项，不会对target参数的字符串进行解码，仅仅是对模块返回的数据串进行编码。
* wait: 定义等候串口接收的最长时间，该时间窗口中所有串口返回的数据都会被反馈至浏览器，单位ms，范围0～60000，示例：wait=1000。省略该参数缺省是0。
* clean: 指定是否在发送target字串前（如果有的话）清除串口接收缓存，可选 true/false，示例：“clean=true”，省略该参数缺省是false；
* mime: 定义返回类型（content-type），可选如下表，示例：“mime=json”，省略该参数缺省是“text/html”；细节见下面“http服务器mime类型编码表”；


****************


**POST方式与串口数据交换**

和GET方式类似，POST目标类如：
http://192.168.1.x/logic/sweb/serial?wait=1000&clean=true

支持参数字段如下：

* urlenc: 定义数据交换是否使用url编码方式，可选 true/false，示例：urlenc=true，省略该参数缺省是不用到url编码；（关于url编解码，请看尾部附属章节）
POST方式中该选项，会对POST请求body的内容进行解码，也同时对模块返回的数据进行编码。
* wait: 定义等候串口接收的最长时间，该时间窗口中所有串口返回的数据都会被反馈至浏览器，单位ms，范围0～60000，示例：wait=1000。省略该参数缺省是0；
* clean: 指定是否在发送target字串前（如果有的话）清除串口接收缓存，可选 true/false，示例：“clean=true”，省略该参数缺省是false；
* mime: 定义返回类型（content-type），可选如下表，示例：“mime=json”，省略该参数缺省是“text/html”；细节见下面“http服务器mime类型编码表”；

****************

**http服务器mime类型编码表**

* html: "text/html"
* gif: "image/gif"
* png: "image/png"
* jpg: "image/jpeg"
* bmp: "image/bmp"
* ico: "image/x-icon"
* dat: "application/octet-stream"
* js: "application/x-javascript"
* css: "text/css"
* swf: "application/x-shockwave-flash"
* xml: "text/xml"
* json: "application/json"


补充说明：使用URL编码与串口交换数据
--------
由于浏览器缺省会对处理的字符采用utf8方式进行编码，所以没有办法随心所欲的将任意二进制数据发送到模块的串口，模块串口返回的二进制流也无法被浏览器接收处理，为了解决这个问题，可以使用URL编码来解决这个问题。

URLCode编码方式很简单，其类似于一种字符替代机制。" "（空格）替换为"+"，英文字母、数字、"-"、"."、"_"、"~"不变，之外其他所有字符，都使用"%XY"方式直接设置其16进制编码值即可。
例如，“hello 中国”编码后为“hello+%e4%b8%ad%e5%9b%bd”。

wifiIO模块支持URL编解码，利用这种能力，可以在浏览器和串口间传递任意二进制数据。

不采用URLEncode方式的数据交互过程如下：

* 浏览器页面输入：abc中文
* 内部utf8编码为：61 62 63 (abc)E4 B8 AD (中)E6 96 87 (文)
* wifiIO网络接收：61 62 63 E4 B8 AD E6 96 87
* wifiIO串口输出：61 62 63 E4 B8 AD E6 96 87

采用URLEncode方式的数据交互过程如下：

浏览器送到模块：

* 浏览器页面输入：%E4%B8%AD
* 内部utf8编码为：%E4%B8%AD
* wifiIO网络接收：%E4%B8%AD
* wifiIO串口输出：E4 B8 AD


模块到浏览器：

* wifiIO串口输入：00 01 02
* wifiIO网络发送：%00%01%02
* 浏览器接收：%00%01%02 （这时便可以用js来处理字串形成byte数组）


