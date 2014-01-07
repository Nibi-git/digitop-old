//программа для устройства защиты бытовой техники от превышения напряжения в сети
//на основе Tiny26

#include <ioavr.h>
#include <intrinsics.h>
#include "hardware.h"
#include "segments.h"

__eeprom unsigned int UpTresholdEEPROM, DownTresholdEEPROM, K1EEPROM;
__eeprom unsigned int ProtectTimerOnMaxValueEEPROM;
__flash signed int segmentsDec[3]={100,10,1};
volatile unsigned char ResultSummReady;
unsigned int DisplayValue;//volatile 
unsigned char String[3];
unsigned char cycle_count;//, //Brightness = 2;
volatile unsigned char VideoBuffer[3];
unsigned char keypress[2], KeyTimer;
volatile unsigned int ProtectTimerOnValue;
unsigned int ProtectTimerOnMaxValue;

unsigned char ProtectTimerOffValue;
#define ProtectTimerOffMaxValue 100 // 120 // 1.2 ~= 10 ms

unsigned int Voltage;
volatile unsigned int UpTreshold, DownTreshold;
unsigned int DisplayedVoltage;
volatile unsigned char DisplayMode = RealtimeVoltage, EnableDisplay = 128; // небольшая экономия кода

volatile unsigned int PeakResult;
volatile unsigned int Peak; // внутреннее значение

volatile unsigned char PlusSamplesCounter, ZeroSamplesCounter;

//volatile unsigned char PSResult ;
unsigned int K1; //500
#define K2 1000
#define FastProtectVoltageTreshold 120
#define DACNoiseTreshold 5

inline void __watchdog_init (void)
{
//запускаю сторожевой таймер на 2 секунды
__watchdog_reset ();
WDTCR |= ((1<<WDCE)|(1<<WDE));
WDTCR = (1<<WDE)|(7<<WDP0);
__watchdog_reset ();
}

void CharToStringDec(signed int inp) // обрезанная до сотен
{
unsigned char i;
String[0]=String[1]=String[2]=0;
// перевод
for(i=0;i<3;)
  {
  if((inp-segmentsDec[i])>=0)
    {
    inp-=segmentsDec[i];
    String[i]++;
    }
  else i++;
  }
}

inline void InitPorts (void)
{
PortButton |= (1<<Button_DN)|(1<<Button_UP); // подтягивающие резисторы

PortAnodeDir |= ((1<<SEG_A)|(1<<SEG_B)|(1<<SEG_C)|(1<<SEG_D)|(1<<SEG_E)|(1<<SEG_F)|(1<<SEG_G)|(1<<SEG_DP));
PortKathodeAndRelayDir |= ((1<<Kathode_1) | (1<<Kathode_2) | (1<<Kathode_3)|(1<<Relay));
}

inline void InitADC (void)
{
ADMUX = ((2<<REFS0)|(InADC << MUX0));
ADCSR = ((1 << ADEN)|(5 << ADPS0)|(0<<ADFR)|(0<<ADSC)|(0<<ADIE));

//5 << ADPS0 Prescaler = 32  f = 8000000/(32*13)=19230 Гц
//6 << ADPS0 Prescaler = 64  f = 8000000/(64*13)=9615 Гц
//7 << ADPS0 Prescaler = 128
}

inline void InitTimers (void)
{
TCCR0 |= (4<<CS00);//3
TCCR1B |= (4<<CS10);//3
TIMSK |= ((1<<TOIE0)|(1<<TOIE1));
}

void SaveSettings (void)
{
UpTresholdEEPROM = UpTreshold;
DownTresholdEEPROM = DownTreshold;
ProtectTimerOnMaxValueEEPROM = ProtectTimerOnMaxValue;
K1EEPROM = K1;
}

void CheckTresholdSettings (void)
{
if (UpTreshold > 290) UpTreshold = 290;
if (UpTreshold < 210) UpTreshold = 210;

if (DownTreshold > 200) DownTreshold = 200;
if (DownTreshold < 140) DownTreshold = 140;
}

void CheckTimerSettings (void)
{
if (ProtectTimerOnMaxValue > 995) ProtectTimerOnMaxValue = 995;
if (ProtectTimerOnMaxValue < 5) ProtectTimerOnMaxValue = 5;
}

void CheckK1Settings (void)
{
if (K1 > 999) K1 = 999;
if (K1 < 1) K1 = 1;
}

void LoadSettings (void)
{
UpTreshold = UpTresholdEEPROM;
DownTreshold = DownTresholdEEPROM;
ProtectTimerOnMaxValue = ProtectTimerOnMaxValueEEPROM;
K1 = K1EEPROM;

CheckTresholdSettings ();
CheckTimerSettings ();
CheckK1Settings ();
}

