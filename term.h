// This Header holds the code for the TERM struct and related
//
#define max_cols 132 
#define max_rows 38
#define blinkval 30
#include "charset.h"

// Data structures and Initializers
// ################################

typedef struct TTY{
// Controls tty behavior of a terminal
	// Holds 2D character array
	char ** charbuf; 			        // Holds the characters on screen
	char ** revbuf; 		          // Character reversed if 1, normal 0
	char ** blinkbuf; 			      // Character blinks if 1, normal 0
	int blinkctr; char blinking; // Increments until blinking all in blinkbuf
	int cols; int rows; 		      // Accessible dimensions of charbuf
	int char_w; int char_h;		    // Individual char's pixel res
	int cursor_x; int cursor_y;	  // Cursor position
	int savcur_x; int savcur_y;	  // Saved cursor position
	int scl_top; int scl_bot;     // Top and bottom of scrolling region
	int scl_left; int scl_right;  // Left and Right margin of scrolling region
	char ** rendbuf;			        // Track rendered characters
  char ** rendrev_buf;          // Track inverted rendered characters
	char graphic_mode;			      // new chars are 0:normal, 5:blink, 7:reverse
  char save_graphic_mode;       // Save blink and reverse
	char screen_mode;			        // reverse screen, 0 dark, 1 light mode
	char auto_wrap_mode;		      // 1 for autowrap, 0 for stop at margin
  char insert_replace_mode;     // 1 for replace, 0 for insert and offset
  char *tab_stops;              // Track if tab advances to these columns 
  char line_drawing;            // 1 for line_Drawing charset, 0 for normal
  char cursor_visible;          // Render cursor or not
} TTY;

typedef struct FB{
// Controls a framebuffer pixel array
	char ** pixbuf;						    // Holds pixel states
	int x_res; int y_res; 			  // Pixel resolution
} FB;

TTY * initTTY(int cols, int rows){
// Initiate TTY for given size
	TTY *newTTY = (TTY*)malloc(sizeof(TTY));
	newTTY->cols = cols; newTTY->rows = rows;
	cols = max_cols+2; rows = max_rows+2; // Give 1 char margin of safety
						  // Also allows for indexing starting at 1, safer scrolling
  // 2D arrays
	newTTY->charbuf      = (char**)malloc(rows*sizeof(char*));
	newTTY->revbuf       = (char**)malloc(rows*sizeof(char*));
	newTTY->blinkbuf     = (char**)malloc(rows*sizeof(char*));
	newTTY->rendbuf      = (char**)malloc(rows*sizeof(char*));
  newTTY->rendrev_buf  = (char**)malloc(rows*sizeof(char*));
	for (int j = 0; j < rows; j++){
		newTTY->charbuf[j]      = (char*)malloc(cols*sizeof(char));
		newTTY->revbuf[j]       = (char*)malloc(cols*sizeof(char));
		newTTY->blinkbuf[j]     = (char*)malloc(cols*sizeof(char));
		newTTY->rendbuf[j]      = (char*)malloc(cols*sizeof(char));
    newTTY->rendrev_buf[j]  = (char*)malloc(cols*sizeof(char));
    // Zero out these
    for (int i = 0; i < cols; i++){
      newTTY->charbuf[j][i] = ' '; newTTY->rendbuf[j][i] = ' ';
      newTTY->blinkbuf[j][i] = 0; newTTY->revbuf[j][i] = 0; newTTY->rendrev_buf[j][i] = 0;
    }
	}
  newTTY->tab_stops = (char*)malloc(cols*sizeof(char));
  for (int i = 0; i < cols; i++){
    newTTY->tab_stops[i] = (i % 8 == 1); // 1 on 9, 17, 25, 33, ..., 129
  }
  // Other attributes to sane defaults
	newTTY->cursor_x = 1; newTTY->cursor_y  = 1; 
	newTTY->savcur_x = 1; newTTY->savcur_y  = 1; newTTY->save_graphic_mode = 0;
	newTTY->scl_top  = 1; newTTY->scl_bot   = newTTY->rows;
	newTTY->scl_left = 1; newTTY->scl_right = newTTY->cols;
	newTTY->blinkctr = 0; newTTY->blinking = 0; newTTY->graphic_mode    = 0;	newTTY->screen_mode     = 0;
	newTTY->auto_wrap_mode  = 1;
  newTTY->insert_replace_mode = 1;
  newTTY->line_drawing = 0;
  newTTY->cursor_visible = 1;
	return newTTY;
}

