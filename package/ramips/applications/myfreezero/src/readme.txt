CGI接口：
（URL:http:\\127.0.0.1\cgi-bin\test.cgi?net=eth0&id=blk)
 访问URL参数为：net=eth0&id=blk  其中net:发送MACADDR的网络接口，id：为厂商的ID

本程序第一次访问 alpha.veryci.cc：3003\init获取SEED.
然后用MAC+ID加密，然后跳转：http:\\alpha.veryci.cc:3003\reg?k=密码

命令测试接口：
./test.cgi net0 aabbccddeeffggiijjkk  其中net:发送MACADDR的网络接口，id：为厂商的产品ID（由非常云分配）