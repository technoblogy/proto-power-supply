/* Proto Power Supply
 
   David Johnson-Davies - www.technoblogy.com - 4th November 2017
   ATmega328P @ 8 MHz (internal oscillator; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

// Seven-segment definitions
const int charArrayLen = 12;
char charArray[charArrayLen] = {
//  ABCDEFG  Segments
  0b1111110, // 0
  0b0110000, // 1
  0b1101101, // 2
  0b1111001, // 3
  0b0110011, // 4
  0b1011011, // 5
  0b1011111, // 6
  0b1110000, // 7
  0b1111111, // 8
  0b1111011, // 9
  0b0000000, // 10  Space
  0b0000001  // 11  Dash
};

const int Dash = 11;
const int Space = 10;
const int Ndigits = 3;                 // Number of digits
const int Steps = 220;                 // Divide range into 220 steps of 0.025V
volatile int Count = 0;                // Encoder value goes from 0 to Steps-1

volatile int Buffer[] = { Dash, Dash, Dash };
volatile int digit = 0;

// Pin assignments; give the bit position in PORTD for each digit
int Digits[] = {5, 6, 7};

// All voltages are in mV
const int MaxVoltage = 5500;           // Maximum voltage; ie 5.50  V
const int Stepsize = 25;               // Stepsize; ie 0.025V
volatile int Voltage = 0;              // Current voltage setting
volatile int Target = 0;               // Target voltage for regulator
volatile bool Overload = false;        // Overload trip

// Set the PWM Voltage **********************************************

void SetVoltage (int Count) {
  Target = Count * Stepsize;
  OCR2B = Count;
}

// Rotary Encoder **********************************************

const int A = PINC2;
const int B = PINC3;

volatile int a0;
volatile int c0;

// Called when encoder value changes
void ChangeValue (bool Up) {
  Count = max(min((Count + (Up ? 1 : -1)), Steps-1), 0);
  SetVoltage(Count);
  Overload = false;
}

// Pin change interrupt service routine
ISR (PCINT1_vect) {
  int a = PINC>>A & 1;
  int b = PINC>>B & 1;
  if (a != a0) {              // A changed
    a0 = a;
    if (b != c0) {
      c0 = b;
      ChangeValue(a == b);
    }
  }
}

// Push button interrupt on INT0
ISR (INT0_vect) {
  Overload = true;
  SetVoltage(0);
  Count = 0;
}

// Display multiplexer **********************************************

void DisplayNextDigit() {
  static int LastVoltage;
  DDRD = DDRD & ~(1<<Digits[digit]);         // Make previous digit bit an input
  digit = (digit+1) % (Ndigits+1);
  if (digit < Ndigits) {
    char segs = charArray[Buffer[digit]];
    if (digit==0) segs = segs | 0x80;        // Decimal point
    PORTB = ~segs;
    DDRB = segs;
    DDRD = DDRD | 1<<Digits[digit];          // Make digit bit an output
    PORTD = PORTD | 1<<Digits[digit];        // Take digit bit high
  } else {
    DDRB = 0;                                // All segments off
    int Voltage = (ReadADC() * (long)MaxVoltage)/1024;
    // Add hysteresis to stop display jumping
    if (abs(LastVoltage - Voltage) >= 10) {
      LastVoltage = Voltage;
      // Overload protection - if output is more than 0.5V below target
      if (Target - Voltage > 500) {
        Overload = true;
        SetVoltage(0);
        Count = 0;
      }
      if (Overload) Buffer[0] = Buffer[1] = Buffer[2] = Dash;
      else Display(Voltage/10);
    }
  }
}

// Display a three digit decimal number
void Display (int i) {
  for (int d=2; d>=0 ; d--) {
    Buffer[d]=i % 10;
    i = i / 10;
  }
}

// Timer interrupt - multiplexes display and does ADC conversions
ISR (TIMER2_OVF_vect) {
  DisplayNextDigit();
}

// Do ADC conversion **********************************************

int ReadADC () {
  uint8_t low, high;
  ADCSRA = ADCSRA | 1<<ADSC;           // Start
  while (ADCSRA & 1<<ADSC);            // Wait while conversion in progress
  low = ADCL;
  high = ADCH;
  return high<<8 | low;
}

// Setup **********************************************

void setup() {
  DDRB = 0;                            // All segments off

  // Configure INT0 interrupt input (PD2)
  PORTD = 1<<PORTD2;                   // Set input pullup
  EICRA = 2<<ISC00;                    // Interrupt on falling edge
  EIMSK = 1<<INT0;                     // Enable interrupt
  
  // Configure rotary encoder input
  PORTC = 1<<PORTC2 | 1<<PORTC3;       // Set input pullups on A and B
  PCMSK1 = 1<<PCINT10;                 // Pin change interrupt on A (PC2/PCINT10)
  PCICR = 1<<PCIE1;                    // Enable interrupt
  PCIFR = 1<<PCIF1;                    // Clear interrupt flag
  
  // Set up Timer/Counter2 to generate PWM on OC2B (PD3) and multiplex the display
  DDRD = 1<<DDD3;                      // Make PD3 (OC2B) an output for PWM
  TCCR2A = 2<<COM2B0 | 1<<WGM20;       // Normal output, Phase Correct PWM to OCR2A
  TCCR2B = 1<<WGM22 | 2<<CS20;         // Divide clock by 8
  OCR2A = Steps-1;                     // Divide range into 220 steps of 0.025V
  TIMSK2 = 1<<TOIE2;                   // Enable overflow interrupt
  
  // Set up ADC on ADC0 (PC0)
  ADMUX = 1<<REFS0 | 0<<MUX0;          // Vcc as ref, ADC0 (PC0)
  ADCSRA = 1<<ADEN | 7<<ADPS0;         // Enable ADC, 62.5kHz clock
}

// Everything done under interrupt
void loop () {
}
