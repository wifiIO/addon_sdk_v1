/**
 * @file			mirror.c
 * @brief			第一届硬件黑客马拉松冠军作品
 *					http://www.csdn.net/article/2013-08-25/2816687-xiaomi-HAXLR8on-in-shenzhen
 *					http://www.csdn.net/article/2013-08-28/2816722-Hardware-Hackathon-Documentary
 *					http://wifi.io/blog.php?id=523abac3a4c32057613ee44c
 * @author			dy@wifi.io
*/


#include "include.h"


#define PIN_OE 	WIFIIO_GPIO_21
#define PIN_LAT	WIFIIO_GPIO_22
#define PIN_SCK	WIFIIO_GPIO_23

#define PIN_A 	WIFIIO_GPIO_14
#define PIN_B		WIFIIO_GPIO_15
#define PIN_C		WIFIIO_GPIO_16
#define PIN_D		WIFIIO_GPIO_17

#define PIN_DAT_R1	WIFIIO_GPIO_01
#define PIN_DAT_R2	WIFIIO_GPIO_04
#define PIN_DAT_B1	WIFIIO_GPIO_02
#define PIN_DAT_B2	WIFIIO_GPIO_12
#define PIN_DAT_G1	WIFIIO_GPIO_03
#define PIN_DAT_G2	WIFIIO_GPIO_13

#define PIN_R1_LOW api_io.low(PIN_DAT_R1)
#define PIN_R1_HIGH api_io.high(PIN_DAT_R1)
#define PIN_R2_LOW api_io.low(PIN_DAT_R2)
#define PIN_R2_HIGH api_io.high(PIN_DAT_R2)

#define PIN_G1_LOW api_io.low(PIN_DAT_G1)
#define PIN_G1_HIGH api_io.high(PIN_DAT_G1)
#define PIN_G2_LOW api_io.low(PIN_DAT_G2)
#define PIN_G2_HIGH api_io.high(PIN_DAT_G2)

#define PIN_B1_LOW api_io.low(PIN_DAT_B1)
#define PIN_B1_HIGH api_io.high(PIN_DAT_B1)
#define PIN_B2_LOW api_io.low(PIN_DAT_B2)
#define PIN_B2_HIGH api_io.high(PIN_DAT_B2)



#define PIN_OE_LOW api_io.low(PIN_OE)
#define PIN_OE_HIGH api_io.high(PIN_OE)

#define PIN_SCK_LOW api_io.low(PIN_SCK)
#define PIN_SCK_HIGH api_io.high(PIN_SCK)

#define PIN_LAT_LOW api_io.low(PIN_LAT)
#define PIN_LAT_HIGH api_io.high(PIN_LAT)

#define PIN_A_LOW api_io.low(PIN_A)
#define PIN_A_HIGH api_io.high(PIN_A)

#define PIN_B_LOW api_io.low(PIN_B)
#define PIN_B_HIGH api_io.high(PIN_B)

#define PIN_C_LOW api_io.low(PIN_C)
#define PIN_C_HIGH api_io.high(PIN_C)

#define PIN_D_LOW api_io.low(PIN_D)
#define PIN_D_HIGH api_io.high(PIN_D)





#define COLOR_BLUE	0x80
#define COLOR_GREEN	0x40
#define COLOR_RED	0x20

#define NROW_NUM 8
#define NCOL_NUM 32


volatile u8_t row,  *buffptr;
u8_t *matrixbuff[2],  swapflag, backindex;

#define ICON8_WIDTH	8
#define ICON16_WIDTH	16
#define ICON_NUM	3

const u8_t icon_8x8_cloud[ICON_NUM*ICON8_WIDTH] = {
	0x06, 0x69, 0x91, 0x81, 0x81, 0x7e, 0x00, 0x00,	//cloud1
	0x06, 0x29, 0x51, 0x81, 0x81, 0x7e, 0x00, 0x00,	//cloud2
	0x06, 0x09, 0x71, 0x81, 0x81, 0x7e, 0x00, 0x00,	//cloud3
};

