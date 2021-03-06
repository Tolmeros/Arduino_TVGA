#include "TVGA.h"

#if defined(__AVR_ATmega2560__)
  #error Not implemented yet.
#endif

//HSYNC pin used by TIMER2
#if defined(__AVR_ATmega2560__)
  #define HSYNCPIN 9
#else
  #define HSYNCPIN 3
#endif

//These two pin cannot be modified without modify the HSYNC assembler code
#if defined(__AVR_ATmega2560__)
  #define COLORPIN0 30
  #define COLORPIN1 31
#else
  #define COLORPIN0 6
  #define COLORPIN1 7
#endif

//VSYNC pin used by TIMER1. Can be 9 or 10
#if defined(__AVR_ATmega2560__)
  #define VSYNCPIN 11
#else
  #define VSYNCPIN 9
#endif

//Number of VGA lines to be skipped (black lines)
/*These lines includes the vertical sync pulse and back porch.
Minimum value must be 35 (calculate from Nick Gammon)
You can modify this value to center the framebuffer vertically, or not*/
#if defined(__AVR_ATmega2560__) && \
(defined(ATMEGA2560_HIGHRES) || defined(ATMEGA2560_MAXRES))
  #define SKIPLINES 32
#else
  #define SKIPLINES 90
#endif

static byte afreq, afreq0;
unsigned long vtimer;
static byte aline, rlinecnt;
static bool doublerl;
static byte vskip;
byte vgaxfb[VGAX_HEIGHT*VGAX_BWIDTH];

