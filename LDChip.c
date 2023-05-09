/*
	CPU: STC11L08XE
	晶振：22.1184MHZ
	波特率：9600 bit/S*/
	
#include "config.h"

extern void  delay(unsigned long uldata);

uint8 idata ucRegVal;
extern uint8 idata nAsrStatus;

void ProcessInt0(void);

/*复位LD模块*/
void LD_Reset()
{
  RSTB=1;
  delay(5);
  RSTB=0;
  delay(5);
  RSTB=1;

  delay(5);
  CSB=0;
  delay(5);
  CSB=1;
  delay(5);
}
/*LD模块命令初始化*/
void LD_Init_Common()
{
  LD_ReadReg(0x06);
  LD_WriteReg(0x17, 0x35);
  delay(10);
  LD_ReadReg(0x06);

  LD_WriteReg(0x89, 0x03);
  delay(5);
  LD_WriteReg(0xCF, 0x43);
  delay(5);
  LD_WriteReg(0xCB, 0x02);

  /*PLL setting*/
  LD_WriteReg(0x11, LD_PLL_11);

  LD_WriteReg(0x1E,0x00);
  LD_WriteReg(0x19, LD_PLL_ASR_19);
  LD_WriteReg(0x1B, LD_PLL_ASR_1B);
  LD_WriteReg(0x1D, LD_PLL_ASR_1D);
  delay(10);

  LD_WriteReg(0xCD, 0x04);
//	LD_WriteReg(0x17, 0x4c);
  delay(5);
  LD_WriteReg(0xB9, 0x00);
  LD_WriteReg(0xCF, 0x4F);
  LD_WriteReg(0x6F, 0xFF);
}

/*LD模块 ASR功能初始化*/
void LD_Init_ASR()
{
  LD_Init_Common();
  LD_WriteReg(0xBD, 0x00);
  LD_WriteReg(0x17, 0x48);
  delay( 10 );
  LD_WriteReg(0x3C, 0x80);
  LD_WriteReg(0x3E, 0x07);
  LD_WriteReg(0x38, 0xff);
  LD_WriteReg(0x3A, 0x07);
  LD_WriteReg(0x40, 0);
  LD_WriteReg(0x42, 8);
  LD_WriteReg(0x44, 0);
  LD_WriteReg(0x46, 8);
  delay( 1 );
}

/*中断处理函数
	当LD模块接收到音频信号时，将进入该函数，
	判断识别是否有结果，如果没有从新配置寄
  存器准备下一次的识别。*/
void ProcessInt0(void)
{
  uint8 nAsrResCount=0;

  EX0=0;
  ucRegVal = LD_ReadReg(0x2B);
  LD_WriteReg(0x29,0) ;
  LD_WriteReg(0x02,0) ;
  if((ucRegVal & 0x10) &&
      LD_ReadReg(0xb2)==0x21 &&
      LD_ReadReg(0xbf)==0x35)			/*识别成功*/
  {
    nAsrResCount = LD_ReadReg(0xba);
    if(nAsrResCount>0 && nAsrResCount<=4)
    {
      nAsrStatus=LD_ASR_FOUNDOK;
    }
    else
    {
      nAsrStatus=LD_ASR_FOUNDZERO;
    }
  }															 /*没有识别结果*/
  else
  {
    nAsrStatus=LD_ASR_FOUNDZERO;
  }

  LD_WriteReg(0x2b, 0);
  LD_WriteReg(0x1C,0);/*写0:ADC不可用*/

  LD_WriteReg(0x29,0) ;
  LD_WriteReg(0x02,0) ;
  LD_WriteReg(0x2B,  0);
  LD_WriteReg(0xBA, 0);
  LD_WriteReg(0xBC,0);
  LD_WriteReg(0x08,1);	 /*清除FIFO_DATA*/
  LD_WriteReg(0x08,0);	/*清除FIFO_DATA后 再次写0*/


  EX0=1;
}

/*运行ASR识别流程 返回1启动成功 返回0启动失败
	识别顺序:
	1、RunASR() 完整的ASR语音识别流程
	2、LD_AsrStart() ASR初始化
	3、LD_AsrAddFixed() 添加关键词语到LD3320芯片中
	4、LD_AsrRun()	启动一次ASR语音识别流程*/