const u8_t icon_8x8_rain[ICON_NUM*ICON8_WIDTH] = {
	0x70, 0x8b, 0x85, 0x81, 0xff, 0x55, 0x14, 0x54,	//rain1
	0x70, 0x8b, 0x85, 0x81, 0xff, 0x55, 0x55, 0x54,	//rain2
	0x00, 0x70, 0x8b, 0x85, 0x81, 0xff, 0x55, 0x55,	//rain3
};

const u8_t icon_8x8_sun[ICON_NUM*ICON8_WIDTH] = {
	0x91, 0x42, 0x18, 0x25, 0xa4, 0x18, 0x42, 0x89,	//sun1
	0x12, 0x84, 0x58, 0x25, 0xa4, 0x1a, 0x21, 0x48,	//sun2
	0x81, 0x5a, 0x3c, 0x66, 0x66, 0x3c, 0x5a, 0x81,	//sun3
};

const u8_t icon_8x8_C[ICON8_WIDTH] = {
	0x40, 0xac, 0x52, 0x10, 0x12, 0x0c, 0x00, 0x00
};

const u8_t icon_16x16_haxlr[ICON16_WIDTH*2] = {
	0x40, 0x00,
	0x20, 0x38,
	0x10, 0x70,
	0x08, 0x72,
	0x04, 0x3E,
	0x03, 0x7E,
	0x03, 0xAC,
	0x01, 0xC0,
	0x02, 0xF8,
	0x37, 0x74,
	0x7E, 0x7A,
	0x7C, 0x5D,
	0x4E, 0x2F,
	0x0E, 0x17,
	0x1C, 0x0E,
	0x00, 0x00,
};