//VSYNC interrupt
ISR(TIMER1_OVF_vect) {
  aline=-1;
  vskip=SKIPLINES;
  vtimer++;
  rlinecnt=0;
  doublerl=1;
}
//HSYNC interrupt
ISR(TIMER2_OVF_vect) {
  /*
  NOTE: I prefer to generate the line here, inside the interrupt.
  Gammon's code generate the line pixels inside main().
  My versin generate the signal using only interrupts, so inside main() function
  you can do anything you want. Your code will be interrupted when VGA signal
  needs to be generated
  */
  //generate audio modulation. around 15 clocks
  asm volatile(                                   //4c to load Z and Y
    "      ld r16, Z                        \n\t" //c1 r16=afreq
    "      cpi %[freq0], 0                  \n\t" //c1 afreq0==0 ?
    "      breq no_audio                    \n\t" //c1/2 *0
    "play_audio:                            \n\t" 
    "      cpi r16, 0                       \n\t" //c1 afreq==0 ?
    "      brne dont_flip_audio_pin         \n\t" //c1/2 *1
    "flip_audio_pin:                        \n\t" 
    "      ldi r18, 1                       \n\t" //c1
    "      out %[audiopin], r18             \n\t" //c1
    "      st Z, %[freq0]                   \n\t" //c1 afreq=afreq0
    "      rjmp end                         \n\t" //c2
    //"    mov r16, %[freq0]\n\r"
    //"    dec r16\n\r"
    "no_audio:                              \n\t" 
    "      nop                              \n\t" //c1
    "      nop                              \n\t" //c1
    "      nop                              \n\t" //c1
    //"    nop                              \n\t" //c1
    "      nop                              \n\t" //c1
    "      nop                              \n\t" //c1
    "      nop                              \n\t" //c1
    "      rjmp end                         \n\t" //c2
    "dont_flip_audio_pin:                   \n\t" 
    "      dec r16                          \n\t" //c1
    "      st Z, r16                        \n\t" //c1
    //"    nop                              \n\t" //c1
    "end:                                   \n\t"
  :
  : "z" (&afreq),
    [freq0] "r" (afreq0),
    [audiopin] "i" _SFR_IO_ADDR(PINC)
  : "r16", "r18");
  
  //check vertical porch
  if (vskip) {
      vskip--;
      return;
  }
  if (rlinecnt<VGAX_HEIGHT) {   
    //interrupt jitter fix (needed to keep signal stable)
    //code from https://github.com/cnlohr/avrcraft/tree/master/terminal
    //modified from 4 nop align to 8 nop align
    #define DEJITTER_SYNC 9
    {
      uint8_t jitter;
      asm volatile(
        "     lds %[jitter], %[timer0]   \n\t" //
        "     subi %[jitter], %[tsync]   \n\t" //
        "     andi %[jitter], 7          \n\t" //
        "     ldi ZL, pm_lo8(NOP_SLIDE)  \n\t" //
        "     ldi ZH, pm_hi8(NOP_SLIDE)  \n\t" //
        "     add ZL, %[jitter]          \n\t" //
        "     adc ZH, __zero_reg__       \n\t" //
        "     ijmp                       \n\t" //
        "NOP_SLIDE:                      \n\t" //
        ".rept 7                         \n\t" //
        "     nop                        \n\t" //
        ".endr                           \n\t" //
      : [jitter] "=d" (jitter)
      : [timer0] "i" (&TCNT0),
        [tsync] "i" ((uint8_t)DEJITTER_SYNC)
      : "r30", "r31");
    }
    /*
    Output all pixels.

    NOTE: My trick here is to unpack 4 pixels and shift them before writing to
    PORTD.

    Pixels are packed as 0b11223344 because the first pixel write have no time
    to perform a shift (ld, out) and must be prealigned to the two upper bits 
    of PORTD, where the two wires of the VGA DSUB are connected. The second, 
    the third and the forth pixels are shifted left using mul opcode instead 
    of a left shift opcode. Shift opcodes are slow and can shift only 1 bit at
    a time, using 1 clock cycle. mul is faster.

    Instead of using a loop i use the .rept assembler directive to generate an 
    unrolled loop of 30 iterations.
    */
    /*
    asm volatile (
      "    ldi r20, 4       \n\t" //const for <<2bit
      #ifdef VGAX_DEV_DEPRECATED
      ".rept 14             \n\t" //center line
      "    nop              \n\t" //
      ".endr                \n\t" //
      #endif
      ".rept 30             \n\t" //output 4 pixels for each iteration
      "    ld r16, Z+       \n\t" //
      "    out %[port], r16 \n\t"
      "    nop              \n\t"
      ".endr                \n\t" //
      "    nop              \n\t" //expand last pixel
      "    ldi r16, 0       \n\t" //
      "    out %[port], r16 \n\t" //write black for next pixels
    :
    : [port] "I" (_SFR_IO_ADDR(PORTB)),
      "z" "I" ((byte*)vgaxfb + rlinecnt*VGAX_BWIDTH)
    : "r16", "r20", "memory");
    */
    /*
    SPCR |= (1 << SPE) | (1 << MSTR);
    asm volatile (
      "    ldi r20, 4       \n\t" //const for <<2bit
      #ifdef VGAX_DEV_DEPRECATED
      ".rept 14             \n\t" //center line
      "    nop              \n\t" //
      ".endr                \n\t" //
      #endif
      ".rept 20             \n\t" //center line
      "    nop              \n\t" //
      ".endr                \n\t" //
      ".rept 15             \n\t" //output 4 pixels for each iteration
      "    ld r16, Z+       \n\t" //
      "    out %[port], r16 \n\t"
      //"    out 0x2E, r16 \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      "    nop              \n\t"
      //"    nop              \n\t"
      //"    in r16, 0x2E \n\t"
      //"    in r16, 0x2D \n\t"
      //"    in r16, %[port] \n\t"
      //"    in r16, %[ports] \n\t"
      ".endr                \n\t" //
      "    nop              \n\t" //expand last pixel
    :
    : [port] "I" (_SFR_IO_ADDR(SPDR)),
      [ports] "I" (_SFR_IO_ADDR(SPSR)),
      "z" "I" ((byte*)vgaxfb + rlinecnt*VGAX_BWIDTH)
    : "r16", "r20", "memory");
    
    SPCR &= ~((1 << SPE) | (1 << MSTR));
    */
    
    /*
    SPCR |= (1 << SPE) | (1 << MSTR);
    asm volatile (
      "    ldi r20, 15      \n\t" //const for <<2bit
      #ifdef VGAX_DEV_DEPRECATED
      ".rept 14             \n\t" //center line
      "    nop              \n\t" //
      ".endr                \n\t" //
      #endif
      ".rept 20             \n\t" //center line
      "    nop              \n\t" //
      ".endr                \n\t" //
      "pixel_loop:          \n\t" //output 4 pixels for each iteration
      "    ld r16, Z+       \n\t" //
      "    out %[port], r16 \n\t" //write pixel byte
      "    dec r20          \n\t"
      "    breq pixel_loop_exit \n\t"
      //"    in r16, %[port] \n\t"
      "pixel_loop_wait:     \n\t"
      "    in r16, %[ports] \n\t"
      "    sbrs r16, 7      \n\t"
      "    rjmp pixel_loop_wait \n\t"
      "    rjmp pixel_loop \n\t"
      "pixel_loop_exit:     \n\t"
      "    nop              \n\t" //expand last pixel
    :
    : [port] "I" (_SFR_IO_ADDR(SPDR)),
      [ports] "I" (_SFR_IO_ADDR(SPSR)),
      "z" "I" ((byte*)vgaxfb + rlinecnt*VGAX_BWIDTH)
    : "r16", "r20", "memory");
    SPCR &= ~((1 << SPE) | (1 << MSTR));
    */

    #if 0
    SPCR |= (1 << SPE) | (1 << MSTR);
    asm volatile (
      "    ldi r20, 21      \n\t" //const for <<2bit
      #ifdef VGAX_DEV_DEPRECATED
      ".rept 14             \n\t" //center line
      "    nop              \n\t" //
      ".endr                \n\t" //
      #endif
      ".rept 14             \n\t" //center line
      "    nop              \n\t" //
      ".endr                \n\t" //
      "pixel_loop:          \n\t" //output 4 pixels for each iteration
      "    ld r16, Z+       \n\t" // C2
      "    out %[port], r16 \n\t" //write pixel byte
      "    dec r20          \n\t" // C1
      "    breq pixel_loop_exit \n\t" // F: C1, T:C2
      //"    in r16, %[port] \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      //"    nop             \n\t"
      "    rjmp pixel_loop \n\t" //C2
      "pixel_loop_exit:     \n\t"
      "    nop              \n\t" //expand last pixel
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
    :
    : [port] "I" (_SFR_IO_ADDR(SPDR)),
      [ports] "I" (_SFR_IO_ADDR(SPSR)),
      "z" "I" ((byte*)vgaxfb + rlinecnt*VGAX_BWIDTH)
    : "r16", "r20", "memory");
    SPCR &= ~((1 << SPE) | (1 << MSTR));
    #endif

    #if 1
    UCSR0B = (1<<TXEN0);
    
    asm volatile (
      #if 1
      "    ldi r16, 0       \n\t"
      "    sts 0xC6, r16    \n\t"
      "    .rept 8          \n\t" //
      "      nop            \n\t" //
      "    .endr            \n\t" //
      #endif
      "    ldi r20, 23      \n\t" //const for <<2bit
      "pixel_loop:          \n\t" //output 4 pixels for each iteration
      "    ld r16, Z+       \n\t" // C2
      "    sts 0xC6, r16 \n\t" //write pixel byte
      "    dec r20          \n\t" // C1
      "    breq pixel_loop_exit \n\t" // F: C1, T:C2
      //"    in r16, %[port] \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      //"    nop             \n\t"
      //"    nop             \n\t"
      //"    nop             \n\t"
      //"    nop             \n\t"
      "    rjmp pixel_loop \n\t" //C2
      "pixel_loop_exit:     \n\t"
      "    nop              \n\t" //expand last pixel
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
      "    nop             \n\t"
    :
    //: [port] "I" (UDR0),
    : "z" "I" ((byte*)vgaxfb + rlinecnt*VGAX_BWIDTH)
    : "r16", "r20", "memory");
    UCSR0B = 0;
    #endif

    //increment framebuffer line counter after 6 VGA lines
    #if defined(__AVR_ATmega2560__) && defined(ATMEGA2560_MAXRES)
      #define CLONED_LINES (2-1)
    #else
      //#define CLONED_LINES (6-1)
      #define CLONED_LINES (3-1)
    #endif
    if (++aline==CLONED_LINES) { 
      aline=-1;
      rlinecnt++;
      #if 0
      if (doublerl && (rlinecnt >= VGAX_HEIGHT)) {
        rlinecnt=0;
        doublerl=0;
      }
      #endif
    } else {
      #ifdef VGAX_DEV_DEPRECATED
      //small delay to keep the line signal aligned
      asm volatile(
        ".rept 17 \n\t" //
        "    nop  \n\t" //
        ".endr    \n\t" //
      :::);
      #endif
    }
  } 
}
void VGAX::begin(bool enableTone) {
  //Timers setup code, modified version of the Nick Gammon's VGA sketch
  cli();
  //setup audio pin
  if (enableTone) {
    pinMode(A0, OUTPUT);
  }
  //disable TIMER0 interrupt
  TIMSK0=0;
  TCCR0A=0;
  TCCR0B=(1 << CS00); //enable 16MHz counter (used to fix the HSYNC interrupt jitter)
  OCR0A=0;
  OCR0B=0;
  TCNT0=0;

  //TIMER1 - vertical sync pulses
  pinMode(VSYNCPIN, OUTPUT);
  #if VSYNCPIN==10 //ATMEGA328 PIN 10
  TCCR1A=bit(WGM10) | bit(WGM11) | bit(COM1B1);
  TCCR1B=bit(WGM12) | bit(WGM13) | bit(CS12) | bit(CS10); //1024 prescaler
  OCR1A=259; //16666 / 64 uS=260 (less one)
  OCR1B=0; //64 / 64 uS=1 (less one)
  TIFR1=bit(TOV1); //clear overflow flag
  TIMSK1=bit(TOIE1); //interrupt on overflow on TIMER1
  #else //ATMEGA328 PIN 9 or ATMEGA2560 PIN 11
  TCCR1A=bit(WGM11) | bit(COM1A1);
  TCCR1B=bit(WGM12) | bit(WGM13) | bit(CS12) | bit(CS10); //1024 prescaler
  ICR1=259; //16666 / 64 uS=260 (less one)
  OCR1A=0; //64 / 64 uS=1 (less one)
  TIFR1=bit(TOV1); //clear overflow flag
  TIMSK1=bit(TOIE1); //interrupt on overflow on TIMER1
  #endif

  //TIMER2 - horizontal sync pulses
  pinMode(HSYNCPIN, OUTPUT);
  TCCR2A=bit(WGM20) | bit(WGM21) | bit(COM2B1); //pin3=COM2B1
  TCCR2B=bit(WGM22) | bit(CS21); //8 prescaler
  OCR2A=63; //32 / 0.5 uS=64 (less one)
  OCR2B=7; //4 / 0.5 uS=8 (less one)
  TIFR2=bit(TOV2); //clear overflow flag
  TIMSK2=bit(TOIE2); //interrupt on overflow on TIMER2
  
  //pins for outputting the colour information
  //pinMode(COLORPIN0, OUTPUT);
  //pinMode(COLORPIN1, OUTPUT);

  //spi
  //DORD=1
  //CPOL=0
  //CPHA=0
  //SPCR = bit(SPE) | bit(DORD) | bit(MSTR);
  
  //pinMode(11, OUTPUT);
  //pinMode(13, OUTPUT);
  //SPCR = bit(SPE) | bit(MSTR);
  //SPCR = 0b01010000;
  //SPCR = 0b01010011;
  //SPSR = bit(SPI2X);
  //SPSR = 0b00000001;

  /*
  DDRB |= (1 << 5) | (1 << 3) | (1 << 2);
  //DDRB |= (1 << 5) | (1 << 3);
  DDRB &= ~(1 << 4);
  //PORTB |= (1 << 2);
  SPCR |= (1 << SPE) | (1 << MSTR);
  SPSR |= (1 << SPI2X);
  */

  UBRR0 = 0;
  UCSR0C = (1<<UMSEL01)|(1<<UMSEL00)|(0<<UCPHA0)|(0<<UCPOL0);
  //UCSR0B = (1<<TXEN0);
  UBRR0 = 0;
  

  sei();
}
void VGAX::end() {
  //disable TIMER0
  TCCR0A=0;
  TCCR0B=0;
  //disable TIMER1
  TCCR1A=0;
  TCCR1B=0;
  //disable TIMER2
  TCCR2A=0;
  TCCR2B=0;
}
void VGAX::tone(unsigned int frequency) {
  //HSYNC=32usec
  afreq=1000000 / frequency / 2 / 32;
  afreq0=afreq;
}
void VGAX::noTone() {
  afreq0=0;
}
void VGAX::delay(int msec) {
  while (msec--) {
    unsigned cnt=16000/32; //TODO: use a more precise way to calculate cnt
    while (cnt--)
      asm volatile("nop\nnop\nnop\nnop\n");
  }
}

void VGAX::copy(byte *src) {
  byte *o=(byte*)vgaxfb;
  unsigned cnt=VGAX_BSIZE;
  while (cnt--)
    *o++=pgm_read_byte(src++);
}