FB * initFB(int x_res, int y_res){
// Initiate FB for given size
	FB *newFB = (FB*)malloc(sizeof(FB));
	newFB->x_res = x_res; newFB->y_res = y_res;
	newFB->pixbuf = (char**)malloc(y_res*sizeof(char*));
	for (int j = 0; j < y_res; j++){
		newFB->pixbuf[j] = (char*)malloc(x_res*sizeof(char));
    // Zero out pixels
    for (int i = 0; i < x_res; i++)
      newFB->pixbuf[j][i] = 0;
	}
	return newFB;
}

typedef struct TERM{
// Master device
	// curr(ent) tty/fb allows for hot swapping, maybe add later
	TTY *currtty;
 	TTY *tty0;
	FB  *currfb;
 	FB  *fb0;
	char col_mode; 
} TERM;

TERM * initTERM(byte col_mode){
// Construct a TERM object and all of its child objects
	TERM *newTERM = (TERM*)malloc(sizeof(TERM));
	newTERM->col_mode = col_mode;
  int cols = 80;
  if (col_mode == 132) cols = 132; 
	newTERM->tty0 = initTTY(cols, max_rows); // Maxsize with movable border
	newTERM->currtty = newTERM->tty0;
  // TODO: Currently does not respect curtty's char_h/w
	newTERM->fb0 = initFB((max_cols+1)*8, (max_rows+1)*8);
	newTERM->fb0->x_res = 8*newTERM->tty0->cols;
	newTERM->currfb = newTERM->fb0;
	return newTERM;
}

// FrameBuffer controls
// ####################
void byte_on_screen(TERM *T, byte b, int x, int y, byte rev){
  //Puts a byte into the consecutive fb spaces beginning at x,y
  for (int i = 0; i < 8; i++){
    T->currfb->pixbuf[y][x+i] = ((((b << i) & 128) > 0) ^ rev);
  }
}

void render_char(TERM *T, int x, int y, bool rev){
// Loads the character cell's contents at x,y into the framebuffer
  // Also applies reverse and blinking attributes
  if ((x > T->currtty->cols) || (y > T->currtty->rows) || (x < 1) || (y < 1))
    return;
  char chr = T->currtty->charbuf[y][x];
    
  for (int j = 0; j < 8; j++){
    // Excuse the math, converts from 1-indexed character cell in tty to 0 indexed fb
    byte_on_screen(T, charset[chr * 8 + j], (x-1)*8, (y-1)*8+j, rev);
  }
  T->currtty->rendbuf[y][x] = T->currtty->charbuf[y][x];
  T->currtty->rendrev_buf[y][x] = rev; 
}

void timedRender(TERM *T){
// Render for the length of a horizontal line
  uint32_t render_t = ARM_DWT_CYCCNT;
  static int r_x = 1; static int r_y = 1;
  while (ARM_DWT_CYCCNT - render_t < 18000){ // Safety timing?
    // Only render if needed to avoid jitter so: // Unrendered char
    // Real reverse, if reversed and not blinking, or blinking
    bool is_cursor = (r_x == T->currtty->cursor_x) && (r_y == T->currtty->cursor_y);
    bool rev = T->currtty->revbuf[r_y][r_y] ^ ((T->currtty->blinkbuf[r_y][r_y] &&
    T->currtty->blinking) || (is_cursor && T->currtty->blinking));
    if ((T->currtty->charbuf[r_y][r_x] != T->currtty->rendbuf[r_y][r_x]) ||
       (T->currtty->rendrev_buf[r_y][r_x] != rev)){
      // Only render if needed
      render_char(T, r_x, r_y, rev);
    }
    // Increment position
    r_x++;
    if (r_x > T->currtty->cols){
      r_x = 1; r_y++;
    }
    if (r_y > T->currtty->rows) r_y = 1;
  }
  while (ARM_DWT_CYCCNT - render_t < 44313); // Pad to 54.3 us
}

// Cursor controls, cursor movements do not scroll
// ####################