const u8_t font_5x7[][5] = {
        { 0x00, 0x00, 0x00, 0x00, 0x00 },               /*   - 0x20 - 32 */
        { 0x00, 0x00, 0x5f, 0x00, 0x00 },               /* ! - 0x21 - 33 */
        { 0x00, 0x07, 0x00, 0x07, 0x00 },               /* " - 0x22 - 34 */
        { 0x14, 0x7f, 0x14, 0x7f, 0x14 },               /* # - 0x23 - 35 */
        { 0x24, 0x2a, 0x7f, 0x2a, 0x12 },               /* $ - 0x24 - 36 */
        { 0x23, 0x13, 0x08, 0x64, 0x62 },               /* % - 0x25 - 37 */
        { 0x36, 0x49, 0x55, 0x22, 0x50 },               /* & - 0x26 - 38 */
        { 0x00, 0x05, 0x03, 0x00, 0x00 },               /* ' - 0x27 - 39 */
        { 0x00, 0x1c, 0x22, 0x41, 0x00 },               /* ( - 0x28 - 40 */
        { 0x00, 0x41, 0x22, 0x1c, 0x00 },               /* ) - 0x29 - 41 */
        { 0x14, 0x08, 0x3e, 0x08, 0x14 },               /* * - 0x2a - 42 */
        { 0x08, 0x08, 0x3e, 0x08, 0x08 },               /* + - 0x2b - 43 */
        { 0x00, 0x50, 0x30, 0x00, 0x00 },               /* , - 0x2c - 44 */
        { 0x08, 0x08, 0x08, 0x08, 0x08 },               /* - - 0x2d - 45 */
        { 0x00, 0x60, 0x60, 0x00, 0x00 },               /* . - 0x2e - 46 */
        { 0x20, 0x10, 0x08, 0x04, 0x02 },               /* / - 0x2f - 47 */
        { 0x3e, 0x51, 0x49, 0x45, 0x3e },               /* 0 - 0x30 - 48 */
        { 0x00, 0x42, 0x7f, 0x40, 0x00 },               /* 1 - 0x31 - 49 */
        { 0x42, 0x61, 0x51, 0x49, 0x46 },               /* 2 - 0x32 - 50 */
        { 0x21, 0x41, 0x45, 0x4b, 0x31 },               /* 3 - 0x33 - 51 */
        { 0x18, 0x14, 0x12, 0x7f, 0x10 },               /* 4 - 0x34 - 52 */
        { 0x27, 0x45, 0x45, 0x45, 0x39 },               /* 5 - 0x35 - 53 */
        { 0x3c, 0x4a, 0x49, 0x49, 0x30 },               /* 6 - 0x36 - 54 */
        { 0x01, 0x71, 0x09, 0x05, 0x03 },               /* 7 - 0x37 - 55 */
        { 0x36, 0x49, 0x49, 0x49, 0x36 },               /* 8 - 0x38 - 56 */
        { 0x06, 0x49, 0x49, 0x29, 0x1e },               /* 9 - 0x39 - 57 */
        { 0x00, 0x36, 0x36, 0x00, 0x00 },               /* : - 0x3a - 58 */
        { 0x00, 0x56, 0x36, 0x00, 0x00 },               /* ; - 0x3b - 59 */
        { 0x08, 0x14, 0x22, 0x41, 0x00 },               /* < - 0x3c - 60 */
        { 0x14, 0x14, 0x14, 0x14, 0x14 },               /* = - 0x3d - 61 */
        { 0x00, 0x41, 0x22, 0x14, 0x08 },               /* > - 0x3e - 62 */
        { 0x02, 0x01, 0x51, 0x09, 0x06 },               /* ? - 0x3f - 63 */
        { 0x32, 0x49, 0x79, 0x41, 0x3e },               /* @ - 0x40 - 64 */
        { 0x7e, 0x11, 0x11, 0x11, 0x7e },               /* A - 0x41 - 65 */
        { 0x7f, 0x49, 0x49, 0x49, 0x36 },               /* B - 0x42 - 66 */
        { 0x3e, 0x41, 0x41, 0x41, 0x22 },               /* C - 0x43 - 67 */
        { 0x7f, 0x41, 0x41, 0x22, 0x1c },               /* D - 0x44 - 68 */
        { 0x7f, 0x49, 0x49, 0x49, 0x41 },               /* E - 0x45 - 69 */
        { 0x7f, 0x09, 0x09, 0x09, 0x01 },               /* F - 0x46 - 70 */
        { 0x3e, 0x41, 0x49, 0x49, 0x7a },               /* G - 0x47 - 71 */
        { 0x7f, 0x08, 0x08, 0x08, 0x7f },               /* H - 0x48 - 72 */
        { 0x00, 0x41, 0x7f, 0x41, 0x00 },               /* I - 0x49 - 73 */
        { 0x20, 0x40, 0x41, 0x3f, 0x01 },               /* J - 0x4a - 74 */
        { 0x7f, 0x08, 0x14, 0x22, 0x41 },               /* K - 0x4b - 75 */
        { 0x7f, 0x40, 0x40, 0x40, 0x40 },               /* L - 0x4c - 76 */
        { 0x7f, 0x02, 0x0c, 0x02, 0x7f },               /* M - 0x4d - 77 */
        { 0x7f, 0x04, 0x08, 0x10, 0x7f },               /* N - 0x4e - 78 */
        { 0x3e, 0x41, 0x41, 0x41, 0x3e },               /* O - 0x4f - 79 */
        { 0x7f, 0x09, 0x09, 0x09, 0x06 },               /* P - 0x50 - 80 */
        { 0x3e, 0x41, 0x51, 0x21, 0x5e },               /* Q - 0x51 - 81 */
        { 0x7f, 0x09, 0x19, 0x29, 0x46 },               /* R - 0x52 - 82 */
        { 0x46, 0x49, 0x49, 0x49, 0x31 },               /* S - 0x53 - 83 */
        { 0x01, 0x01, 0x7f, 0x01, 0x01 },               /* T - 0x54 - 84 */
        { 0x3f, 0x40, 0x40, 0x40, 0x3f },               /* U - 0x55 - 85 */
        { 0x1f, 0x20, 0x40, 0x20, 0x1f },               /* V - 0x56 - 86 */
        { 0x3f, 0x40, 0x38, 0x40, 0x3f },               /* W - 0x57 - 87 */
        { 0x63, 0x14, 0x08, 0x14, 0x63 },               /* X - 0x58 - 88 */
        { 0x07, 0x08, 0x70, 0x08, 0x07 },               /* Y - 0x59 - 89 */
        { 0x61, 0x51, 0x49, 0x45, 0x43 },               /* Z - 0x5a - 90 */
        { 0x00, 0x7f, 0x41, 0x41, 0x00 },               /* [ - 0x5b - 91 */
        { 0x02, 0x04, 0x08, 0x10, 0x20 },               /* \ - 0x5c - 92 */
        { 0x00, 0x41, 0x41, 0x7f, 0x00 },               /* ] - 0x5d - 93 */
        { 0x04, 0x02, 0x01, 0x02, 0x04 },               /* ^ - 0x5e - 94 */
        { 0x40, 0x40, 0x40, 0x40, 0x40 },               /* _ - 0x5f - 95 */
        { 0x00, 0x01, 0x02, 0x04, 0x00 },               /* ` - 0x60 - 96 */
        { 0x20, 0x54, 0x54, 0x54, 0x78 },               /* a - 0x61 - 97 */
        { 0x7f, 0x48, 0x44, 0x44, 0x38 },               /* b - 0x62 - 98 */
        { 0x38, 0x44, 0x44, 0x44, 0x20 },               /* c - 0x63 - 99 */
        { 0x38, 0x44, 0x44, 0x48, 0x7f },               /* d - 0x64 - 100 */
        { 0x38, 0x54, 0x54, 0x54, 0x18 },               /* e - 0x65 - 101 */
        { 0x08, 0x7e, 0x09, 0x01, 0x02 },               /* f - 0x66 - 102 */
        { 0x38, 0x44, 0x44, 0x54, 0x34 },               /* g - 0x67 - 103 */
        { 0x7f, 0x08, 0x04, 0x04, 0x78 },               /* h - 0x68 - 104 */
        { 0x00, 0x44, 0x7d, 0x40, 0x00 },               /* i - 0x69 - 105 */
        { 0x20, 0x40, 0x44, 0x3d, 0x00 },               /* j - 0x6a - 106 */
        { 0x7f, 0x10, 0x28, 0x44, 0x00 },               /* k - 0x6b - 107 */
        { 0x00, 0x41, 0x7f, 0x40, 0x00 },               /* l - 0x6c - 108 */
        { 0x7c, 0x04, 0x18, 0x04, 0x78 },               /* m - 0x6d - 109 */
        { 0x7c, 0x08, 0x04, 0x04, 0x78 },               /* n - 0x6e - 110 */
        { 0x38, 0x44, 0x44, 0x44, 0x38 },               /* o - 0x6f - 111 */
        { 0x7c, 0x14, 0x14, 0x14, 0x08 },               /* p - 0x70 - 112 */
        { 0x08, 0x14, 0x14, 0x18, 0x7c },               /* q - 0x71 - 113 */
        { 0x7c, 0x08, 0x04, 0x04, 0x08 },               /* r - 0x72 - 114 */
        { 0x48, 0x54, 0x54, 0x54, 0x20 },               /* s - 0x73 - 115 */
        { 0x04, 0x3f, 0x44, 0x40, 0x20 },               /* t - 0x74 - 116 */
        { 0x3c, 0x40, 0x40, 0x20, 0x7c },               /* u - 0x75 - 117 */
        { 0x1c, 0x20, 0x40, 0x20, 0x1c },               /* v - 0x76 - 118 */
        { 0x3c, 0x40, 0x30, 0x40, 0x3c },               /* w - 0x77 - 119 */
        { 0x44, 0x28, 0x10, 0x28, 0x44 },               /* x - 0x78 - 120 */
        { 0x0c, 0x50, 0x50, 0x50, 0x3c },               /* y - 0x79 - 121 */
        { 0x44, 0x64, 0x54, 0x4c, 0x44 },               /* z - 0x7a - 122 */
        { 0x00, 0x08, 0x36, 0x41, 0x00 },               /* { - 0x7b - 123 */
        { 0x00, 0x00, 0x7f, 0x00, 0x00 },               /* | - 0x7c - 124 */
        { 0x00, 0x41, 0x36, 0x08, 0x00 },               /* } - 0x7d - 125 */
        { 0x10, 0x08, 0x08, 0x10, 0x08 },               /* ~ - 0x7e - 126 */
};

