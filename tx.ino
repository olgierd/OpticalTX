#include <JTEncode.h>

#define JT9                   // transmission mode [ JT9 / JT65 / CW / PSK ]
#define BASEFREQ 2620         // frequency [hz]
#define SIGNAL_PIN 4          // arduino output pin

#define JT_SERIALSYNC 0       // when enabled, starts transmission in JT modes after receiving any char on serial input

#define WPMSPEED 20

char message[] = "HELLO DR OM";





// don't touch this part, unless you know what you're doing //////////////////////////////////////////////////////
#ifdef JT65
#define SYMBOLTIME 372
#define SYMLENGTH 126
#define SYMTIME 2.6917
#define DELAYAFTER 13150
#endif

#ifdef JT9
#define SYMBOLTIME 580
#define SYMLENGTH 85
#define SYMTIME 1.736
#define DELAYAFTER 10750
#endif

#ifdef PSK
#define SYMBOLTIME 32
uint16_t varicode[] = { 1, 1, 887, 747, 863, 751, 765, 767, 239, 29, 31, 733, 31, 885, 939, 759, 1, 757, 941, 943, 859, 875, 877, 855, 891, 893, 951, 853, 861, 955, 763, 895, 1, 511, 351, 501, 475, 725, 699, 383, 251, 247, 367, 479, 117, 53, 87, 431, 183, 189, 237, 255, 375, 347, 363, 429, 427, 439, 245, 445, 493, 85, 471, 687, 701, 125, 235, 173, 181, 119, 219, 253, 341, 127, 509, 381, 215, 187, 221, 171, 213, 477, 175, 111, 109, 343, 437, 349, 373, 379, 685, 503, 495, 507, 703, 365, 735, 11, 95, 47, 45, 3, 61, 91, 43, 13, 491, 191, 27, 59, 15, 7, 63, 447, 21, 23, 5, 55, 123, 107, 223, 93, 469, 695, 443, 693, 727, 949 };
#endif

#ifdef CW
unsigned char morse[] = { 33,104,106,68,0,98,70,96,32,103,69,100,35,34,71,102,109,66,64,1,65,97,67,105,107,108,18,159,143,135,131,129,128,144,152,156,158 };
#define DITTIME 1200.0/WPMSPEED
#define DAHTIME 3*DITTIME-5
#define CWSHIFT 600
#endif

#define SETFREQ(x) OCR1A = 8000000/(x)

int x=0, y=0, t=0, key=0, val=0, len=0;
uint8_t symbols[255];
JTEncode jtencode;

void setup() {
  pinMode(SIGNAL_PIN, OUTPUT);     

  #ifdef JT65
    jtencode.jt65_encode(message, symbols);
  #endif

  #ifdef JT9
    jtencode.jt9_encode(message, symbols);
  #endif


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
  digitalWrite(4, t);
  digitalWrite(5, t);
  t=1-t;
}



#if defined(JT65) || defined(JT9)
void tx() {
  
  
  while(1) {
    if(JT_SERIALSYNC) { while(!Serial.available()) {} }
  
    while(1) {
      SETFREQ((BASEFREQ+symbols[x]*SYMTIME)+0.5);  // set appropriate frequency
      x++;
      delay(SYMBOLTIME);    // wait until symbol is transmitted
      if(x==SYMLENGTH) {    // after transmitting all symbols
        x=0;
        SETFREQ(500);         // fill the gap between transmissions with 2kHz constant tone
        if(JT_SERIALSYNC) break;
        delay(DELAYAFTER);
      }
    }
    
    if(JT_SERIALSYNC) { while(Serial.available()) {Serial.read();} }
  }
  
}
#endif

#if defined(PSK)
short cur_sym;
void tx() {
  SETFREQ(BASEFREQ);
  while(1) {
    for(x=0; x<sizeof(message); x++) {  
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
  }
}
#endif

#if defined(CW)
void tx() {
  
    for(x=0; x<sizeof(message)/sizeof(char); x++) {   // for every symbol
      key = message[x];

      // space
      if(key == ' ') delay(DAHTIME);

      // letters
      if(key >= 'A' && key <= 'Z') { val = morse[key-'A']; len = ((morse[key-'A'])>>5) + 1; }
      if(key >= 'a' && key <= 'z') { val = morse[key-'a']; len = ((morse[key-'a'])>>5) + 1; }

      // numbers
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
      
    }
  
}
#endif

void loop() {


    tx();
}