bool is_in_margins(TERM *T){
// Returns true if cursor is in scroll margins
  return ((T->currtty->cursor_x >= T->currtty->scl_left) &&
    (T->currtty->cursor_x <= T->currtty->scl_right) &&
    (T->currtty->cursor_y >= T->currtty->scl_top) &&
    (T->currtty->cursor_y <= T->currtty->scl_bot));
}

void cursor_up(TERM *T, int cnt){
	if (T->currtty->cursor_y - cnt >= T->currtty->scl_top) T->currtty->cursor_y -= cnt;
	else T->currtty->cursor_y = T->currtty->scl_top;
}

void cursor_down(TERM *T,int cnt){
	if (T->currtty->cursor_y + cnt <= T->currtty->scl_bot)
	       	T->currtty->cursor_y += cnt;
	else T->currtty->cursor_y = T->currtty->scl_bot;
}

void cursor_left(TERM *T, int cnt){
	if (T->currtty->cursor_x - cnt >= T->currtty->scl_left) T->currtty->cursor_x -= cnt;
	else T->currtty->cursor_x = T->currtty->scl_left;
}

void cursor_right(TERM *T, int cnt){
  if (T->currtty->cursor_x + cnt <= T->currtty->scl_right) T->currtty->cursor_x += cnt;
  else T->currtty->cursor_x = T->currtty->scl_right;
}

void set_cursor(TERM *T, int x, int y){
// Force cursor position
	if (x < 1) x = 1;
	if (x > T->currtty->cols) x = T->currtty->cols;
	if (y < 1) y = 1;
	if (y > T->currtty->rows) y = T->currtty->rows;
	T->currtty->cursor_x = x; T->currtty->cursor_y = y;
}

void save_cursor(TERM *T){
// Save cursor position
	T->currtty->savcur_x = T->currtty->cursor_x;
	T->currtty->savcur_y = T->currtty->cursor_y;
  T->currtty->save_graphic_mode = T->currtty->graphic_mode;
}

void report_cursor_position(TERM *T){
	Serial.write(27); // Escape
	Serial.write('[');
	Serial.print(T->currtty->cursor_y);
	Serial.write(';');
  Serial.print(T->currtty->cursor_x);
  Serial.write('R'); 
}

void report_terminal_ok(TERM *T){
  Serial.write(27); // Escape
  Serial.write('[');
  Serial.write('0');
  Serial.write('n'); 
}

void restore_cursor(TERM *T){
// Load cursor position from saved
	T->currtty->cursor_x = T->currtty->savcur_x;
	T->currtty->cursor_y = T->currtty->savcur_y;
  T->currtty->graphic_mode = T->currtty->save_graphic_mode;
	// If outside visible area, move to nearest extreme
	if (T->currtty->cursor_x < 1) T->currtty->cursor_x = 1;
	if (T->currtty->cursor_y < 1)	T->currtty->cursor_y = 1;
	if (T->currtty->cursor_x > T->currtty->cols) T->currtty->cursor_x = T->currtty->cols;
	if (T->currtty->cursor_y > T->currtty->rows) T->currtty->cursor_y = T->currtty->rows;
}

void tab_clear(TERM *T, int Pn){
// Remove tab stops
  if (Pn == 0){ // Only clear at current space
    T->currtty->tab_stops[T->currtty->cursor_x] = 0;
  } else if (Pn == 3){ // Clear all
    for (int i = 0; i <= max_cols; i++)
      T->currtty->tab_stops[i] = 0;
  }
}


void tab_set(TERM *T){
// Set tab stops
  T->currtty->tab_stops[T->currtty->cursor_x] = 1;
}

void tab(TERM *T){
// Move active position to the next tab stop on the current line
  int margin = T->currtty->cols;
  if (is_in_margins(T)) margin = T->currtty->scl_right;
  for (int i = T->currtty->cursor_x; i <= margin; i++){
    if (T->currtty->tab_stops[i]){
      T->currtty->cursor_x = i;
      return;
    }
  }
  T->currtty->cursor_x = margin;
}

// Scroll controls
// ####################
void set_vert_scroll_margins(TERM *T, int top, int bot){
// Set the margins, return cursor to origin
  //Defaults
  if (top == 0) top = 1;
  if (bot == 0) bot = T->currtty->rows;
	// If region is illegal, ignore 
	if ((top < 1) || (top > bot)) return;
	if (bot > T->currtty->rows) return;
	// Set origin to home
	T->currtty->scl_top = top; T->currtty->scl_bot = bot;
	T->currtty->cursor_x = 1;  T->currtty->cursor_y = 1;
}
	
