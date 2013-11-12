
/**
 * @file			vs1003.c
 * @brief			mp3解码模块驱动代码
 *	
 *	
 * @author			dy@wifi.io
*/




#include "include.h"

#define VS_XDCS_PIN WIFIIO_GPIO_03
#define VS_XCS_PIN WIFIIO_GPIO_04
#define VS_DREQ_PIN WIFIIO_GPIO_02
#define VS_RST_PIN WIFIIO_GPIO_13

#define VS_IS_READY() (api_io.get(VS_DREQ_PIN))

#define VS_XDCS_LOW() api_io.low(VS_XDCS_PIN)
#define VS_XDCS_HIGH() api_io.high(VS_XDCS_PIN)

#define VS_XCS_LOW() api_io.low(VS_XCS_PIN)
#define VS_XCS_HIGH() api_io.high(VS_XCS_PIN)

#define VS_DREQ_LOW() api_io.low(VS_DREQ_PIN)
#define VS_DREQ_HIGH() api_io.high(VS_DREQ_PIN)

#define VS_RST_LOW() api_io.low(VS_RST_PIN)
#define VS_RST_HIGH() api_io.high(VS_RST_PIN)








#define VS_WRITE_COMMAND	0x02
#define VS_READ_COMMAND	0x03
//VS10XX寄存器定义
#define SPI_MODE	0x00   
#define SPI_STATUS	0x01   
#define SPI_BASS	0x02   
#define SPI_CLOCKF	0x03   
#define SPI_DECODE_TIME	0x04   
#define SPI_AUDATA	0x05   
#define SPI_WRAM	0x06   
#define SPI_WRAMADDR	0x07   
#define SPI_HDAT0	0x08   
#define SPI_HDAT1	0x09 
  
#define SPI_AIADDR	0x0a   
#define SPI_VOL	0x0b   
#define SPI_AICTRL0	0x0c   
#define SPI_AICTRL1	0x0d   
#define SPI_AICTRL2	0x0e   
#define SPI_AICTRL3	0x0f   
#define SM_DIFF	0x01   
#define SM_JUMP	0x02   
#define SM_RESET	0x04   
#define SM_OUTOFWAV	0x08   
#define SM_PDOWN	0x10   
#define SM_TESTS	0x20   
#define SM_STREAM	0x40   
#define SM_PLUSV	0x80   
#define SM_DACT	0x100   
#define SM_SDIORD	0x200   
#define SM_SDISHARE	0x400   
#define SM_SDINEW	0x800   
#define SM_ADPCM	0x1000   
#define SM_ADPCM_HP	0x2000		






//////////////////////////////////////////////////////////////

typedef struct 
{							
	u8_t mvol;		//主音量,范围:0~254
	u8_t bflimit;		//低音频率限定,范围:2~15(单位:10Hz)
	u8_t bass;		//低音,范围:0~15.0表示关闭.(单位:1dB)
	u8_t tflimit;		//高音频率限定,范围:1~15(单位:Khz)
	u8_t treble;		//高音,范围:0~15(单位:1.5dB)(原本范围是:-8~7,通过函数修改了);
	u8_t effect;		//空间效果设置.0,关闭;1,最小;2,中等;3,最大.

	u8_t saveflag;	//保存标志,0X0A,保存过了;其他,还从未保存	
}__attribute__ ((packed)) _vs10xx_obj;




//VS10XX默认设置参数
_vs10xx_obj vsset=
{
	220,	//音量:220
	6,		//低音上线 60Hz
	15,		//低音提升 15dB	
	10,		//高音下限 10Khz	
	15,		//高音提升 10.5dB
	0,		//空间效果	
};











//SPIx 读写一个字节
//TxData:要写入的字节
//返回值:读取到的字节

u8_t VS_SPI_ReadWriteByte(u8_t TxData)
{
	while((SPI3->SR & 1<<1)==0);//等待发送区空	
	SPI3->DR=TxData;		//发送一个byte 
	while((SPI3->SR&1<<0)==0);//等待接收完一个byte  
	return SPI3->DR;//返回收到的数据					
}

void VS_SPI_SpeedLow(void){}
void VS_SPI_SpeedHigh(void){}


