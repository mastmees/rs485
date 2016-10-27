/*
MIT License

Copyright (c) 2016 Madis Kaal

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

// on beginning of start bit enable driver, turn on led, and set up
// timer to fire after 12 bit transmit times. This is for worst case
// where zero byte is transmitted and driver needs to be enabled for
// start bit, 8 data bits and a stop bit based on the single falling
// edge of the data line
//
// there is only 8 bit timer, to be able to count 12000 us we need to
// scale the numbers down. To accomplish that, we'll pre-scale the
// 9.6MHz clock 1024 times, resulting in 106.66us resolution, which works
// out great for common bit rates with exact 12 bit time and result of
// scaled down resolution:
//
// 1200    10000 us  10026 us   94 ticks  
// 2400    5000 us   5013 us    47 ticks
// 4800    2500 us   2560 us    24 ticks    
// 9600    1250 us   1280 us    12 ticks
// 19200   625 us    640 us     6 ticks
// 38400   313 us    320 us     3 ticks     
// 57600   209 us    213 us     2 ticks
// 115200  105 us    106 us     1 tick
//
// note that fuses must be set to run at 9.6 MHz internal clock at full speed

uint8_t ticktable[]={1,2,3,6,12,24,47,94};

// this is computed in main loop from ADC2 value, but set
// a default until first ADC cycle completes
volatile uint8_t pulseticks=256-6;

// the overflow happens once the counter overflows and resets to zero
// the clock prescaler is reset, so it actually counts entire tick before
// the first real tick count happens. To have an interrupt after the single
// tick we need to load the counter with 255, for two ticks with 254 and
// so on
ISR(INT0_vect)
{
  PORTB=(PORTB|0x01)&(~0x08);
  TCCR0B=0x05; // prescaler=1024
  TCCR0A=0x00; // mode 0
  GTCCR=1;     // reset clock prescaler
  TCNT0=pulseticks;
  TIFR0=0x02;  // clear any pending interrupts
  TIMSK0=0x02; // enable overflow interrupt
}

// when timer fires, meaning that no falling edges on transmit
// signal were detected during the timeout period
// turn off the driver if it was enabled and reschedule interrupt
// for turning off the led
// timer counter has zero value, and next overflow will happen in
// about 28 ms
ISR(TIM0_OVF_vect)
{
  if (PORTB&0x01) { // if driver is on
    PORTB&=(~0x01); // then disable it, but keep led on and wait for next
                    // overflow
  }
  else {
    PORTB=(PORTB|0x08); // no new data arrived, turn the led off
    TIMSK0=0x00;    // and disable overflow interrupt
  }
}

// WDT interrupt cancels the sleep so that watchdog can be reset
ISR(WDT_vect)
{
}

/*
I/O configuration
-----------------
I/O pin                               direction     DDR  PORT

PB0 Driver Enable                     output        1    0
PB1 Driver Input                      input,pullup  0    1
PB2 unused                            input,pullup  0    1
PB3 LED (active low)                  output        1    1
PB4 timing setting trimpot            input,ADC     0    0
PB5 unused                            input,pullup  0    1

See http://www.nomad.ee/micros/rs485/ for hardware info
*/
int main(void)
{
uint16_t av;
  MCUSR=0;
  MCUCR=0;
  // I/O directions and initial state
  DDRB=0x09;
  PORTB=0x2e;
  // configure sleep mode
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  // configure watchdog to interrupt&reset, 4 sec timeout
  WDTCR|=0x18;
  WDTCR=0xe8;
  // set up for external interrupts
  MCUCR=0x02; // falling edge on INT0 causes interrupt
  GIMSK=0x40; // enable INT0 interrupts
  // set up for reading trimpot
  DIDR0=0x10; // disable digital input on PB4
  ADMUX=0x02; // select PB4 for analog input
  ADCSRA=(1<<ADEN)|(1<<ADSC)|(1<<ADIF)|7; // ADC clock prescaler to 128
  sei();
  while (1) {
    sleep_cpu(); // any interrupt wakes us up
    wdt_reset();
    WDTCR|=0x40;
    // if ADC has finished conversion, then
    // compute the driver enable pulse time
    // as set by trimpot. take out this part
    // if you want to stick to default pulseticks
    // defined in a beginning
    if (ADCSRA&(1<<ADIF)) {
      av=ADCL;
      av|=(ADCH<<8);
      av=(av>>7)&7; // keep just top 3 bits
      pulseticks=256-ticktable[av];      
      ADCSRA=(1<<ADEN)|(1<<ADSC)|(1<<ADIF)|7;
    }
  }
}