void set_horiz_scroll_margins(TERM *T, int left, int right){
// Set the margins, return cursor to origin
  // Defaults
  if (left == 0) left = 1;
  if (right == 0) right = T->currtty->cols;
  // If region is illegal, ignore 
	if ((left < 1) || (left > right)) return;
	if (right > T->currtty->cols) return;
	// Set origin to home
	T->currtty->scl_left = left; T->currtty->scl_right = right;
	T->currtty->cursor_x = 1;    T->currtty->cursor_y = 1;
}

void scroll_up(TERM *T, int top_line, int count){
// Scroll the screen from top_line down to the bottom margin
	// Use scroll region
	int l = T->currtty->scl_left; int r = T->currtty->scl_right;

	if (top_line < 1) top_line = 1;
	while (count > 0){
		// Copy each line up one
		for (int j = top_line; j < T->currtty->scl_bot; j++){
			for (int i = l; i <= r; i++)
				T->currtty->charbuf[j][i] = T->currtty->charbuf[j+1][i];
		}
		// Bottom row now blank
		for (int i = l; i <= r; i++)
			T->currtty->charbuf[T->currtty->scl_bot][i] = ' ';
		count--;
	}
}

void scroll_down(TERM *T, int top_line, int count){
// Scroll the screen down from top line to bottom margin
// Top line is replaced with empty line
	// Use scroll region
	int l = T->currtty->scl_left; int r = T->currtty->scl_right;
	if (top_line < 1) top_line = 1;
	while (count > 0){
		for (int j = T->currtty->scl_bot; j > top_line; j--){
			for (int i = l; i <= r; i++)
				T->currtty->charbuf[j][i] = T->currtty->charbuf[j-1][i];
		}
		for (int i = l; i <= r; i++)
			T->currtty->charbuf[top_line][i] = ' ';
		count--;
	}
}

void scroll_left(TERM *T, int left_col, int count){
	// Use scroll region
	int top = T->currtty->scl_top; int bot = T->currtty->scl_bot;
// Scroll all text between margins left by count
	if (left_col < 1) left_col = 1;
	//if (left_col > T->currtty->cols) left_col = T->currtty->cols
	while (count > 0){
		for (int j = top; j <= bot; j++){
			for (int i = left_col; i < T->currtty->scl_right; i++)
				T->currtty->charbuf[j][i] =  T->currtty->charbuf[j][i+1];
			T->currtty->charbuf[j][T->currtty->cols] =  ' ';
		}
		count--;
	}
}

void scroll_right(TERM *T, int left_col, int count){
// Scroll all text between margins left by count
	// Use scroll region
	int top = T->currtty->scl_top; int bot = T->currtty->scl_bot;
	if (left_col < 1) left_col = 1;
	while (count > 0){
		for (int j = top; j <= bot; j++){
			for (int i = T->currtty->scl_right; i > left_col; i--)
				T->currtty->charbuf[j][i] =  T->currtty->charbuf[j][i-1];
			T->currtty->charbuf[j][left_col] =  ' ';
		}
		count--;
	}
}

int end_of_line(TERM *T, int line){
// Returns rightmost character in line
	for (int i = T->currtty->cols; i >=1; i--)
		if (T->currtty->charbuf[line][i] != ' ') return i;
	return 1;
}

void line_feed(TERM *T){ 
// Move cursor down one, if at bottom, scroll up
	if (T->currtty->cursor_y > T->currtty->scl_bot){
    // If below scroll region, move down but don't scroll
    if (T->currtty->cursor_y < T->currtty->rows)
        T->currtty->cursor_y++;
	} else if (T->currtty->cursor_y < T->currtty->scl_bot)
		T->currtty->cursor_y++;
	else
		scroll_up(T, T->currtty->scl_top, 1);
  //int eol = end_of_line(T, T->currtty->cursor_y);
	//if (T->currtty->cursor_x > eol) T->currtty->cursor_x = eol;
}

