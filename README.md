# Text VGA Library for Arduino UNO


## What is ArduinoTVGA?

This is a VGA library for Arduino UNO.  
To use this library you need only 4 resistors and one DSUB15 connector.  
This library require an ATMega328 MCU (or higher) MCU. Does not work with ATTINY family or ATMega168.

## Credits

Based on the [VGAX Library for Arduino UNO and MEGA](https://github.com/smaffer/vgax) by Sandro Maffiodo.

## Text

Expectations:
* 33 columns
* 20 rows
* font 5x7 pixels 
* char place 6x8 pixels (1 pixel white space between 5x7 text characters)


## Audio

## Wiring

You need:

- 1x 470ohm resistors
- 2x 68ohm resistors 
- 1x DSUB15 female connector

Then connect them like the following schema.  
*NOTE: The DSUB15 connector is shown from rear view*

## PIN and PORT

Video generation is implemented using PORTD, so **you cannot use any of the
PORTD pins**.

The vertical synchronization signal is generated on pin 9. Gammon's version use
the pin 10 but i prefer to keep pins 10 11 12 13 free for common SPI usage.

On Arduino MEGA PORTD is substituted to PORTA, vertical sync is pin 11 and horizontal pin is 9.

## Interrupt

VGAX library generate the video signal using only interrupts, so, inside main() function, you can do anything you want. Your code will be interrupted when VGA signal needs to be generated.

[Nick Gammon](http://www.gammon.com.au/forum/?id=11608)'s original code generate the line pixels inside main(). I prefer to generate the line inside interrupts to keep the MCU free to do some other things, like run games or play sounds.

**WARNING: You cannot add other interrupts or the VGA signal generation will be broken.**

## Timers

This library uses **all the 3 timers** of ATMega328 MCU. On ATMega2560 there are more unused timers.

*TIMER1* and *TIMER2* are configured to generate HSYNC and VSYNC pulses.
The setup code for these two timers has been [created by Nick Gammon](http://www.gammon.com.au/forum/?id=11608).
I have only made some modifications to use pin 9 instead of pin 10. On ATMega2560 HSYNC and VSYNC are different.

*TIMER0* is used to fix the interrupt jitter. I have modified an assembler trick
originally writen by [Charles CNLOHR](https://github.com/cnlohr/avrcraft/tree/master/terminal).

By default the *TIMER0* is used by Arduino to implement these functions:

	unsigned millis();
	unsigned long micros();
	void delay(ms); 
	void delayMicroseconds(us);

Instead of using these functions, you should use the alternative versions
provided by my library.

## Library usage

To use the VGAX library you need to include its header

    #include <Text_VGAX.h>

VGAX class is static, so you can use the class without create an instance of it:

    void setup() {
      Text_VGAX::begin();
    }

Or, if you prefer, you can create your instance, but keep in mind that cannot be
more than one VGAX instance at a time:

    Text_VGAX vga;

    void setup() {
      vga.begin();
    }


### Fonts


### Audio


## Tools
