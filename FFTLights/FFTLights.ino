
#include <stdint.h>
#include <ffft.h>



#define  IR_AUDIO  0 // ADC channel to capture



/** LIGHTS **/
#define clockpin 13 // CI
#define enablepin 10 // EI
#define latchpin 9 // LI
#define datapin 11 // DI
 
#define NumLEDs 1
 
int LEDChannels[NumLEDs][3] = {0};
int SB_CommandMode;
int SB_RedCommand;
int SB_GreenCommand;
int SB_BlueCommand;
/** /LIGHTS **/


volatile  byte  position = 0;
volatile  long  zero = 0;

int16_t capture[FFT_N];			/* Wave captureing buffer */
complex_t bfly_buff[FFT_N];		/* FFT buffer */
uint16_t spektrum[FFT_N/2];		/* Spectrum output buffer */

void setup()
{
  setupLights();
  setAllTo(1023,1023,1023);
  setupAudio();
  setAllTo(0,0,0);
}


void setupLights() {
   pinMode(datapin, OUTPUT);
   pinMode(latchpin, OUTPUT);
   pinMode(enablepin, OUTPUT);
   pinMode(clockpin, OUTPUT);
   SPCR = (1<<SPE)|(1<<MSTR)|(0<<SPR1)|(0<<SPR0);
   digitalWrite(latchpin, LOW);
   digitalWrite(enablepin, LOW);
}


void setupAudio() {
  Serial.begin(57600);
  adcInit();
  adcCalb();
}


// free running ADC fills capture buffer
ISR(ADC_vect)
{
  if (position >= FFT_N)
    return;
  
  capture[position] = ADC + zero;
  if (capture[position] == -1 || capture[position] == 1)
    capture[position] = 0;

  position++;
}
void adcInit(){
  /*  REFS0 : VCC use as a ref, IR_AUDIO : channel selection, ADEN : ADC Enable, ADSC : ADC Start, ADATE : ADC Auto Trigger Enable, ADIE : ADC Interrupt Enable,  ADPS : ADC Prescaler  */
  // free running ADC mode, f = ( 16MHz / prescaler ) / 13 cycles per conversion 
  ADMUX = _BV(REFS0) | IR_AUDIO; // | _BV(ADLAR); 
//  ADCSRA = _BV(ADSC) | _BV(ADEN) | _BV(ADATE) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) //prescaler 64 : 19231 Hz - 300Hz per 64 divisions
  ADCSRA = _BV(ADSC) | _BV(ADEN) | _BV(ADATE) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // prescaler 128 : 9615 Hz - 150 Hz per 64 divisions, better for most music
  sei();
}
void adcCalb(){
  Serial.println("Start to calc zero");
  long midl = 0;
  // get 2 meashurment at 2 sec
  // on ADC input must be NO SIGNAL!!!
  for (byte i = 0; i < 2; i++)
  {
    position = 0;
    delay(100);
    midl += capture[0];
    delay(900);
  }
  zero = -midl/2;
  Serial.println("Done.");
}






/** LIGHTS helper functions **/
void SB_SendPacket() {
 
    if (SB_CommandMode == B01) {
     SB_RedCommand = 120;
     SB_GreenCommand = 100;
     SB_BlueCommand = 100;
    }
 
    SPDR = SB_CommandMode << 6 | SB_BlueCommand>>4;
    while(!(SPSR & (1<<SPIF)));
    SPDR = SB_BlueCommand<<4 | SB_RedCommand>>6;
    while(!(SPSR & (1<<SPIF)));
    SPDR = SB_RedCommand << 2 | SB_GreenCommand>>8;
    while(!(SPSR & (1<<SPIF)));
    SPDR = SB_GreenCommand;
    while(!(SPSR & (1<<SPIF)));
 
}
 
void WriteLEDArray() {
 
    SB_CommandMode = B00; // Write to PWM control registers
    for (int h = 0;h<NumLEDs;h++) {
	  SB_RedCommand = LEDChannels[h][0];
	  SB_GreenCommand = LEDChannels[h][1];
	  SB_BlueCommand = LEDChannels[h][2];
	  SB_SendPacket();
    }
 
    delayMicroseconds(15);
    digitalWrite(latchpin,HIGH); // latch data into registers
    delayMicroseconds(15);
    digitalWrite(latchpin,LOW);
 
    SB_CommandMode = B01; // Write to current control registers
    for (int z = 0; z < NumLEDs; z++) SB_SendPacket();
    delayMicroseconds(15);
    digitalWrite(latchpin,HIGH); // latch data into registers
    delayMicroseconds(15);
    digitalWrite(latchpin,LOW);
 
}


void clearLights() {
  for (int i=0; i<NumLEDs; i++) {
   LEDChannels[0][0] = 0;
   LEDChannels[0][1] = 0;
   LEDChannels[0][2] = 0;
  }
  WriteLEDArray();
}
void setAllTo(int r, int g, int b) {
  r = r > 1023 ? 1023 : r;
  r = r < 0    ? 0    : r;
  g = g > 1023 ? 1023 : g;
  g = g < 0    ? 0    : g;
  b = b > 1023 ? 1023 : b;
  b = b < 0    ? 0    : b;
  for (int i=0; i<NumLEDs; i++) {
   LEDChannels[0][0] = r;
   LEDChannels[0][1] = g;
   LEDChannels[0][2] = b;
  }
  WriteLEDArray();
}

/*****************************/





int scale = 15;
int ar = 0;
int ag = 0;
int ab = 0;

void loop()
{
  if (position == FFT_N)
  {
    fft_input(capture, bfly_buff);
    fft_execute(bfly_buff);
    fft_output(bfly_buff, spektrum);

    for (byte i = 0; i < 64; i++){
      Serial.print(spektrum[i]);
      Serial.print(' ');
    }
    Serial.print(spektrum[0]);

    
   int r = spektrum[0]+spektrum[1]+spektrum[2]+spektrum[3]+spektrum[4];
   int g = spektrum[7]+spektrum[8]+spektrum[9]+spektrum[10]+spektrum[11];
   int b = spektrum[15]+spektrum[16]+spektrum[17]+spektrum[18]+spektrum[19];
   r = r*scale;
   g = g*scale;
   b = b*scale;
   ar = (ar * 2 + r)/3;
   ag = (ag * 2 + g)/3;
   ab = (ab * 2 + b)/3;
   Serial.print("||||||");
   Serial.print(r);
   Serial.print(",");
   Serial.print(g);
   Serial.print(",");
   Serial.print(b);
   Serial.print(",");
   
   Serial.println("!");
   setAllTo(ar, ag, ab);
   
   
   
   position = 0;
  }
}


