
#ifndef __LCDI2C_H__
#define __LCDI2C_H__



typedef struct{
	void (*init)(wifiIO_gpio_t sda,wifiIO_gpio_t scl, u8_t lcd_Addr,u8_t lcd_cols,u8_t lcd_rows);
	void (*print)(char* str);
	void(*clear)(void);
	void(*home)(void);
	void(*noDisplay)(void);
	void(*display)(void);
	void(*noBlink)(void);
	void(*blink)(void);
	void(*noCursor)(void);
	void(*cursor)(void);
	void(*setCursor)(u8_t, u8_t); 
	void(*scrollDisplayLeft)(void);
	void(*scrollDisplayRight)(void);
	void(*leftToRight)(void);
	void(*rightToLeft)(void);
	void(*noBacklight)(void);
	void(*backlight)(void);
	void(*autoscroll)(void);
	void(*noAutoscroll)(void); 
	void(*createChar)(u8_t, u8_t[]);
}lcd_i2c_api_t;

extern const lcd_i2c_api_t api_lcd_i2c;	//别的addon中可以 使用该申明 就可以使用




#endif /* __LCDI2C_H__ */