//向VS10XX写命令
//address:命令地址
//data:命令数据
void VS_WR_Cmd(u8_t address,u16_t data)
{  
	while(!VS_IS_READY());//等待空闲		
	VS_SPI_SpeedLow();//低速 	
	VS_XDCS_HIGH(); 	
	VS_XCS_LOW(); 	
	VS_SPI_ReadWriteByte(VS_WRITE_COMMAND);//发送VS10XX的写命令
	VS_SPI_ReadWriteByte(address); //地址
	VS_SPI_ReadWriteByte(data>>8); //发送高八位
	VS_SPI_ReadWriteByte(data);	//第八位
	VS_XCS_HIGH();		
	VS_SPI_SpeedHigh();//高速	
}


//向VS10XX写数据
//data:要写入的数据
void VS_WR_Data(u8_t data)
{
	VS_SPI_SpeedHigh();//高速,对VS1003B,最大值不能超过36.864/4Mhz，这里设置为9M 
	VS_XDCS_LOW();   
	VS_SPI_ReadWriteByte(data);
	VS_XDCS_HIGH();	
}		
//读VS10XX的寄存器		
//address：寄存器地址
//返回值：读到的值
//注意不要用倍速读取,会出错
u16_t VS_RD_Reg(u8_t address)
{
	u16_t temp=0;		
	while(!VS_IS_READY());//非等待空闲状态 		
	VS_SPI_SpeedLow();//低速 
	VS_XDCS_HIGH();	
	VS_XCS_LOW();		
	VS_SPI_ReadWriteByte(VS_READ_COMMAND);	//发送VS10XX的读命令
	VS_SPI_ReadWriteByte(address);		//地址
	temp=VS_SPI_ReadWriteByte(0xff); 		//读取高字节
	temp=temp<<8;
	temp+=VS_SPI_ReadWriteByte(0xff); 		//读取低字节
	VS_XCS_HIGH();	
	VS_SPI_SpeedHigh();//高速	
   return temp; 
}  

//读取VS10xx的RAM
//addr：RAM地址
//返回值：读到的值
u16_t VS_WRAM_Read(u16_t addr) 
{ 
	u16_t res;				
	VS_WR_Cmd(SPI_WRAMADDR, addr); 
	res=VS_RD_Reg(SPI_WRAM);  
	return res;
} 




//硬复位MP3
//返回1:复位失败;0:复位成功	
u8_t VS_HD_Reset(void)
{
	u8_t retry=0;
	VS_RST_LOW();
	_tick_sleep(20);
	VS_XDCS_HIGH();//取消数据传输
	VS_XCS_HIGH();//取消数据传输
	VS_RST_HIGH();	
	while(!VS_IS_READY()&&retry<200)//等待DREQ为高
	{
		retry++;
		_tick_sleep(1); //delay_us(50);
	};
	_tick_sleep(20);	
	if(retry>=200)return 1;
	else return 0;			
}


//软复位VS10XX
void VS_Soft_Reset(void)
{	
	u8_t retry=0;				
	while(!VS_IS_READY()); //等待软件复位结束	
	VS_SPI_ReadWriteByte(0Xff);//启动传输
	retry=0;
	while(VS_RD_Reg(SPI_MODE)!=0x0800)// 软件复位,新模式  
	{
		VS_WR_Cmd(SPI_MODE,0x0804);// 软件复位,新模式		
		_tick_sleep(2);//等待至少1.35ms 
		if(retry++>100)break;		
	}			
	while(!VS_IS_READY());//等待软件复位结束	
	retry=0;
	while(VS_RD_Reg(SPI_CLOCKF)!=0X9800)//设置VS10XX的时钟,3倍频 ,1.5xADD 
	{
		VS_WR_Cmd(SPI_CLOCKF,0X9800);//设置VS10XX的时钟,3倍频 ,1.5xADD
		if(retry++>100)break;		
	}													
	_tick_sleep(20);
} 




//得到需要填充的数字
//返回值:需要填充的数字
u16_t VS_Get_EndFillByte(void)
{
	return VS_WRAM_Read(0X1E06);//填充字节
}  


//发送一次音频数据
//固定为32字节
//返回值:0,发送成功
//		1,VS10xx不缺数据,本次数据未成功发送	
u8_t VS_Send_MusicData(u8_t* buf, size_t n)
{
	u8_t i;
	if(VS_IS_READY())  //送数据给VS10XX
	{				
		VS_XDCS_LOW();  
		for(i=0;i<n;i++)
		{
			VS_SPI_ReadWriteByte(buf[i]);				
		}
		VS_XDCS_HIGH();				
	}
	else 
		return 1;

	return 0;//成功发送了
}


