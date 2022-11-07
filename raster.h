#define VSYNC 6
#define HSYNC 4
#define VIDEO 2
#define INTENSITY 0

void Hpulse() {
  // Creates a precise 8ms positive pulse
  uint32_t h_t = ARM_DWT_CYCCNT;
  digitalWriteFast(HSYNC, 1);
  while (ARM_DWT_CYCCNT - h_t < 6513);
  digitalWriteFast(HSYNC, 0);
}

void Hdraw(byte *hline, byte mode) {
  // Raster a horizontal line
  // Mode 0: 80 col
  // Mode 1: 132 col
  cli();
  uint32_t h_t = ARM_DWT_CYCCNT; // Restrict to 54.3 us
  if (mode == 1){ // 132 Column res
    uint32_t margin_t = ARM_DWT_CYCCNT;
    while (ARM_DWT_CYCCNT - margin_t < 600);
    for (int x = 0; x < 1059; x++) {
      uint32_t p_t = ARM_DWT_CYCCNT;
      digitalWriteFast(VIDEO, hline[x]);
      if (!hline[x]) asm volatile("nop\n nop\n");
      else asm volatile("nop\n");
      while (ARM_DWT_CYCCNT - p_t < 27);
    }
    digitalWriteFast(VIDEO, 0);
    
  } else { // 80 Column res
    delayNanoseconds(1000);
    for (int x = 0; x < 640; x++) {
      uint32_t p_t = ARM_DWT_CYCCNT;
      digitalWriteFast(VIDEO, hline[x]);
      if (!hline[x]) asm volatile("nop\n nop\n");
      else asm volatile("nop\n");
      while (ARM_DWT_CYCCNT - p_t < 51);//48
    }
  }
  digitalWriteFast(VIDEO, 0);
  while (ARM_DWT_CYCCNT - h_t < 44313); // Pad to 54.3 us
  sei();
}

void Hpad(){
  // Empty line Hsyncing to fill to end of VSYNC
    uint32_t h_t = ARM_DWT_CYCCNT;
    while (ARM_DWT_CYCCNT - h_t < 44313);
}

void draw(TERM *T){
  // Raster a full screen
  // Handle blinking here, every blinkval draws, invert all blinking
  uint32_t h_t = ARM_DWT_CYCCNT;
  T->currtty->blinkctr++;
  if (T->currtty->blinkctr > blinkval){
    T->currtty->blinking = !T->currtty->blinking;
    T->currtty->blinkctr = 0;
  }
  while (ARM_DWT_CYCCNT - h_t < 44313);
  Hpulse();
  for (int d = 0; d < 0; d++){ // Top Margin
    Hpad();
    Hpulse();
  }
  for (int j = 0; j < T->currfb->y_res; j++){
    byte * line = T->currfb->pixbuf[j];
    byte mode = 0;
    if (T->currtty->cols > 80) mode = 1;
    Hdraw(line, mode);
    Hpulse();
  }
}
