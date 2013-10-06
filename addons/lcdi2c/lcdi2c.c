



#include "include.h"



#define BIT_EN 0x04 // Enable bit
#define BIT_RW 0x02 // Read/Write bit
#define BIT_RS 0x01 // Register select bit
#define BIT_BKLT 0x08
#define BIT_NOBKLT 0x00	// flags for backlight control





/*
*/


#include "lcdi2c.h"


// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00



typedef struct{
	i2c_sim_dev_t i2c_sim_dev;
	u8_t _Addr;
	u8_t _displayfunction;
	u8_t _displaycontrol;
	u8_t _displaymode;
	u8_t _numlines;
	u8_t _cols;
	u8_t _rows;
	u8_t _backlightval;
}lcd_i2c_dev_t;
static lcd_i2c_dev_t lcd_i2c_dev;




// ************ low level data pushing commands **********

#define iic_cmd(value) send(value, 0)
#define iic_write(value) send(value, BIT_RS)



void expanderWrite(u8_t dat){
	int ret;
	dat |= lcd_i2c_dev._backlightval;
	ret = i2c_sim.write(&lcd_i2c_dev.i2c_sim_dev, lcd_i2c_dev._Addr, &dat, 1);
	if(ret != STATE_OK)
		LOG_WARN("expanderWrite err.%d\r\n", ret);
}

void pulseEnable(u8_t dat){
	expanderWrite(dat | BIT_EN);	// BIT_EN high
	api_tim.tim6_poll(1);		// enable pulse must be >450ns
	
	expanderWrite(dat & ~BIT_EN);	// BIT_EN low
	api_tim.tim6_poll(50);		// commands need > 37us to settle
} 


void write4bits(u8_t value) {
	expanderWrite(value);
	pulseEnable(value);
}


// write either cmd or data
void send(u8_t value, u8_t mode) {
	u8_t highnib=value&0xf0;
	u8_t lownib=(value<<4)&0xf0;
	write4bits((highnib)|mode);
	write4bits((lownib)|mode); 
}




// ********** high level commands, for the user! 

static void begin(u8_t cols, u8_t lines, u8_t dotsize);

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set: 
//	  DL = 1; 8-bit interface data 
//	  N = 0; 1-line display 
//	  F = 0; 5x8 dot character font 
// 3. Display on/off control: 
//	  D = 0; Display off 
//	  C = 0; Cursor off 
//	  B = 0; Blinking off 
// 4. Entry mode set: 
//	  I/D = 1; Increment by 1
//	  S = 0; No shift 

void init(wifiIO_gpio_t sda,wifiIO_gpio_t scl, u8_t lcd_Addr,u8_t lcd_cols,u8_t lcd_rows)
{
	i2c_sim.init(&lcd_i2c_dev.i2c_sim_dev, sda, scl, 10);

	lcd_i2c_dev._Addr = lcd_Addr;
	lcd_i2c_dev._cols = lcd_cols;
	lcd_i2c_dev._rows = lcd_rows;
	lcd_i2c_dev._backlightval = BIT_NOBKLT;

	lcd_i2c_dev._displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	begin(lcd_i2c_dev._cols, lcd_i2c_dev._rows, 0);

}




void print(char* str)
{
	while(*str)
		iic_write(*str++);
}


void clear(void)
{
	iic_cmd(LCD_CLEARDISPLAY);// clear display, set cursor position to zero
	api_tim.tim6_poll(2000);	// this cmd takes a long time!
}

void home(void)
{
	iic_cmd(LCD_RETURNHOME);	// set cursor position to zero
	api_tim.tim6_poll(2000);	// this cmd takes a long time!
}

// Turn the display on/off (quickly)
void noDisplay(void)
{
	lcd_i2c_dev._displaycontrol &= ~LCD_DISPLAYON;
	iic_cmd(LCD_DISPLAYCONTROL | lcd_i2c_dev._displaycontrol);
}
void display(void)
{
	lcd_i2c_dev._displaycontrol |= LCD_DISPLAYON;
	iic_cmd(LCD_DISPLAYCONTROL | lcd_i2c_dev._displaycontrol);
}

// Turn on and off the blinking cursor
void noBlink(void)
{
	lcd_i2c_dev._displaycontrol &= ~LCD_BLINKON;
	iic_cmd(LCD_DISPLAYCONTROL | lcd_i2c_dev._displaycontrol);
}
void blink(void){
	lcd_i2c_dev._displaycontrol |= LCD_BLINKON;
	iic_cmd(LCD_DISPLAYCONTROL | lcd_i2c_dev._displaycontrol);
}


// Turns the underline cursor on/off
void noCursor(void)
{
	lcd_i2c_dev._displaycontrol &= ~LCD_CURSORON;
	iic_cmd(LCD_DISPLAYCONTROL | lcd_i2c_dev._displaycontrol);
}
void cursor(void)
{
	lcd_i2c_dev._displaycontrol |= LCD_CURSORON;
	iic_cmd(LCD_DISPLAYCONTROL | lcd_i2c_dev._displaycontrol);
}