//切歌
//通过此函数切歌，不会出现切换“噪声”				
void VS_Restart_Play(void)
{
	u16_t temp;
	u16_t i;
	u8_t n;	
	u8_t vsbuf[32];
	for(n=0;n<32;n++)
		vsbuf[n]=0;//清零

	temp=VS_RD_Reg(SPI_MODE);	//读取SPI_MODE的内容
	temp|=1<<3;					//设置SM_CANCEL位
	temp|=1<<2;					//设置SM_LAYER12位,允许播放MP1,MP2
	VS_WR_Cmd(SPI_MODE,temp);	//设置取消当前解码指令

	for(i=0;i<2048;)			//发送2048个0,期间读取SM_CANCEL位.如果为0,则表示已经取消了当前解码
	{
		if(VS_Send_MusicData(vsbuf, 32)==0)//每发送32个字节后检测一次
		{
			i+=32;						//发送了32个字节
			temp=VS_RD_Reg(SPI_MODE);	//读取SPI_MODE的内容
			if((temp&(1<<3))==0)break;	//成功取消了
		}	
	}

	if(i<2048)//SM_CANCEL正常
	{
		temp=VS_Get_EndFillByte()&0xff;//读取填充字节
		for(n=0;n<32;n++)
			vsbuf[n]=temp;//填充字节放入数组

		for(i=0;i<2052;)
		{
			if(VS_Send_MusicData(vsbuf, 32)==0)i+=32;//填充	
		}	
	}
	else 
		VS_Soft_Reset();	//SM_CANCEL不成功,坏情况,需要软复位	

	temp=VS_RD_Reg(SPI_HDAT0); 
	temp+=VS_RD_Reg(SPI_HDAT1);

	if(temp)					//软复位,还是没有成功取消,放杀手锏,硬复位
	{
		VS_HD_Reset();			//硬复位
		VS_Soft_Reset();		//软复位 
	} 
}

//设定VS10XX播放的音量和高低音
//volx:音量大小(0~254)
void VS_Set_Vol(u8_t volx)
{
	u16_t volt=0;			//暂存音量值
	volt=254-volx;			//取反一下,得到最大值,表示最大的表示 
	volt<<=8;
	volt+=254-volx;			//得到音量设置后大小
	VS_WR_Cmd(SPI_VOL,volt);//设音量 
}


//设定高低音控制
//bfreq:低频上限频率	2~15(单位:10Hz)
//bass:低频增益			0~15(单位:1dB)
//tfreq:高频下限频率	1~15(单位:Khz)
//treble:高频增益		0~15(单位:1.5dB,小于9的时候为负数)
void VS_Set_Bass(u8_t bfreq,u8_t bass,u8_t tfreq,u8_t treble)
{
	u16_t bass_set=0; //暂存音调寄存器值
	signed char temp=0;	
	if(treble==0)temp=0;			//变换
	else if(treble>8)temp=treble-8;
	else temp=treble-9;  
	bass_set=temp&0X0F;				//高音设定
	bass_set<<=4;
	bass_set+=tfreq&0xf;			//高音下限频率
	bass_set<<=4;
	bass_set+=bass&0xf;				//低音设定
	bass_set<<=4;
	bass_set+=bfreq&0xf;			//低音上限	
	VS_WR_Cmd(SPI_BASS,bass_set);	//BASS 
}


//设定音效
//eft:0,关闭;1,最小;2,中等;3,最大.
void VS_Set_Effect(u8_t eft)
{
	u16_t temp;	
	temp=VS_RD_Reg(SPI_MODE);	//读取SPI_MODE的内容
	if(eft&0X01)temp|=1<<4;		//设定LO
	else temp&=~(1<<5);			//取消LO
	if(eft&0X02)temp|=1<<7;		//设定HO
	else temp&=~(1<<7);			//取消HO						
	VS_WR_Cmd(SPI_MODE,temp);	//设定模式	
}	



///////////////////////////////////////////////////////////////////////////////
//设置音量,音效等.
void VS_Set_All(void)				
{			
	VS_Set_Vol(vsset.mvol);
	VS_Set_Bass(vsset.bflimit,vsset.bass,vsset.tflimit,vsset.treble);  
	VS_Set_Effect(vsset.effect);
}



//重设解码时间						
void VS_Reset_DecodeTime(void)
{
	VS_WR_Cmd(SPI_DECODE_TIME,0x0000);
	VS_WR_Cmd(SPI_DECODE_TIME,0x0000);//操作两次
}


