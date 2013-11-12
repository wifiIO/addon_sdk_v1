/**
 * @file			hash_test.c
 * @brief			hash测试程序
 *	对字串进行hash计算，加密结果可以比对 注释结果
 * @author			dy@wifi.io
*/


#include "include.h"










/*
SHA1("The quick brown fox jumps over the lazy dog") = 2fd4e1c6 7a2d28fc ed849ee1 bb76e739 1b93eb12

*/

int main(int argc, char* argv[])
{
	char str[] = "The quick brown fox jumps over the lazy dog";
	char buf[256];
	unsigned char out[20];

	api_crypto.sha1((unsigned char *)str, _strlen(str), out);
	utl.snprintf_hex(buf, sizeof(buf), (const u8_t *)out, sizeof(out), 0x80|' ');
	LOG_INFO("sha1 result:\"%s\".\r\n", buf);

	return ADDON_LOADER_ABORT;	//删除该addon (无需驻留)
}