int is_mirror_on = 0;

void updateDisplay(u8_t *frame_buf) 
{
	int i;
	PIN_OE_HIGH;
	PIN_LAT_HIGH;

	if(++row >= NROW_NUM)
		row     = 0;

	u8_t addr = row -1;
	if(addr & 0x1) PIN_A_HIGH; else PIN_A_LOW;
	if(addr & 0x2) PIN_B_HIGH; else PIN_B_LOW;
	if(addr & 0x4) PIN_C_HIGH; else PIN_C_LOW;

	u8_t *ptr_top = (u8_t *)frame_buf + NCOL_NUM*row;
	u8_t *ptr_bot = (u8_t *)frame_buf + NCOL_NUM*row + NCOL_NUM*NROW_NUM;

	PIN_OE_LOW;	   // Re-enable output
	PIN_LAT_LOW; // Latch down

	for(i=0; i<NCOL_NUM; i++) {
		if(ptr_top[i] & 0x80  && is_mirror_on) PIN_B1_HIGH;else PIN_B1_LOW;
		if(ptr_top[i] & 0x40  && is_mirror_on) PIN_G1_HIGH;else PIN_G1_LOW;
		if(ptr_top[i] & 0x20  && is_mirror_on) PIN_R1_HIGH;else PIN_R1_LOW;

		if(ptr_bot[i] & 0x80  && is_mirror_on) PIN_B2_HIGH;else PIN_B2_LOW;
		if(ptr_bot[i] & 0x40  && is_mirror_on) PIN_G2_HIGH;else PIN_G2_LOW;
		if(ptr_bot[i] & 0x20  && is_mirror_on) PIN_R2_HIGH;else PIN_R2_LOW;

		PIN_SCK_HIGH;
		PIN_SCK_LOW;
	} 

}