//FOR WAV HEAD0 :0X7761 HEAD1:0X7665    
//FOR MIDI HEAD0 :other info HEAD1:0X4D54
//FOR WMA HEAD0 :data speed HEAD1:0X574D
//FOR MP3 HEAD0 :data speed HEAD1:ID
//比特率预定值,阶层III
const u16_t bitrate[2][16]=
{ 
{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0}, 
{0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}
};
//返回Kbps的大小
//返回值：得到的码率
u16_t VS_Get_HeadInfo(void)
{
	unsigned int HEAD0;
	unsigned int HEAD1;  
 	HEAD0=VS_RD_Reg(SPI_HDAT0); 
    HEAD1=VS_RD_Reg(SPI_HDAT1);
  	//printf("(H0,H1):%x,%x\n",HEAD0,HEAD1);
    switch(HEAD1)
    {        
        case 0x7665://WAV格式
        case 0X4D54://MIDI格式 
		case 0X4154://AAC_ADTS
		case 0X4144://AAC_ADIF
		case 0X4D34://AAC_MP4/M4A
		case 0X4F67://OGG
        case 0X574D://WMA格式
		case 0X664C://FLAC格式
        {
			////printf("HEAD0:%d\n",HEAD0);
            HEAD1=HEAD0*2/25;//相当于*8/100
            if((HEAD1%10)>5)return HEAD1/10+1;//对小数点第一位四舍五入
            else return HEAD1/10;
        }
        default://MP3格式,仅做了阶层III的表
        {
            HEAD1>>=3;
            HEAD1=HEAD1&0x03; 
            if(HEAD1==3)
				HEAD1=1;
            else 
				HEAD1=0;
            return bitrate[HEAD1][HEAD0>>12];
        }
    }  
}

//得到mp3的播放时间n sec
//返回值：解码时长
u16_t VS_Get_DecodeTime(void)
{ 		
	u16_t dt=0;	 
	dt=VS_RD_Reg(SPI_DECODE_TIME);      
 	return dt;
} 	    					  



//显示播放时间,比特率 信息 
//lenth:歌曲总长度
void mp3_msg_show()
{	
	LOG_INFO("Bitrate:%d.\r\n",VS_Get_HeadInfo());	   //获得比特率
	LOG_INFO("DecodeTime:%d.\r\n",VS_Get_DecodeTime()); //得到解码时间
}			  		 





//#include "test_mp3.h"

void Play_Music(void)
{

	int ret = STATE_OK;
	u8_t temp[512];
	FIL fobj;
	UINT n;


	VS_HD_Reset();
	VS_Soft_Reset();


	VS_Restart_Play();				//重启播放 
	VS_Set_All();					//设置音量等信息			
	VS_Reset_DecodeTime();			//复位解码时间	




	if(FR_OK != (ret = api_fs.f_open(&fobj, "/app/vs1003/demo.mp3", FA_OPEN_EXISTING | FA_READ))){
		LOG_INFO("mp3 file open err:%d.\r\n", ret);
		return;
	}


	//mp3_msg_show();
	while(FR_OK == api_fs.f_read(&fobj, temp, sizeof(temp), &n) && n > 0)   //播放音乐的主循环
	{
		int i = 0;
		while(i < n){
			int j = MIN(n-i, 32);
			while(0 != VS_Send_MusicData(temp + i, j)){};	//给VS10XX发送音频数据
			i += j;
		};

		//api_console.printf("~");
	}
	//mp3_msg_show();

	api_fs.f_close(&fobj);



/*
	n = sizeof(music);
	{
		int i = 0;
		while(i < n){
			int j = MIN(n-i, 32);
			while(0 != VS_Send_MusicData(music + i, j)){};	//给VS10XX发送音频数据
			i += j;
			_tick_sleep(1);
		};

		//api_console.printf("~");
	}


*/


	api_console.printf("\r\n");

	VS_HD_Reset();  //硬复位
	VS_Soft_Reset();//软复位

}


//初始化VS10XX的IO口	

//pin_mosi PB5
//pin_miso PB4
//pin_clock PB3


void VS_Init(void)
{
	//spi3 clock
	stm_rcc.RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);
	stm_rcc.RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	stm_gpio.GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_SPI3);
	stm_gpio.GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_SPI3);
	stm_gpio.GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_SPI3);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd	= GPIO_PuPd_UP;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	stm_gpio.GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	stm_gpio.GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	stm_gpio.GPIO_Init(GPIOB, &GPIO_InitStructure);

	SPI_InitTypeDef	SPI_InitStructure;
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	stm_spi.SPI_Init(SPI3, &SPI_InitStructure);

	stm_spi.SPI_Cmd(SPI3, ENABLE);


}	




