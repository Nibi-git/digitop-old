#define CtrlClockRate 8000000

#define PortAnode PORTA
#define PortAnodeDir DDRA

#define PortKathode PORTB
#define PortKathodeDir DDRB

#define PortKathodeAndRelayDir DDRB // 

#define Kathode_1 PB1
#define Kathode_2 PB0
#define Kathode_3 PB6

#define PortButton PORTB
#define PortButtonDir DDRB
#define PinButton PINB

#define Button_DN 2
#define Button_UP 5

#define PortRelay PORTB
#define PortRelayDir DDRB

#define Relay 3
#define InADC 7

#define SEG_A PA0
#define SEG_B PA7
#define SEG_C PA5
#define SEG_D PA3
#define SEG_E PA2
#define SEG_F PA1
#define SEG_G PA4
#define SEG_DP PA6

#define T0_RELOAD 0x83 //200 83
#define T1_RELOAD 0x9c // prescaler 4 9c
#define NUMBER_DIGITS 3


#define shortpress 1
#define midpress 80
#define longpress 150
#define KeyTimerMax 250

//code_state
#define DN_SHORT 1
#define UP_SHORT 2
#define DN_MID 3
#define UP_MID 4
#define DN_UP_MID 5

//DisplayMode
#define RealtimeVoltage 1
#define UpTresholdVoltage 2
#define DownTresholdVoltage 3
#define ProtectTimer 4

#define UpTresholdTune 50
#define DownTresholdTune 51
#define ProtectTimerTune 52
#define K1Tune 53

#define SamplesCount 100 // примерно