void drawPixel(int x, int y, u8_t c)
{
	buffptr = matrixbuff[backindex];
	if(x >= NCOL_NUM)return;
	if(y >= 2*NROW_NUM)return;
	if(x < 0)return;
	if(y < 0)return;

	buffptr[y*NCOL_NUM + x]	= c;
}


//画线
void line(int  x1, int  y1, int  x2, int y2, u8_t color)
{
	int  x, y, t;
	if((x1 == x2) && (y1 == y2))
		drawPixel(x1, y1, color);
	else if(ABS(y2 - y1) > ABS(x2 - x1))
	{
		if(y1 > y2)
		{
			t = y1;
			y1 = y2;
			y2 = t;
			t = x1;
			x1 = x2;
			x2 = t;
		}
		for(y = y1; y <= y2; y ++)
		{
			x = (y - y1) * (x2 - x1) / (y2 - y1) + x1;
			drawPixel(x, y, color);
		}
	}
	else
	{
		if(x1 > x2)
		{
			t = y1;
			y1 = y2;
			y2 = t;
			t = x1;
			x1 = x2;
			x2 = t;
		}
		for(x = x1; x <= x2; x ++)
		{
			y = (x - x1) * (y2 - y1) / (x2 - x1) + y1;
			drawPixel(x, y, color);
		}
	}
}

void rectangle(int x1,int y1,int x2,int y2, u8_t color)
{
	line(x1,y1,x2,y1, color);
	line(x1,y1,x1,y2, color);
	line(x1,y2,x2,y2, color);
	line(x2,y1,x2,y2, color);
}
void filled_rectangle(int x1,int y1,int x2,int y2, u8_t color)
{
	int i;
	for(i=y1; i<=y2; i++)
	{
		line(x1,i,x2,i, color);
	}
}

//在指定位置画一个指定大小的圆
//(x,y):中心点
//r    :半径
void circle(int x0,int y0,u8_t r, u8_t color)
{
	int a,b;
	int di;
	a=0;
	b=r;
	di=3-2*r;             //判断下个点位置的标志
	while(a<=b)
	{
		drawPixel(x0-b,y0-a, color);             //3           
		drawPixel(x0+b,y0-a, color);             //0           
		drawPixel(x0-a,y0+b, color);             //1       
		drawPixel(x0-b,y0-a, color);             //7           
		drawPixel(x0-a,y0-b, color);             //2             
		drawPixel(x0+b,y0+a, color);             //4               
		drawPixel(x0+a,y0-b, color);             //5
		drawPixel(x0+a,y0+b, color);             //6 
		drawPixel(x0-b,y0+a, color);             
		a++;
		/***使用Bresenham算法画圆**/     
		if(di<0)di +=4*a+6;	  
		else
		{
			di+=10+4*(a-b);   
			b--;
		} 
		drawPixel(x0+a,y0+b, color);
	}
}