void setCursor(u8_t col, u8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if ( row > lcd_i2c_dev._numlines ) {
		row = lcd_i2c_dev._numlines-1;		// we count rows starting w/0
	}
	iic_cmd(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// These commands scroll the display without changing the RAM
void scrollDisplayLeft(void)
{
	iic_cmd(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void scrollDisplayRight(void)
{
	iic_cmd(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}


// This is for text that flows Left to Right
void leftToRight(void)
{
	lcd_i2c_dev._displaymode |= LCD_ENTRYLEFT;
	iic_cmd(LCD_ENTRYMODESET | lcd_i2c_dev._displaymode);
}

// This is for text that flows Right to Left
void rightToLeft(void)
{
	lcd_i2c_dev._displaymode &= ~LCD_ENTRYLEFT;
	iic_cmd(LCD_ENTRYMODESET | lcd_i2c_dev._displaymode);
}

// Turn the (optional) backlight off/on
void noBacklight(void) {
	lcd_i2c_dev._backlightval=BIT_NOBKLT;
	expanderWrite(0);
}

void backlight(void) {
	lcd_i2c_dev._backlightval=BIT_BKLT;
	expanderWrite(0);
}



// This will 'right justify' text from the cursor
void autoscroll(void)
{
	lcd_i2c_dev._displaymode |= LCD_ENTRYSHIFTINCREMENT;
	iic_cmd(LCD_ENTRYMODESET | lcd_i2c_dev._displaymode);
}

// This will 'left justify' text from the cursor
void noAutoscroll(void)
{
	lcd_i2c_dev._displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
	iic_cmd(LCD_ENTRYMODESET | lcd_i2c_dev._displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void createChar(u8_t location, u8_t charmap[])
{
	int i;
	location &= 0x7; // we only have 8 locations 0-7
	iic_cmd(LCD_SETCGRAMADDR | (location << 3));
	for (i =0; i<8; i++) {
		iic_write(charmap[i]);
	}
}








static void begin(u8_t cols, u8_t lines, u8_t dotsize) {
	if (lines > 1) {
		lcd_i2c_dev._displayfunction |= LCD_2LINE;
	}
	lcd_i2c_dev._numlines = lines;

	// for some 1 line displays you can select a 10 pixel high font
	if ((dotsize != 0) && (lines == 1)) {
		lcd_i2c_dev._displayfunction |= LCD_5x10DOTS;
	}

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
	api_os.tick_sleep(50); 

	// Now we pull both BIT_RS and R/W low to begin commands
	expanderWrite(lcd_i2c_dev._backlightval);	// reset expanderand turn backlight off (Bit 8 =1)
	api_os.tick_sleep(1000);


	//put the LCD into 4 bit mode
	// this is according to the hitachi HD44780 datasheet
	// figure 24, pg 46

	// we start in 8bit mode, try to set 4 bit mode
	 write4bits(0x03 << 4);
	 api_tim.tim6_poll(4500); // wait min 4.1ms


	 // second try
	 write4bits(0x03 << 4);
	 api_tim.tim6_poll(4500); // wait min 4.1ms


	 // third go!
	 write4bits(0x03 << 4); 
	 api_tim.tim6_poll(150);


	 // finally, set to 4-bit interface
	 write4bits(0x02 << 4); 


	// set # lines, font size, etc.
	iic_cmd(LCD_FUNCTIONSET | lcd_i2c_dev._displayfunction);	


	// turn the display on with no cursor or blinking default
	lcd_i2c_dev._displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	display();


	// clear it off
	clear();

	// Initialize to default text direction (for roman languages)
	lcd_i2c_dev._displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;

	// set the entry mode
	iic_cmd(LCD_ENTRYMODESET | lcd_i2c_dev._displaymode);


	home();

}



const lcd_i2c_api_t api_lcd_i2c __ADDON_EXPORT__ = {
	.init = init,
	.print = print,
	.clear = clear,
	.home = home,
	.noDisplay = noDisplay,
	.display = display,
	.noBlink = noBlink,
	.blink = blink,
	.noCursor = noCursor,
	.cursor = cursor,
	.setCursor = setCursor,
	.scrollDisplayLeft = scrollDisplayLeft,
	.scrollDisplayRight = scrollDisplayRight,
	.leftToRight = leftToRight,
	.rightToLeft = rightToLeft,
	.noBacklight = noBacklight,
	.backlight = backlight,
	.autoscroll = autoscroll,
	.noAutoscroll = noAutoscroll,
	.createChar = createChar,
};



////////////////////////////////////////
//每一个addon都有main，该函数在加载后被运行
// 这里适合:
// addon自身运行环境检查；
// 初始化相关数据；

//若返回 非 ADDON_LOADER_GRANTED 将导致函数返回后
//被卸载

//该函数的运行上下文 是wifiIO进程
////////////////////////////////////////

int main(int argc, char* argv[])
{
	api_tim.tim6_init(1000000);	//初始化定时器6 1us为单位

	LOG_INFO("main: lcdi2c attached...\r\n");

	api_lcd_i2c.init(WIFIIO_GPIO_01 ,WIFIIO_GPIO_02, 0x4E,16, 2);
//	api_lcd_i2c.backlight();
//	api_lcd_i2c.autoscroll();
	api_lcd_i2c.print("Hello world!");
/*
*/
	

	return ADDON_LOADER_GRANTED;
//err_exit:
//	return ADDON_LOADER_ABORT;
}






/*
i2c_sim_dev_t i2c_dev;
#define PCF8574_Address 0x4E //PCF8574 I2C地址


static void write_cmd(u8_t addr, u8_t cmd)	//向LCD1602写入指令 
{
	int ret = STATE_OK;
	u8_t temp=0;

	api_tim.tim6_poll(20);		//20us
	//DelayUS(17);//此处插入一个等待很重要，小于这个时间会导致连续写时出现错误
	//先写入高4位

	temp = cmd&0xF0;//先处理高4位，EN=0,RW=0,RS=0
	ret = i2c_sim.write(&i2c_dev, addr, &temp, 1);
	if(ret)LOG_WARN("1 %d\r\n", ret);

	BIT_SET(temp, BIT_EN); //拉高EN
	ret = i2c_sim.write(&i2c_dev, addr, &temp, 1);
//	if(ret)LOG_WARN("2 %d\r\n", ret);

	BIT_CLEAR(temp, BIT_EN)//EN置低，下降沿写入液晶
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("3 %d\r\n", ret);

	//接下来写入低4位
	temp = cmd<<4;
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("4 %d\r\n", ret);

	BIT_SET(temp, BIT_EN); //拉高EN
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("5 %d\r\n", ret);

	BIT_CLEAR(temp, BIT_EN);	//EN置低，下降沿写入液晶
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("6 %d\r\n", ret);

}

static void write_dat(u8_t addr, u8_t data)	//向LCD1602写入数据 
{
	int ret = STATE_OK;
	u8_t temp=0;

	//DelayUS(17);//此处插入一个等待很重要，小于这个时间会导致连续写时出现错误
	api_tim.tim6_poll(20);		//20us


	//先处理高4位，EN=0,RW=0,RS=1
	temp = data&0xF0;
	BIT_SET(temp, BIT_RS);
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
	if(ret)LOG_WARN("1 %d\r\n", ret);


	BIT_SET(temp, BIT_EN);	//拉高EN
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("2 %d\r\n", ret);

	BIT_CLEAR(temp, BIT_EN);	//EN置低，下降沿写入液晶
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("3 %d\r\n", ret);

	//接下来写入低4位
	temp = data<<4;
	BIT_SET(temp, BIT_RS);
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("4 %d\r\n", ret);

	BIT_SET(temp, BIT_EN);	//拉高EN
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("5 %d\r\n", ret);

	BIT_CLEAR(temp, BIT_EN);	//EN置低，下降沿写入液晶
	ret = i2c_sim.write(&i2c_dev, addr, &temp,1);
//	if(ret)LOG_WARN("6 %d\r\n", ret);

}

void init(u8_t addr)			 //LCD1602初始化函数
{
	write_cmd(addr, 0x28);//先进行功能设置，四位数据接口，两行显示，5′7点阵字符。
	api_os.tick_sleep(40);	//等待上条指令完成
	write_cmd(addr, 0x0C);//Display On/Off Control，显示开关：D=1为开，光标开关：C=1为开，闪烁开关：B=1为开
	//write_cmd(addr, 0x06);//AC地址递增模式
	//api_os.tick_sleep(2);
	write_cmd(addr, 0x01);//清屏
	api_os.tick_sleep(2);
}

void  set_pos(u8_t addr, u8_t x, u8_t y) //向LCD1602设置下一个准备写入的字符位置
{
	u8_t address = (y == 0) ? (0x80 + x) : (0xC0 + x);
	write_cmd(addr, address);
}

void  pos_string(u8_t addr, u8_t X, u8_t Y, char *String) //向指定的位置开始写字符串 
{
	set_pos(addr, X, Y);
	while(*String)
		write_dat(addr, *String++);
}

void  put_string(u8_t addr, char *String) //开始写字符串 
{
	while(*String)
		write_dat(addr, *String++);
}







////////////////////////////////////////
//每一个addon都有main，该函数在加载后被运行
// 这里适合:
// addon自身运行环境检查；
// 初始化相关数据；

//若返回 非 ADDON_LOADER_GRANTED 将导致函数返回后
//被卸载

//该函数的运行上下文 是wifiIO进程
////////////////////////////////////////

int main(int argc, char* argv[])
{
	api_tim.tim6_init(1000000);	//初始化定时器6 1us为单位

	LOG_INFO("main: lcdi2c attached...\r\n");



	i2c_sim.init(&i2c_dev,WIFIIO_GPIO_01 ,WIFIIO_GPIO_02 ,10);
	init(PCF8574_Address);
	put_string(PCF8574_Address, "hello!!!");


//	return ADDON_LOADER_GRANTED;
//err_exit:
	return ADDON_LOADER_ABORT;
}


*/