uint8 RunASR(void)
{
  uint8 i=0;
  uint8 asrflag=0;
  for (i=0; i<5; i++)	//	防止由于硬件原因导致LD3320芯片工作不正常
  {
    LD_AsrStart();
    delay(50);
    if (LD_AsrAddFixed()==0)
    {
      LD_Reset();			//	LD3320芯片内部出现不正常，立即重启LD3320芯片
      delay(50);			//	从初始化开始重新ASR识别流程
      continue;
    }
    delay(10);
    if (LD_AsrRun() == 0)
    {
      LD_Reset();			//	LD3320芯片内部出现不正常，立即重启LD3320芯片
      delay(50);			//	从初始化开始重新ASR识别流程
      continue;
    }
    asrflag=1;
    break;					//	ASR流程启动成功，退出当前for循环。开始等待LD3320送出的中断信号
  }

  return asrflag;
}
/*检测LD模块是否空闲 返回1空闲*/
uint8 LD_Check_ASRBusyFlag_b2()
{
  uint8 j;
  uint8 flag = 0;
  for (j=0; j<10; j++)
  {
    if (LD_ReadReg(0xb2) == 0x21)
    {
      flag = 1;
      break;
    }
    delay(10);
  }
  return flag;
}
/*启动ASR*/
void LD_AsrStart()
{
  LD_Init_ASR();
}
/*运行ASR 返回1启动成功*/
uint8 LD_AsrRun()
{
  EX0=0;
  LD_WriteReg(0x35, MIC_VOL);
  LD_WriteReg(0x1C, 0x09);
  LD_WriteReg(0xBD, 0x20);
  LD_WriteReg(0x08, 0x01);
  delay( 1 );
  LD_WriteReg(0x08, 0x00);
  delay( 1 );

  if(LD_Check_ASRBusyFlag_b2() == 0)
  {
    return 0;
  }
//	LD_WriteReg(0xB6, 0xa); //识别时间	 1S
//	LD_WriteReg(0xB5, 0x1E); //背景音段时间 300ms
//	LD_WriteReg(0xB8, 10); //结束时间

//	LD_WriteReg(0x1C, 0x07); //配置双通道音频信号做为输入信号
  LD_WriteReg(0x1C, 0x0b);   //配置麦克风做为输入信号


  LD_WriteReg(0xB2, 0xff);
  delay( 1);
  LD_WriteReg(0x37, 0x06);
  delay( 1 );
  LD_WriteReg(0x37, 0x06);
  delay( 5 );
  LD_WriteReg(0x29, 0x10);

  LD_WriteReg(0xBD, 0x00);
  EX0=1;
  return 1;
}
/* 向LD模块添加关键词 返回1启动成功
					 用户修改.
					 根据格式添关键词，修改sRecog 和pCode 数组的长度
					 和对应变了k的循环置。拼音串和识别码一一对应。
					 每次识别时，必须先发一级口令*/
uint8 LD_AsrAddFixed()
{
  uint8 k, flag;
  uint8 nAsrAddLength;
#define DATE_A 50   /*数组二维数值*/
#define DATE_B 20		/*数组一维数值*/
  uint8 code sRecog[DATE_A][DATE_B] =
  {
    "lai fu",\
    "kai deng",\
    "guan deng",\
    "da kai dian shi",\
    "guan bi dian shi",\
    "da kai bing xiang",\
    "guan bi bing xiang",\
    "da kai kong tiao",\
    "guan bi kong tiao",\
    "quan bu da kai",\
    "quan bu guan bi",\
  };	/*添加关键词，用户修改*/
  uint8 code pCode[DATE_A] =
  {
    CODE_CMD, \
		CODE_KD, \
		CODE_GD, \
		CODE_KDS, \
		CODE_GDS, \
		CODE_KBX, \
		CODE_GBX, \
		CODE_KKT, \
		CODE_GKT, \
		CODE_QDK, \
		CODE_QGB, \
		CODE_11, \
		CODE_12, \
		CODE_13, \
		CODE_14, \
		CODE_15, \
		CODE_16, \
		CODE_17, \
		CODE_18, \
		CODE_19, \
		CODE_20, \
		CODE_21, \
		CODE_22, \
		CODE_23, \
		CODE_24, \
		CODE_25, \
		CODE_26, \
		CODE_27, \
		CODE_28, \
		CODE_29, \
		CODE_30, \
		CODE_31, \
		CODE_32, \
		CODE_33, \
		CODE_34, \
		CODE_35, \
		CODE_36, \
		CODE_37, \
		CODE_38, \
		CODE_39, \
		CODE_40, \
		CODE_41, \
		CODE_42, \
		CODE_43, \
		CODE_44, \
		CODE_45, \
		CODE_46, \
		CODE_47, \
		CODE_48, \
		CODE_49, \
  };	/*添加识别码，用户修改*/
  flag = 1;
  for (k=0; k<DATE_A; k++)
  {

    if(LD_Check_ASRBusyFlag_b2() == 0)
    {
      flag = 0;
      break;
    }

    LD_WriteReg(0xc1, pCode[k] );
    LD_WriteReg(0xc3, 0 );
    LD_WriteReg(0x08, 0x04);
    delay(1);
    LD_WriteReg(0x08, 0x00);
    delay(1);

    for (nAsrAddLength=0; nAsrAddLength<DATE_B; nAsrAddLength++)
    {
      if (sRecog[k][nAsrAddLength] == 0)
        break;
      LD_WriteReg(0x5, sRecog[k][nAsrAddLength]);
    }
    LD_WriteReg(0xb9, nAsrAddLength);
    LD_WriteReg(0xb2, 0xff);
    LD_WriteReg(0x37, 0x04);
  }
  return flag;
}
/*获取识别结果 返回	LD_ReadReg(0xc5 )； 读取内部寄存器返回识别码。*/
uint8 LD_GetResult()
{
  return LD_ReadReg(0xc5 );
}