void put_icon8x8(int x, int y, const u8_t*p, char color, char bkcolor)
{
	u8_t tmp_char = 0, col, row;

	for (row=0; row<8; row++){

		tmp_char = p[row];
		for (col = 0; col < 8; col++)
		{
			if ( (tmp_char >> (7-col)) & 0x01)
				drawPixel(x+col, y+row, color); // 字符颜色
			else
				drawPixel(x+col, y+row, bkcolor); // 背景颜色
		}
	}
}

void put_char5x7(int x, int y, int c, char color, char bkcolor)
{
	u8_t tmp_char = 0, col, row;
	u8_t *font = (u8_t*)font_5x7[c - 0x20];

	for (col=0; col<5; col++){

		tmp_char = font[col];
		for (row = 0; row < 8; row++)
		{
			if ( (tmp_char >> row) & 0x01)
				drawPixel(x+col, y+row, color); // 字符颜色
			else
				drawPixel(x+col, y+row, bkcolor); // 背景颜色
		}
	}
}



void put_icon16x16(int x, int y, const u8_t* font, char color, char bkcolor)
{
	u8_t tmp_1st_char = 0, tmp_2nd_char = 0, col, row;
	for (row=0; row<16; row++){
		tmp_1st_char = font[2*row];
		tmp_2nd_char = font[2*row+1];
		for (col = 0; col < 8; col++)
		{
			if ( (tmp_1st_char >> (7-col)) & 0x01)
				drawPixel(x+col, y+row, color); // 字符颜色
			else
				drawPixel(x+col, y+row, bkcolor); // 背景颜色
			if ( (tmp_2nd_char >> (7-col)) & 0x01)
				drawPixel(x+col + 8, y+row, color); // 字符颜色
			else
				drawPixel(x+col + 8, y+row, bkcolor); // 背景颜色
		}
	}
}

void put_icons16x16(int x, int y, const u8_t* font, int num, char color, char bkcolor)
{
	int i;
	for(i = 0; i < num; i++){
		put_icon16x16(x+i*16, 0, font+32, color, bkcolor);
	}
}



//返回str点阵长度
void put_string5x7(int x, int y, char* str, char color, char bkcolor)
{
	int i, str_len = _strlen(str);

	for(i = 0; i < str_len; i++)
		put_char5x7(x + 6*i, y, str[i], color, bkcolor);

}



void frame_flip(void)
{
	backindex = 1 - backindex;

	_memset(matrixbuff[backindex], 0, NCOL_NUM*NROW_NUM*2);
}



char msg[128] = "Hello from memo mirror.;-) ...";
char msg_new[128] = "";
char msg_flag = 0;

char day = 'S';
u8_t temp = 30;

u8_t flip_mutex = 0;
u8_t end_flag = 0;

int scroll_thread(void* p)
{
	_tick_sleep(5000);
//	is_mirror_on = 0;

	//文字更新
	while(!end_flag){
		int width =  _strlen(msg) * 6;	//当前msg的点阵长度
		int pos = NCOL_NUM;

		//当没有新的消息时 循环滚动文字
		while(0 == msg_flag){

			flip_mutex ++;
			
			put_string5x7(pos--, 8, msg, COLOR_BLUE, 0);


			flip_mutex --;

			if(pos + width < 0)
				pos = NCOL_NUM;

			_tick_sleep(100);
		}

		_strcpy(msg, msg_new);	//更新msg	并清空flag
		msg_flag = 0;
	}

	return 0;
}



#define IR_SEND_PIN WIFIIO_GPIO_15	//WIFIIO_PWM_03CH1	使用硬件PWM产生38K
#define IR_RECV_PIN WIFIIO_GPIO_16
const char *hello = "hi";
const char *bye = "bye";
char is_anybody = 0;

int ot_inform(int exsit)
{
	char json[256];
	u32_t n = 0;
	n += _snprintf(json+n, sizeof(json)-n, "{\"method\":\"forward\",\"params\":{\"url\":\"http://memo.wifi.io/mirror.php\",\"content\":{\"f\":%u}}}", exsit); 
	return api_ot.notify(json, n);
}

