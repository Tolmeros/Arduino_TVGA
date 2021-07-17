/*
COPYRIGHT (C) 2014 Sandro Maffiodo
smaffer@gmail.com
http://www.sandromaffiodo.com

based on:
VGAX Library for Arduino UNO and MEGA by Sandro Maffiodo:
 https://github.com/smaffer/vgax
the "VGA color video generation" by Nick Gammon:
  http://www.gammon.com.au/forum/?id=11608.
inspired from the work of Peten Paja:
  http://petenpaja.blogspot.fi/2013/11/toorums-quest-ii-retro-video-game.html
AVR interrupt dejitter from Charles CNLOHR:
  https://github.com/cnlohr/avrcraft/tree/master/terminal
*/
#ifndef __TVGA_library__
#define __TVGA_library__

#define TVGA_VERSION "0.0.1"

#if defined(__AVR_ATmega2560__)
  #error Not implemented yet.
#endif

#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <avr/pgmspace.h>

//uncomment ATMEGA2560_HIGHRES to use 120x90px squared pixels
//#define ATMEGA2560_HIGHRES 

//uncomment ATMEGA2560_MAXRES to use 120x240px rectangular pixels
//#define ATMEGA2560_MAXRES

#if defined(__AVR_ATmega2560__) && defined(ATMEGA2560_MAXRES)
  #define VGAX_HEIGHT 240 //number of lines
#elif defined(__AVR_ATmega2560__) && defined(ATMEGA2560_HIGHRES)
  #define VGAX_HEIGHT 80 //number of lines
#else
  #define VGAX_HEIGHT 60 //number of lines
#endif
#define VGAX_BWIDTH 30 //number of bytes in a row
#define VGAX_WIDTH (VGAX_BWIDTH*4) //number of pixels in a row
#define VGAX_BSIZE (VGAX_BWIDTH*VGAX_HEIGHT) //size of framebuffer in bytes
#define VGAX_SIZE (VGAX_WIDTH*VGAX_HEIGHT) //size of framebuffer in pixels

//framebuffer. if you want you can write directly to this array. its safe
extern byte vgaxfb[VGAX_HEIGHT*VGAX_BWIDTH];

//clock replacement. this is increment in the VSYNC interrupt, so run at 60Hz
extern unsigned long vtimer;

//VGAX class. This is a static class. Multiple instances will not work
class VGAX {
public:
  /*
   * begin()
   * end()
   *    NOTES: begin() method reconfigure TIMER0 TIMER1 and TIMER2.
   *    If you need to shutdown this library, you need to call end() and
   *    reconfigure all the three timers by yourself. The lib will not
   *    restore the previous timers configuration
   */
  static void begin(bool enableTone=true);
  static void end();
  static void delay(int msec);
  /*
   * millis()
   *    return the number of milliseconds ellapsed
   */
  static inline unsigned long millis() {
    return vtimer*16;
  }
  /*
   * micros()
   *    return the number of microseconds ellapsed  
   */
  static inline unsigned long micros() {
    return vtimer*16000;
  }
  /*
   * tone(frequency)
   *    frequency: tone frequency
   *
   * Audio generation will start in the next horizontal interrupt. To mute
   * audio you need to call noTone after some time.
   */
  static void tone(unsigned int frequency);
  /*
   * noTone()
   *     stop the tone generation
   */
  static void noTone();
  /*
   * copy(src)
   *    src: source data. src size must be equal to framebuffer
   */
  static void copy(byte *src);
};
#endif