void reverse_line_feed(TERM *T){ 
// Move cursor up one, if at top, scroll down
  if (T->currtty->cursor_y > T->currtty->scl_top)
    T->currtty->cursor_y--;
  else
    scroll_down(T, T->currtty->scl_top, 1);
  //int eol = end_of_line(T, T->currtty->cursor_y);
  //if (T->currtty->cursor_x > eol) T->currtty->cursor_x = eol;
}

void carriage_return(TERM *T){
// If inside scroll area, move to left margin, else left edge of screen
  if (is_in_margins(T)){
        T->currtty->cursor_x = T->currtty->scl_left;
  } else {
    T->currtty->cursor_x = 1;
  }
}

void delete_char(TERM *T, int Pn){
// Delete character at active position and move chars to right left to fill space
  int x = T->currtty->cursor_x; int y = T->currtty->cursor_y;
  // Don't delete at illegal position
  if ((x < 1) || (x > T->currtty->cols)) return;
  
  for (int i = x; i <= T->currtty->scl_right - Pn; i++){
    T->currtty->charbuf[y][i] = T->currtty->charbuf[y][i+Pn];
  }
  // Fill with spaces to right
  for (int i = T->currtty->scl_right - Pn + 1; i <= T->currtty->scl_right; i++)
    T->currtty->charbuf[y][i] = ' ';
}

void backspace(TERM *T){
// Move the cursor backwards one column
  if (is_in_margins(T)){
    if (T->currtty->cursor_x > T->currtty->scl_left)
      T->currtty->cursor_x--;
  } else {
    if (T->currtty->cursor_x > 1)
      T->currtty->cursor_x--;
  }
}

void next_line(TERM *T){
  carriage_return(T);
  line_feed(T);
}

void insert_line(TERM *T, int Pn){
  //Insert an empty line at active position
  int x = T->currtty->cursor_x; int y = T->currtty->cursor_y;
  if (((x < T->currtty->scl_left) || (x > T->currtty->scl_right)) ||
    ((y < T->currtty->scl_top) || (y > T->currtty->scl_bot))) return;
  if (y + Pn > T->currtty->scl_bot)
    Pn = T->currtty->scl_bot - y + 1;
  scroll_down(T, y, Pn);
  T->currtty->cursor_x = 1; // Reset position to left margin after
}

void delete_line(TERM *T, int Pn){
  // Delete line of characters at active position
  int x = T->currtty->cursor_x; int y = T->currtty->cursor_y;
  if (((x < T->currtty->scl_left) || (x > T->currtty->scl_right)) ||
    ((y < T->currtty->scl_top) || (y > T->currtty->scl_bot))) return;
  if (y + Pn > T->currtty->scl_bot)
    Pn = T->currtty->scl_bot - y + 1;
  scroll_up(T, y, Pn);
  T->currtty->cursor_x = 1; // Reset position to left margin after
}

// Screen controls
void set_col_mode(TERM *T, char col_mode){
// Change between 80 and 132 column mode
  // Setting mode clears screen, scroll area, sends cursor home
  T->col_mode = col_mode;
  if (col_mode == 80) T->currtty->cols = 80;
  if (col_mode == 132) T->currtty->cols = 132;
  // Clear screen
  for (int j = 0; j < max_rows; j++){
    for (int i = 0; i < max_cols; i++)
      T->currtty->charbuf[j][i] = ' ';
  }
  // Clear scroll area
  set_vert_scroll_margins(T, 1, T->currtty->rows);
  set_horiz_scroll_margins(T, 1, T->currtty->cols);
  // Send Cursor home
  T->currtty->cursor_x = 1;
  T->currtty->cursor_y = 1;
}

void set_screen_mode(TERM *T, char screen_mode){
// Set normal and reverse modes
  T->currtty->screen_mode = screen_mode;
}

void erase_in_line(TERM *T, int Pn){
  switch(Pn){
    case(0):{
      // Erase from active position to right margin
      for(int i = T->currtty->cursor_x; i <= T->currtty->cols; i++)
        T->currtty->charbuf[T->currtty->cursor_y][i] = ' ';
      break;
    }
    case(1):{
      // Erase from left margin to current position
      for(int i = 1; i <= T->currtty->cursor_x; i++)
        T->currtty->charbuf[T->currtty->cursor_y][i] = ' ';
      break;
    }
    case(2):{
      // Erase the active line completely
      for(int i = 1; i <= T->currtty->cols; i++)
        T->currtty->charbuf[T->currtty->cursor_y][i] = ' ';
      break;
    }
  }
}
		