//正弦测试 
void VS_Sine_Test(void)
{												
	VS_HD_Reset();	
	VS_WR_Cmd(0x0b,0X2020);	//设置音量	
	VS_WR_Cmd(SPI_MODE,0x0820);//进入VS10XX的测试模式	
	while(!VS_IS_READY());	//等待DREQ为高
	//printf("mode sin:%x\n",VS_RD_Reg(SPI_MODE));
	//向VS10XX发送正弦测试命令：0x53 0xef 0x6e n 0x00 0x00 0x00 0x00
	//其中n = 0x24, 设定VS10XX所产生的正弦波的频率值，具体计算方法见VS10XX的datasheet
	VS_SPI_SpeedLow();//低速 
	VS_XDCS_LOW();//选中数据传输
	VS_SPI_ReadWriteByte(0x53);
	VS_SPI_ReadWriteByte(0xef);
	VS_SPI_ReadWriteByte(0x6e);
	VS_SPI_ReadWriteByte(0x24);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	_tick_sleep(100);
	VS_XDCS_HIGH(); 
	//退出正弦测试
	VS_XDCS_LOW();//选中数据传输
	VS_SPI_ReadWriteByte(0x45);
	VS_SPI_ReadWriteByte(0x78);
	VS_SPI_ReadWriteByte(0x69);
	VS_SPI_ReadWriteByte(0x74);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	_tick_sleep(100);
	VS_XDCS_HIGH();		

	//再次进入正弦测试并设置n值为0x44，即将正弦波的频率设置为另外的值
	VS_XDCS_LOW();//选中数据传输	
	VS_SPI_ReadWriteByte(0x53);
	VS_SPI_ReadWriteByte(0xef);
	VS_SPI_ReadWriteByte(0x6e);
	VS_SPI_ReadWriteByte(0x44);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	_tick_sleep(100);
	VS_XDCS_HIGH();
	//退出正弦测试
	VS_XDCS_LOW();//选中数据传输
	VS_SPI_ReadWriteByte(0x45);
	VS_SPI_ReadWriteByte(0x78);
	VS_SPI_ReadWriteByte(0x69);
	VS_SPI_ReadWriteByte(0x74);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	_tick_sleep(100);
	VS_XDCS_HIGH();	
}	



//ram 测试 
//返回值:RAM测试结果
// VS1003如果得到的值为0x807F，则表明完好;VS1053为0X83FF.																				
u16_t VS_Ram_Test(void)
{
	VS_HD_Reset();	
 	VS_WR_Cmd(SPI_MODE,0x0820);// 进入VS10XX的测试模式
	while (!VS_IS_READY()); // 等待DREQ为高			
 	VS_SPI_SpeedLow();//低速 
	VS_XDCS_LOW();					// xDCS = 1，选择VS10XX的数据接口
	VS_SPI_ReadWriteByte(0x4d);
	VS_SPI_ReadWriteByte(0xea);
	VS_SPI_ReadWriteByte(0x6d);
	VS_SPI_ReadWriteByte(0x54);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	VS_SPI_ReadWriteByte(0x00);
	_tick_sleep(150);  
	VS_XDCS_HIGH();
	return VS_RD_Reg(SPI_HDAT0);// VS1003如果得到的值为0x807F，则表明完好;VS1053为0X83FF.;	
}						







////////////////////////////////////////
//每一个addon都有main，该函数在加载后被运行
//若返回 非 ADDON_LOADER_GRANTED 将导致main函数返回后addon被卸载
////////////////////////////////////////

int main(int argc, char* argv[])
{

	//IO init 
	api_io.init(VS_DREQ_PIN, IN_PULL_UP);
	api_io.init(VS_XDCS_PIN, OUT_OPEN_DRAIN_PULL_UP);
	api_io.init(VS_XCS_PIN, OUT_OPEN_DRAIN_PULL_UP);
	api_io.init(VS_RST_PIN, OUT_OPEN_DRAIN_PULL_UP);


	//api_tim.tim6_init(100000);	//初始化定时器6 10us为单位


	VS_Init();

//	while(1)  
	{				
		LOG_INFO("Ram test.\r\n");
		VS_Ram_Test();

		LOG_INFO("Sin wave test.\r\n");
		VS_Sine_Test();

		LOG_INFO("Mp3 play...\r\n");		

		Play_Music();

	}

//	return ADDON_LOADER_GRANTED;


	return ADDON_LOADER_ABORT;


}

