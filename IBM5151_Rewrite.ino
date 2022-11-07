// Video driver for IBM 5151 monitor
// Overclock to 816MHz for proper function

//#include <avr/io.h>
//#include <avr/interrupt.h>
#include "term.h"
#include "raster.h"

#define VSYNC 6
#define HSYNC 4
#define VIDEO 2
#define INTENSITY 0

TERM *term0;

void setup() {
    // Pins
    pinMode(HSYNC, OUTPUT);
    pinMode(VSYNC, OUTPUT);
    pinMode(VIDEO, OUTPUT);
    pinMode(INTENSITY, OUTPUT);
    digitalWrite(INTENSITY, HIGH); // Analog for less?
    Serial.begin(9600); // Host
    Serial5.begin(9600); // Keyboard (Nano)

    term0 = initTERM(80);
    
    /*
     * Test sequence
     */
 
    byte c = 0;
    for (int j = 1; j < term0->currtty->rows; j++) {
      for (int i = 1; i <= term0->currtty->cols; i++) {
        term0->currtty->charbuf[j][i] = c++;//(j % 10)+'0'; // test pattern
        //render_char(term0, i, j, 0);
      }
    }
    //term0->currtty->revbuf[1][1] = 1;
    //set_cursor(term0, 10, 10);
    //parse(term0, 'T');
    //set_cursor(term0, 15, 10);
    //parse(term0, 'E');
    //set_cursor(term0, 10, 15);
    //parse(term0, 'S');
    //set_cursor(term0, 15, 15);
    //parse(term0, 'T');
    //for (int i = 0; i < 25; i++)
    //set_cursor(term0, 1, 38);
    //scroll_up(term0, 1, 1);
    //line_feed(term0);

}


void loop() {

  // Raster
  uint32_t vclock = ARM_DWT_CYCCNT;
  digitalWriteFast(VSYNC, 1);
  draw(term0);

// #########
// Run in a single empty line
// #########

  uint32_t raster_t = ARM_DWT_CYCCNT;
  // Section1 : read from Serial
  for (int i = 0; i < 20; i++) { // Find proper limit?
      if (Serial.available() > 0) {
          char c = (char)Serial.read();
          //insert_character(term0, c); //DEBUG
          parse(term0, c);
      }
  }
  // Read keyboard
  while (Serial5.available() > 0) {
    char c = (char)Serial5.read();
    //if (c < 0) continue; // TODO Special keys
    Serial.write(c); // Send input to host
  }
  while (ARM_DWT_CYCCNT - raster_t < 44313); // Pad to 54.3 us
  Hpulse();
  // Two more opportunities for processing
  timedRender(term0);
  Hpulse();
  timedRender(term0);
  Hpulse();
  timedRender(term0);
  Hpulse();
  timedRender(term0);
  Hpulse();
  Hpad(); Hpulse();
  
// #########
// Run repeatedly to 20ms 
// #########
  while (ARM_DWT_CYCCNT - vclock < 16323000){
    // Continue pulses
      timedRender(term0);
      Hpulse();
  }
  while (ARM_DWT_CYCCNT - vclock < 16326530); // Pad to 20 ms

  // 850 us low pulse
  digitalWrite(VSYNC, 0);
  vclock = ARM_DWT_CYCCNT;
  while (ARM_DWT_CYCCNT - vclock < 661050){
    timedRender(term0);
    //Hpad();
    Hpulse();
  }
}