inline void UpdateLedScreen (void)
{
switch (DisplayMode)
  {
  case RealtimeVoltage:     DisplayValue = DisplayedVoltage;      break;
  case UpTresholdVoltage:   DisplayValue = UpTreshold;            break;
  case DownTresholdVoltage: DisplayValue = DownTreshold;          break;
  case UpTresholdTune:      DisplayValue = UpTreshold;            break;
  case DownTresholdTune:    DisplayValue = DownTreshold;          break;
  case ProtectTimerTune:    DisplayValue = ProtectTimerOnMaxValue;  break;
  case K1Tune:              DisplayValue = K1;                    break;
  case ProtectTimer:        DisplayValue = ProtectTimerOnValue;     break;
  }

CharToStringDec(DisplayValue);

if (EnableDisplay)
  {
  for (unsigned char i=0; i<3; i++) {VideoBuffer[i] = led_digits[String[i]];}
  if (DisplayMode >= UpTresholdTune)    {    VideoBuffer[2] |= led_digits[0x10];    }
  }
  else
    {
    VideoBuffer[0] = 0;
    VideoBuffer[1] = 0;
    VideoBuffer[2] = 0;
    }
}

#pragma vector = TIMER1_OVF1_vect 
__interrupt void ADCSampleReady (void)
{
unsigned int temp;
TCNT1 = T1_RELOAD; // перезагрузить таймер
temp = ADC;

ADCSR |= (1<<ADSC);

if (temp > DACNoiseTreshold)
  {
  if (temp > Peak) Peak = temp;
  
  PlusSamplesCounter++;
  ZeroSamplesCounter = 0;

	if (PlusSamplesCounter > 200)
	  {
	  PeakResult = 0;
	  Peak = 0;
	  
	  PlusSamplesCounter = 0;
	  ResultSummReady = 1; // рапортуем об обрыве или повреждении
	  }
  }

  else 
    { // считаем сколько нулевых семплов попалось
    ZeroSamplesCounter++;
    
    if ((ZeroSamplesCounter > 20)&&(PlusSamplesCounter > 5)) // полуволна закончилась
      {
      PeakResult = Peak;
      Peak = 0;
      PlusSamplesCounter = 0;
      ZeroSamplesCounter = 0;
      ResultSummReady = 1;
      }
  
    if (ZeroSamplesCounter > 200)
      {
      Peak = 0;
      PeakResult = 0;
      ZeroSamplesCounter = 0;
      
      PlusSamplesCounter = 0;
      ResultSummReady = 1; // рапортуем, что напряжение пропало совсем
      }
    }
}

void PressProcessing (unsigned char code_state)
{
switch (code_state)
  {
  case DN_SHORT:
    switch (DisplayMode)
    {
    case RealtimeVoltage:       DisplayMode = DownTresholdVoltage;                break;
    case UpTresholdVoltage:     DisplayMode = DownTresholdVoltage;                break;
    case DownTresholdTune:      DownTreshold --; CheckTresholdSettings ();        break;
    case UpTresholdTune:        UpTreshold --;   CheckTresholdSettings ();        break;
    case ProtectTimerTune:      ProtectTimerOnMaxValue -=5; CheckTimerSettings ();  break;
    case ProtectTimer:          DisplayMode = DownTresholdVoltage;                break;
    case K1Tune:                K1 --;                                            break;
    }
  break;
  
  case UP_SHORT:
    switch (DisplayMode)
    {
    case RealtimeVoltage:       DisplayMode = UpTresholdVoltage;                  break;
    case DownTresholdVoltage:   DisplayMode = UpTresholdVoltage;                  break;
    case DownTresholdTune:      DownTreshold ++; CheckTresholdSettings ();        break;
    case UpTresholdTune:        UpTreshold ++;   CheckTresholdSettings ();        break;
    case ProtectTimerTune:      ProtectTimerOnMaxValue +=5; CheckTimerSettings ();  break;
    case ProtectTimer:          DisplayMode = UpTresholdVoltage;                  break;  
    case K1Tune:                K1 ++;                                            break;  
    }
  break;
  
  case DN_MID:
  if (DisplayMode == DownTresholdVoltage) DisplayMode = DownTresholdTune;
  break;
  
  case UP_MID:
  if (DisplayMode == UpTresholdVoltage) DisplayMode = UpTresholdTune;
  break;
  
  case DN_UP_MID:
    switch (DisplayMode)
    {
    case RealtimeVoltage:      DisplayMode = ProtectTimerTune;                    break;
    case ProtectTimerTune:     DisplayMode = K1Tune;                              break;
    }
  break;
  }
}

#pragma vector = TIMER0_OVF0_vect 
__interrupt void DynInd (void)
{
TCNT0 = T0_RELOAD; // перезагрузить таймер

if (cycle_count < NUMBER_DIGITS)
  {
  PortKathode |= ((1<<Kathode_1) | (1<<Kathode_2) | (1<<Kathode_3)); //потушили все разряды
  PortAnode &= ~((1<<SEG_A)|(1<<SEG_B)|(1<<SEG_C)|(1<<SEG_D)|(1<<SEG_E)|(1<<SEG_F)|(1<<SEG_G)|(1<<SEG_DP));

  PortAnode = VideoBuffer[cycle_count]; // выдать новые значения для анодов
  PortKathode &= ~ArrayKathodes[cycle_count];// включить катод
  }

if (++cycle_count >= NUMBER_DIGITS) cycle_count=0;
}

