#include <JTEncode.h>


#define MODE_JT65 1
#define MODE_JT9 2
#define MODE_PSK 3
#define MODE_CW 4

#define SIGNAL_PIN 4          // arduino output pin

// when "mode" is set to value other than 0, code runs in non-interactive mode
uint8_t mode=MODE_PSK;
// frequency
int BASEFREQ=1600;
uint8_t WPMSPEED = 20;
uint8_t messagelen=8;
char message[50] = "KANAPKA ";

uint8_t txloop=1;





#define JT65_SYMBOLTIME 372
#define JT65_SYMLENGTH 126
#define JT65_SYMSHIFT 2.6917
#define JT65_DELAYAFTER 13150

#define JT9_SYMBOLTIME 580
#define JT9_SYMLENGTH 85
#define JT9_SYMSHIFT 1.736
#define JT9_DELAYAFTER 10900

#define PSK_SYMBOLTIME 32

#define CWSHIFT 600

#define SETFREQ(x) OCR1A = 8000000/(x)


int SYMBOLTIME=0;
int SYMLENGTH=0;
double SYMSHIFT=0;
int DELAYAFTER=0;

int DITTIME, DAHTIME;

uint16_t varicode[] = { 1, 1, 887, 747, 863, 751, 765, 767, 239, 29, 31, 733, 31, 885, 939, 759, 1, 757, 941, 943, 859, 875, 877, 855, 891, 893, 951, 853, 861, 955, 763, 895, 1, 511, 351, 501, 475, 725, 699, 383, 251, 247, 367, 479, 117, 53, 87, 431, 183, 189, 237, 255, 375, 347, 363, 429, 427, 439, 245, 445, 493, 85, 471, 687, 701, 125, 235, 173, 181, 119, 219, 253, 341, 127, 509, 381, 215, 187, 221, 171, 213, 477, 175, 111, 109, 343, 437, 349, 373, 379, 685, 503, 495, 507, 703, 365, 735, 11, 95, 47, 45, 3, 61, 91, 43, 13, 491, 191, 27, 59, 15, 7, 63, 447, 21, 23, 5, 55, 123, 107, 223, 93, 469, 695, 443, 693, 727, 949 };

unsigned char morse[] = { 33,104,106,68,0,98,70,96,32,103,69,100,35,34,71,102,109,66,64,1,65,97,67,105,107,108,18,159,143,135,131,129,128,144,152,156,158 };

int x=0, y=0, t=0, key=0, val=0, len=0;
uint8_t symbols[130];
JTEncode jtencode;

void setup() {
  pinMode(SIGNAL_PIN, OUTPUT);     

  noInterrupts();           // disable  interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  Serial.begin(9600);

  SETFREQ(BASEFREQ-500);
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS10);    // prescaling 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts

}

ISR(TIMER1_COMPA_vect)          // timer interrupt service
{
  digitalWrite(SIGNAL_PIN, t);
  t=1-t;
}



void tx_jt() {
  
  
  while(1) {
  
    while(1) {
      Serial.println(symbols[x]);
      SETFREQ((BASEFREQ+symbols[x]*SYMSHIFT)+0.5);  // set appropriate frequency
      x++;
      delay(SYMBOLTIME);    // wait until symbol is transmitted
      if(x==SYMLENGTH) {    // after transmitting all symbols
        x=0;
        SETFREQ(BASEFREQ-500);         // fill the gap between transmissions with 2kHz constant tone
        if(!txloop) return;
        delay(DELAYAFTER);
      }
      if(Serial.read() == 'q') { return; }
    }
    
  }
  
}


void tx_psk() {
  short cur_sym;
  SETFREQ(BASEFREQ);
  
  while(1) {
    for(x=0; x<messagelen; x++) {  
      cur_sym = varicode[message[x]];
      y = 15;
      while((cur_sym & 1<<y) == 0) y--;
      t = 1-t; delay(SYMBOLTIME);   // two zeros to separate characters
      t = 1-t; delay(SYMBOLTIME);
      for(; y>=0; y--){
         if((cur_sym & (1<<y)) == 0) t = 1-t;    // if the current bit to transmit == 0 - invert signal phase
         delay(SYMBOLTIME);
      }
      
    }
    if(Serial.read() == 'q' | !txloop) { return; }
  }
}

void tx_cw() {
  uint8_t ena=1;
  
  while(1) {
    
    for(x=0; x<messagelen; x++) {   // for every symbol
      
      key = message[x];
      Serial.println(key);
      // space
      if(key == ' ') { delay(DAHTIME); continue; }

      // letters
      if(key >= 'A' && key <= 'Z') { val = morse[key-'A'];  }
      if(key >= 'a' && key <= 'z') { val = morse[key-'a'];  }

      len = (val>>5) + 1;

      // numbers and /
      if(key >= '/' && key <= '9') { val = morse[key-'0'+27]; len = 5; }

      // chars
      if(key == '?') { val = 12; len = 6; }
      if(key == ',') { val = 51; len = 6; }

      // transmit symbol
      while(len) {
        SETFREQ(BASEFREQ);      // transmit
        delay(( (val>>(len-1)) &1)?DAHTIME:DITTIME);    // for given time (dit or dah)
        SETFREQ(BASEFREQ+CWSHIFT);
        delay(DITTIME);       // dit time - spacing between symbols
        len--; 
      }
      delay(DAHTIME);   // dah time - spacing between letters

      if(Serial.read() == 'q' || !txloop) { return; }

   }
 }
  
}


int serialInt() {
  int ret = 0;
  char c;
  while(1) {
    while(!Serial.available()) {}
    c = Serial.read();
    if(!(c>='0' && c<='9')) {
      return ret;
    }
    else {
      ret = ret*10 + c-'0';
    }
  }
}

void readSerialMessage() {
  
  uint8_t x=0;
  char c;

  while(1) {
    while(!Serial.available()) {}
    c = Serial.read();
    if(c == '\n' || c == '\r') {
      message[x] = 0x00;
      messagelen =  x;
      return;
    }
    else {
      message[x] = c;
      x++;
    }
    
  }
  
}




void loop() {

  Serial.println("mode/freq/wpm/loop/msg");

  if(mode == 0) {
    mode = serialInt();
    BASEFREQ = serialInt();
    WPMSPEED = serialInt();
    txloop = serialInt();
    readSerialMessage();
  }

  switch(mode) {
    case MODE_JT65:
    {
      SYMBOLTIME = JT65_SYMBOLTIME;
      SYMLENGTH = JT65_SYMLENGTH;
      SYMSHIFT = JT65_SYMSHIFT;
      DELAYAFTER = JT65_DELAYAFTER;
      
      jtencode.jt65_encode(message, symbols);

      tx_jt();
      break;
    }
    case MODE_JT9:
    {
      SYMBOLTIME = JT9_SYMBOLTIME;
      SYMLENGTH = JT9_SYMLENGTH;
      SYMSHIFT = JT9_SYMSHIFT;
      DELAYAFTER = JT9_DELAYAFTER;

      jtencode.jt9_encode(message, symbols);

      tx_jt();
      break;
    }
    case MODE_PSK:
    {
      SYMBOLTIME = PSK_SYMBOLTIME;

      message[messagelen] = '\n';
      messagelen++;

      tx_psk();
      
      break;
    }
    case MODE_CW:
    {

      DITTIME = 1200.0/WPMSPEED;
      DAHTIME = 3*DITTIME-5;

      message[messagelen] = ' ';
      messagelen++;
      
      tx_cw();
      break;
    }
    
  }


  asm volatile ("  jmp 0");  

  
}