int ir_thread(void* p)
{
	int cnt = 0;
	api_io.init(IR_RECV_PIN, IN_PULL_UP);
	api_pwm.init(WIFIIO_PWM_03CH1, 6, 224, 80);	//0.1us 8/26.3us 


	_tick_sleep(5000);
//	is_mirror_on = 0;


	while(!end_flag)
	{
		api_pwm.start(WIFIIO_PWM_03CH1);	//发射红外
		api_os.tick_sleep(200);	

		if(!is_anybody ){//没人状态时 
			if(0 == api_io.get(IR_RECV_PIN)){	//有人导致反射
				LOG_INFO("(*)");
				if(++cnt >= 3){
					is_anybody = 1;
					ot_inform(is_anybody);
					is_mirror_on = 1;
					//_strcpy(msg_new,hello);
					//msg_flag = 1;
					cnt = 0;
				}
			}
			else
				cnt = 0;
		}

		else{		//有人状态时
			if(api_io.get(IR_RECV_PIN)){	//无人反射
				LOG_INFO("(_)");
				if(++cnt >= 3){
					is_anybody = 0;
					ot_inform(is_anybody);
					is_mirror_on = 0;
					//_strcpy(msg_new,bye);
					//msg_flag = 1;
					cnt = 0;
				}
			}
			else
				cnt = 0;
		}
		
		api_pwm.stop(WIFIIO_PWM_03CH1);
		api_os.tick_sleep(200);	

	}

	return 0;

}

int weather_thread(void* p)
{
	int idx = 0;
	char str[8];

	_tick_sleep(5000);
//	is_mirror_on = 0;


	//文字更新
	while(!end_flag)
	{
		const u8_t*p;
		char color;


	flip_mutex ++;

		_snprintf(str, sizeof(str), "%02u", temp);
		put_string5x7(0, 0, str, COLOR_GREEN, 0);	//左上显示温度
		put_icon8x8(12, 0, icon_8x8_C, COLOR_GREEN, 0);

	
		if(day == 'S'){
			p = &icon_8x8_sun[idx*ICON8_WIDTH];
			color = COLOR_RED;
		}
		else if(day == 'C'){
			p = &icon_8x8_cloud[idx*ICON8_WIDTH];
			color = COLOR_GREEN;
		}
		else if(day == 'R'){
			p = &icon_8x8_rain[idx*ICON8_WIDTH];
			color = COLOR_BLUE;
		}
		else{
			p = &icon_8x8_cloud[idx*ICON8_WIDTH];
			color = COLOR_GREEN;
		}

		put_icon8x8(22, 0, p, color, 0);
		

	flip_mutex --;



		_tick_sleep(100);

		idx++; idx %= ICON_NUM;
	}

	return 0;
}



int icon_thread(void* p)
{
	int tick = _tick_now();
	//文字更新
	while(_tick_elapsed(tick) < 5000){
		flip_mutex ++;
		put_icon16x16(8, 0, icon_16x16_haxlr, COLOR_RED, 0);
		flip_mutex --;
		_tick_sleep(100);
	}

	//等待其他进程结束
	while(!end_flag){	
		_tick_sleep(500);
	}




	return 0;
}