void KeyboardThread (void)
{
if (cycle_count == 2) // проверяем левую кнопку
  {
  if (!(PinButton & (1<<Button_DN))) {keypress[0]++; KeyTimer = KeyTimerMax;}
  else 
    {
    if ((keypress[0] <  midpress)&&(keypress[0] >  shortpress)) PressProcessing(DN_SHORT);
    keypress[0]=0;
    }
  }

if (cycle_count == 0) // проверяем правую кнопку
  {
  if (!(PinButton & (1<<Button_UP))) {keypress[1]++; KeyTimer = KeyTimerMax;}
  else 
    {
    if ((keypress[1] <  midpress)&&(keypress[1] >  shortpress)) PressProcessing(UP_SHORT);
    keypress[1]=0;
    }
  }

if (keypress[1] == 0) // была нажата кнопка ВНИЗ
{
  if (keypress[0] == midpress) {  keypress[0] = midpress+5;  PressProcessing(DN_MID);  }
  if (keypress[0] > longpress) {  keypress[0] = longpress+5; } 
  if ((keypress[0] == 0)&&(KeyTimer)) KeyTimer--; // или не нажаты обе кнопки
}

if (keypress[0] == 0) // была нажата кнопка ВВЕРХ
{
  if (keypress[1] == midpress) {  keypress[1] = midpress+5;  PressProcessing(UP_MID);  }  
  if (keypress[1] > longpress) {  keypress[1] = longpress+5; }
}
if ((keypress[0] == midpress)&&(keypress[1] > shortpress))  {  keypress[0] = midpress+5; keypress[1] = midpress+5; PressProcessing(DN_UP_MID);  }
if ((keypress[0] > longpress)&&(keypress[1] > longpress))  {  keypress[0] = longpress+5; keypress[1] = longpress+5; }

if (KeyTimer == 1)   {  DisplayMode = RealtimeVoltage;  SaveSettings ();  }
}



unsigned char QuarterSecPrescaler, SecPrescaler;

int main( void )
{
unsigned long temp;
InitPorts ();
InitTimers ();
InitADC ();
LoadSettings ();
ProtectTimerOnValue = ProtectTimerOnMaxValue;	//таймер включения после провалов взведен
//ProtectTimerOffValue = ProtectTimerOffMaxValue; // таймер выключения при неглубоких провалах взведен
__watchdog_init ();
__enable_interrupt();

  while (1)
  {
  __delay_cycles((CtrlClockRate/1000)*10);

  if (ResultSummReady)
    {
    __watchdog_reset ();
    ResultSummReady = 0;
    temp = (((unsigned long)PeakResult*K1)/K2);
    Voltage = (unsigned int)temp;
    }
    
    //быстрая защита
    if ( (Voltage > UpTreshold) || ((Voltage < DownTreshold)&& (Voltage < FastProtectVoltageTreshold)) ) ProtectTimerOnValue = ProtectTimerOnMaxValue; //
    
    //медленная защита  
	if  ((Voltage < DownTreshold) && (Voltage >= FastProtectVoltageTreshold)) 
		{
        if (ProtectTimerOnValue) ProtectTimerOffValue=0; // если уже и так выключено - сразу сгоняем таймер выключения в 0
		if (ProtectTimerOffValue) ProtectTimerOffValue--; // тикает таймер выключения
		}
		else ProtectTimerOffValue = ProtectTimerOffMaxValue;
	if (ProtectTimerOffValue == 0) ProtectTimerOnValue = ProtectTimerOnMaxValue;

    if (ProtectTimerOnValue) PortRelay &= ~(1<<Relay);
      else   PortRelay |= (1<<Relay);     
         
    KeyboardThread ();
    UpdateLedScreen ();
  
    if ((DisplayMode == ProtectTimer)&&((ProtectTimerOnValue == ProtectTimerOnMaxValue) || (ProtectTimerOnValue == 0))) DisplayMode = RealtimeVoltage;
    if ((DisplayMode == RealtimeVoltage)&&(ProtectTimerOnValue == (ProtectTimerOnMaxValue - 1))) DisplayMode = ProtectTimer;
    
    if (QuarterSecPrescaler++ > 10)    //раз в четверть секунды сюда заходим
      {
      QuarterSecPrescaler = 0;
      DisplayedVoltage = Voltage;
      if ((ProtectTimerOnValue)&&(DisplayMode == RealtimeVoltage)) EnableDisplay += 128; // мигаем экраном
        else EnableDisplay = 128; // не мигаем
      }
  
    if (SecPrescaler++>90)
      {
      SecPrescaler = 0;
      if (ProtectTimerOnValue) ProtectTimerOnValue--;
      }
  }
}