void erase_in_display(TERM *T, int Pn){
  switch(Pn){
    case(0):{
      // Erase from active position to end of screen
      erase_in_line(T, 0);
      for (int j = T->currtty->cursor_y+1; j <= T->currtty->rows; j++){
        for (int i = 1; i <= T->currtty->cols; i++)
          T->currtty->charbuf[j][i] = ' ';
      }
      break;
    }
    case (1):{
      // Erase from beginning to active position
      for (int j = 1; j < T->currtty->cursor_y; j++){
        for (int i = 1; i <= T->currtty->cols; i++)
          T->currtty->charbuf[j][i] = ' ';
      }
      erase_in_line(T, 1);
      break;
    }
    case (2):{
      // Clear full screen
      for (int j = 1; j <= T->currtty->rows; j++){
        for (int i = 1; i <= T->currtty->cols; i++)
          T->currtty->charbuf[j][i] = ' ';
      }
      break;
    }
  }
}

void select_graphics_rendition(TERM *T, int *Pn){
  // Set attributes TODO: More than single set?
  T->currtty->graphic_mode = Pn[0];
  
}

void insert_character(TERM *T, char c){
// Normal character, insert at cursor
  int x = T->currtty->cursor_x; int y = T->currtty->cursor_y;
  // Margin is right scroll margin unless outside of scroll area
  int margin = T->currtty->cols;
  if (x < T->currtty->scl_right) margin = T->currtty->scl_right;

  // Replacing
  if (T->currtty->insert_replace_mode){
    T->currtty->charbuf[y][x] = c;
    T->currtty->revbuf[y][x] = (T->currtty->graphic_mode == 7);
    T->currtty->blinkbuf[y][x] = (T->currtty->graphic_mode == 5);
    if (x < margin){ // advance cursor if not at right margin/edge
      T->currtty->cursor_x++;
    } else {         // Only wrap if auto_wrap is enabled
      if (T->currtty->auto_wrap_mode){
        carriage_return(T);
        line_feed(T);
      }
    }
  }
}