int main(int argc, char* argv[])
{
	api_io.init(PIN_OE, OUT_PUSH_PULL);
	api_io.init(PIN_A, OUT_PUSH_PULL);
	api_io.init(PIN_B, OUT_PUSH_PULL);
	api_io.init(PIN_SCK, OUT_PUSH_PULL);
	api_io.init(PIN_LAT, OUT_PUSH_PULL);

	api_io.init(PIN_DAT_R1, OUT_PUSH_PULL);
	api_io.init(PIN_DAT_B1, OUT_PUSH_PULL);
	api_io.init(PIN_DAT_G1, OUT_PUSH_PULL);

	api_io.init(PIN_DAT_R2, OUT_PUSH_PULL);
	api_io.init(PIN_DAT_B2, OUT_PUSH_PULL);
	api_io.init(PIN_DAT_G2, OUT_PUSH_PULL);


	PIN_SCK_LOW;
	PIN_LAT_LOW;
	PIN_OE_HIGH;

	PIN_A_LOW;
	PIN_B_LOW;
	PIN_C_LOW;

	PIN_R1_LOW;
	PIN_R2_LOW;
	PIN_G1_LOW;
	PIN_G2_LOW;
	PIN_B1_LOW;
	PIN_B2_LOW;


	int buffsize  = NCOL_NUM * NROW_NUM * 2;

	matrixbuff[0] = (u8_t *)_malloc(buffsize * 2);
	_memset(matrixbuff[0], 0, buffsize * 2);
	matrixbuff[1] = &matrixbuff[0][buffsize];

	row       = 0;
	backindex = 1;


//	drawPixel(8, 8, COLOR_GREEN);
//	rectangle(0,0,31,15,COLOR_BLUE);
//	circle(15,8,4,COLOR_RED);
//	put_char5x7(2, 2, 'A', COLOR_GREEN, 0);
//	frame_flip();

	os_thread_t* pthread;
	_thread_create(&pthread, "ir", ir_thread, 2048, (void*)0);	//建立一个进程 专门用于红外检测
	_thread_create(&pthread, "scroll", scroll_thread, 512, (void*)0);	//建立一个进程 专门用于文字滚动
	_thread_create(&pthread, "weather", weather_thread, 1560, (void*)0);	//建立一个进程 专门用于icon变化

	_thread_create(&pthread, "icon", icon_thread, 512, (void*)0);	//建立一个进程 专门用于icon变化


	is_mirror_on = 1;

	u32_t tick = _tick_now();
	//刷新进程
	while(1)
	{
		updateDisplay(matrixbuff[1-backindex]);

		if(flip_mutex == 0 && _tick_elapsed(tick) >= 100){
			tick = _tick_now();
			frame_flip();
		}
	}



//	return ADDON_LOADER_GRANTED;
//err_exit:
	return ADDON_LOADER_ABORT;		//release 
}








/*
下面的这个接口 可以通过模块自己的委托接口测试界面来调试。
{
	"method":"mirror.msg",
	"params":"hello"
}
*/

int __ADDON_EXPORT__ JSON_DELEGATE(msg)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;

	//下面从json中提取命令字字符串
	if(JSMN_STRING != pjn->tkn->type || 
	     STATE_OK != jsmn.tkn2val_str(pjn->js, pjn->tkn, msg_new, sizeof(msg_new), NULL)){	//pjn->tkn 指向json中params字段的名值对，使用tkn2val_str()从中提取string类型的value
		ret = STATE_ERROR;	err_msg = "Params error."; 
		LOG_WARN("mirror:%s.\r\n", err_msg);
		goto exit_err;
	}

	msg_flag = 1;
	is_mirror_on = 1;

	if(ack)jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}


/*
	这个接口用于下行通知模块更改天气状态
{
	"method":"mirror.weather",
	"params":{"temperature":34, "day":"S"/"C"/"R"}
}
*/
int __ADDON_EXPORT__ JSON_DELEGATE(weather)(jsmn_node_t* pjn, fp_json_delegate_ack ack, void* ctx)	//继承于fp_json_delegate_start
{
	int ret = STATE_OK;
	char* err_msg = NULL;
	char str[4];

	//下面从json中提取命令字字符串
	if( STATE_OK != jsmn.key2val_str(pjn->js, pjn->tkn, "day", str, sizeof(str), NULL) ||
	     STATE_OK != jsmn.key2val_u8(pjn->js, pjn->tkn, "temperature", &temp)){	//pjn->tkn 指向json中params字段的名值对，使用tkn2val_str()从中提取string类型的value
		ret = STATE_ERROR;	err_msg = "Params error."; 
		LOG_WARN("weather:%s.\r\n", err_msg);
		goto exit_err;
	}

	if('S' == str[0] || 'C' == str[0] || 'R' == str[0]){
		day = str[0];	//更新命令字，第一个字符就是命令
		is_mirror_on = 1;

	}
	else {
		ret = STATE_ERROR;
		err_msg = "Params invalid.";
		LOG_WARN("dollcatch:%s.\r\n", err_msg);
		goto exit_err;
	}

	if(ack)jsmn.delegate_ack_result(ack, ctx, ret);
	return ret;

exit_err:
	if(ack)jsmn.delegate_ack_err(ack, ctx, ret, err_msg);
	return ret;
}




