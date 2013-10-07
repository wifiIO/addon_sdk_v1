#AES加密: 让你的数据更加安全


##概述

这个例子演示如何使用加密库实现AES加密；同时演示了文件系统API的操作。


##使用说明

首先需要上传到文件系统根目录下一个 raw文件，尺寸需要小于10K。

程序运行后 会将其加密为 raw.aes 文件。

加密结果可以比对使用Openssl加密的效果：

openssl.exe enc -aes-128-cbc -in file_in -K 0123456789ABCDEF0123456789ABCDEF -iv FEDCBA9876543210FEDCBA9876543210 -out file_out



****

更多细节请参考源代码。

20131006
问题和建议请email: dy@wifi.io 