int parse(TERM *T, char c){
// Read in characters, put normal chars on screen, else parse and execute an escape sequence
// Return 0 if no error
	if (c != 27){
  // Check for special graphics mode
    if (T->currtty->line_drawing){
      switch(c){
        case '_':c = ' '; break;
        case '`':c = 4; break;          // Diamond
        case 'a':c = 176;break;         //Checkerboard
        case 'b':tab(T); return;
        case 'c':line_feed(T);return;
        case 'd':carriage_return(T); return;
        case 'e':line_feed(T); return;
        case 'f':c = 248; break;        // Degree symbol
        case 'g':c = 241; break;        // Plus.minus
        case 'h':c = '\n'; break;
        case 'i':break; // TODO: Vertical tab??
        case 'j':c = 217; break;        // Lower-right corner
        case 'k':c = 191; break;        // Upper-right corner
        case 'l':c = 218; break;        // Upper-left corner
        case 'm':c = 192; break;        // Lower-left corner
        case 'n':c = 197; break;        // Crossing lines
        case 'q':c = 196; break;        // Horizontal line
        case 't':c = 195; break;        // Left 'T'
        case 'u':c = 180; break;        // Right 'T'
        case 'v':c = 193; break;        // Bottom 'T'
        case 'w':c = 194; break;        // Top 'T'
        case 'x':c = 179; break;        // Vertical Line
        case 'y':c = 243; break;        // <=
        case 'z':c = 242; break;        // >=
        case '{':c = 227; break;        // Pi
        case '|':c = 247; break;        // ~~
        case '}':c = 156; break;        // English Pound sign 
        case '~':c = 249; break;        // Centered dot
      }
    }
    switch(c) {
      case 8: case 127:
        backspace(T); break;
      case 10:
        carriage_return(T); break;
      case 13:
        line_feed(T); break;
      case 9:
        tab(T); break;
      default:
		    insert_character(T, c); break;
    }
		return 0; // Success
	} else {
		// Escape sequence
		char csi = 0; char scs = 0; char mode_set = 0;
		char code = 0;
		int Pn[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		int *ptr = Pn; int cnt = 0;
		c = Serial.read();
    switch(c){
      case '[':{
			  csi = 1; c = Serial.read();
        if (c == '?'){
          mode_set = 1; c - Serial.read();
        }
			  break;
      }
      // Set graphics mode, lazily done as only two charsets supported currently
      case '(':case ')': case '*': case '+':
        scs = 1; c = Serial.read(); break;
    }
		while (((('0' <= c) && (c <= '9')) || (c == ';')) && csi){
			if (c == ';'){
				// Read in another Pn, up to 16
				ptr++; cnt++;
				if (cnt >= 16) return 1; // Error
			} else {
				*ptr *= 10; *ptr += (c - '0');
			}
			c = Serial.read();
		}
		code = c;
    if (mode_set){
      switch(Pn[0]){
        case 3:{
          if (code == 'h') set_col_mode(T, 132);
          else if (code == 'l') set_col_mode(T, 80);
          break;
        }
        case 5:{
          if (code == 'h') set_screen_mode(T, 1);
          else if (code == 'l') set_screen_mode(T, 0);
          break;
        }
      }
      return 0;
    }
    // csi codes
    if (csi){
      switch(code){
        case 'H':{
          if (Pn[0] == 0){
            set_cursor(T, 1, 1); break; // Cursor Home
          } else {
            set_cursor(T, Pn[0], Pn[1]); break;
          }
        }
        case 'r':
          set_vert_scroll_margins(T, Pn[0], Pn[1]); break;
        case 's':
          set_horiz_scroll_margins(T, Pn[0], Pn[1]); break;
        case 'f':
          set_cursor(T, Pn[0], Pn[1]); break;
        case 'g':
          tab_clear(T, Pn[0]); break;
        case 'h':{
          switch(Pn[0]){
            case 4:
              T->currtty->insert_replace_mode = 1; break;
            case 5:
              T->currtty->auto_wrap_mode = 1; break;
          }
        }
        case 'l':{
          switch(Pn[0]){
            case 4: 
              T->currtty->insert_replace_mode = 0; break;
            case 5:
              T->currtty->auto_wrap_mode = 0; break;
          }
        }
        case 'R':
          report_cursor_position(T); break;
        case 'A':
          cursor_up(T, (Pn[0] == 0) ? 1 : Pn[0]);    break;
        case 'B':
          cursor_down(T, (Pn[0] == 0) ? 1 : Pn[0]);  break;
        case 'C':
          cursor_right(T, (Pn[0] == 0) ? 1 : Pn[0]); break;
        case 'D':
          cursor_left(T, (Pn[0] == 0) ? 1 : Pn[0]);  break;
        case 'J':
          erase_in_display(T, Pn[0]); break;
        case 'K':
          erase_in_line(T, Pn[0]); break;
        case 'm':
          select_graphics_rendition(T, Pn); break;
        case 'n':{
          // Report
          if (Pn[0] == 6) {report_cursor_position(T); break;}
          if (Pn[0] == 5) {report_terminal_ok(T); break;}
          break;
        }
        case 'P':
          delete_char(T, Pn[0]); break;
        default:
          break; //Ignore unknown codes
      }
      return 0;
    }
    if (scs){
      // Set character set, currently only two supported
      switch(code){
        case 'B':
          T->currtty->line_drawing = 0; break;
        case '0': 
          T->currtty->line_drawing = 1; break;
        default:
          break;
      }
      return 0;
    }
    if (!csi && !scs){
		  switch(code){
        case 'A':
          cursor_up(T, (Pn[0] == 0) ? 1 : Pn[0]);    break;
        case 'B':
          cursor_down(T, (Pn[0] == 0) ? 1 : Pn[0]);  break;
        case 'C':
          cursor_right(T, (Pn[0] == 0) ? 1 : Pn[0]); break;
        case 'D':
          cursor_down(T, (Pn[0] == 0) ? 1 : Pn[0]);  break;
        case 'E':
          next_line(T); break;
        case 'H':
          tab_set(T); break;
        case 'I':
          reverse_line_feed(T);
  			case 'M':
          cursor_up(T, (Pn[0] == 0) ? 1 : Pn[0]);  break;
        case '7':
          save_cursor(T); break;
        case '8':
          restore_cursor(T); break;
        default:
          break;
		  }
		}
	}
	return 0;
}
