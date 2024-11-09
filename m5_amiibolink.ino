/**
 * @file m5_amiibolink.ino
 * @author Rennan Cockles (r3ck.dev@gmail.com)
 * @brief M5 Amiibolink
 * @version 0.1
 * @date 2024-11-08
 *
 *
 * @Hardwares: M5 Cardputer and StickC
 * @Platform Version: Arduino M5Stack Board Manager v2.0.7
 */


// #define CARDPUTER

#ifdef CARDPUTER
  #include "M5Cardputer.h"
#else
  #include <M5Unified.h>
#endif
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>
#include <SPI.h>
#include <regex>
#include <amiibolink.h>

#define FGCOLOR 0x96FF
#define BGCOLOR 0
#ifdef CARDPUTER
  #define ROT 1
#else
  #define ROT 3
#endif
#define SMOOTH_FONT 1
#define FP 1
#define FM 2
#define FG 3
#define LW 6
#define LH 8
#define BORDER_PAD 5
#define REDRAW_DELAY 200
#define SEL_BTN 37
#define UP_BTN 35
#define DW_BTN 39
#define HEIGHT M5.Display.height()
#define WIDTH  M5.Display.width()
#define MAX_MENU_SIZE (int)(HEIGHT/25)
#define MAX_ITEMS (int)(HEIGHT-20)/(LH*2)

#ifdef CARDPUTER
	#define SDCARD_CS   12
	#define SDCARD_SCK  40
	#define SDCARD_MISO 39
	#define SDCARD_MOSI 14
#else
  #define SDCARD_CS   14
  #define SDCARD_SCK  0
  #define SDCARD_MISO 36
  #define SDCARD_MOSI 26
#endif

struct Option {
  std::string label;
  std::function<void()> operation;
  bool selected = false;

  Option(const std::string& lbl, const std::function<void()>& op, bool sel = false)
    : label(lbl), operation(op), selected(sel) {}
};
struct FileList {
  String filename;
  bool folder;
  bool operation;
};
bool sdcardMounted;
SPIClass sdcardSPI;
std::vector<FileList> fileList;


typedef struct {
  String uid;
  String bcc;
  String sak;
  String atqa;
  String picc_type;
} PrintableUID;

enum AppMode {
  START_MODE,
  AMIIBO_UPLOAD,
  CHANGE_UID_MODE,
};


Amiibolink amiibolink = Amiibolink(true);
AppMode currentMode;
PrintableUID printableUID;
String strDump = "";
bool amiibolinkConnected = false;


bool checkNextPress(){
  #ifdef CARDPUTER
    M5Cardputer.update();
    return (M5Cardputer.Keyboard.isKeyPressed('/') || M5Cardputer.Keyboard.isKeyPressed('.'));
  #endif
  return (digitalRead(DW_BTN) == LOW);
}

bool checkPrevPress() {
  #ifdef CARDPUTER
    M5Cardputer.update();
    return (M5Cardputer.Keyboard.isKeyPressed(',') || M5Cardputer.Keyboard.isKeyPressed(';'));
  #endif
  return (digitalRead(UP_BTN) == LOW);
}

bool checkSelPress(){
  #ifdef CARDPUTER
    M5Cardputer.update();
    return (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || digitalRead(0)==LOW);
  #endif
  return (digitalRead(SEL_BTN) == LOW);
}

bool checkEscPress(){
  #ifdef CARDPUTER
    M5Cardputer.update();
    return (M5Cardputer.Keyboard.isKeyPressed('`') || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE));
  #endif
  return (digitalRead(UP_BTN) == LOW);
}

bool checkAnyKeyPress() {
  #ifdef CARDPUTER
    M5Cardputer.update();
    return M5Cardputer.Keyboard.isPressed();
  #endif
  return (checkNextPress() || checkPrevPress() || checkSelPress());
}

#ifdef CARDPUTER
bool checkNextPagePress() {
  M5Cardputer.update();
  return M5Cardputer.Keyboard.isKeyPressed('/');
}

bool checkPrevPagePress() {
  M5Cardputer.update();
  return M5Cardputer.Keyboard.isKeyPressed(',');
}
#endif

void checkReboot() {
  int countDown;
  /* Long press power off */
  if (digitalRead(UP_BTN) == LOW) {
    uint32_t time_count = millis();
    while (digitalRead(UP_BTN) == LOW) {
      // Display poweroff bar only if holding button
      if (millis() - time_count > 500) {
        M5.Display.setCursor(60, 12);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        countDown = (millis() - time_count) / 1000 + 1;
        M5.Display.printf(" PWR OFF IN %d/3\n", countDown);
        delay(10);
      }
    }

    // Clear text after releasing the button
    delay(30);
    M5.Display.fillRect(60, 12, M5.Display.width() - 60, M5.Display.fontHeight(1), TFT_BLACK);
  }
}

void resetTftDisplay(int size = FM) {
  M5.Display.setCursor(0,0);
  M5.Display.fillScreen(BGCOLOR);
  M5.Display.setTextSize(size);
  M5.Display.setTextColor(FGCOLOR, BGCOLOR);
}

String keyboard(String mytext, int maxSize, String msg) {
  String _mytext = mytext;

  resetTftDisplay();
  bool caps=false;
  int x=0;
  int y=-1;
  int x2=0;
  int y2=0;
  char keys[4][12][2] = { //4 lines, with 12 characteres, low and high caps
    {
      { '1', '!' },//1
      { '2', '@' },//2
      { '3', '#' },//3
      { '4', '$' },//4
      { '5', '%' },//5
      { '6', '^' },//6
      { '7', '&' },//7
      { '8', '*' },//8
      { '9', '(' },//9
      { '0', ')' },//10
      { '-', '_' },//11
      { '=', '+' } //12
     },
    {
      { 'q', 'Q' },//1
      { 'w', 'W' },//2
      { 'e', 'E' },//3
      { 'r', 'R' },//4
      { 't', 'T' },//5
      { 'y', 'Y' },//6
      { 'u', 'U' },//7
      { 'i', 'I' },//8
      { 'o', 'O' },//9
      { 'p', 'P' },//10
      { '[', '{' },//11
      { ']', '}' } //12
    },
    {
      { 'a', 'A' },//1
      { 's', 'S' },//2
      { 'd', 'D' },//3
      { 'f', 'F' },//4
      { 'g', 'G' },//5
      { 'h', 'H' },//6
      { 'j', 'J' },//7
      { 'k', 'K' },//8
      { 'l', 'L' },//9
      { ';', ':' },//10
      { '"', '\'' },//11
      { '|', '\\' } //12
    },
    {
      { '\\', '|' },//1
      { 'z', 'Z' },//2
      { 'x', 'X' },//3
      { 'c', 'C' },//4
      { 'v', 'V' },//5
      { 'b', 'B' },//6
      { 'n', 'N' },//7
      { 'm', 'M' },//8
      { ',', '<' },//9
      { '.', '>' },//10
      { '?', '/' },//11
      { '/', '/' } //12
    }
  };
  int _x = WIDTH/12;
  int _y = (HEIGHT - 54)/4;
  int _xo = _x/2-3;
  int i=0;
  int j=-1;
  bool redraw=true;
  delay(200);
  int cX =0;
  int cY =0;
  M5.Display.fillScreen(BGCOLOR);
  while(1) {
    if(redraw) {
      M5.Display.setCursor(0,0);
      M5.Display.setTextColor(TFT_WHITE, BGCOLOR);
      M5.Display.setTextSize(FM);

      //Draw the rectangles
      if(y<0) {
        M5.Display.fillRect(0,1,WIDTH,22,BGCOLOR);
        M5.Display.drawRect(7,2,46,20,TFT_WHITE);       // Ok Rectangle
        M5.Display.drawRect(55,2,50,20,TFT_WHITE);      // CAP Rectangle
        M5.Display.drawRect(107,2,50,20,TFT_WHITE);     // DEL Rectangle
        M5.Display.drawRect(159,2,74,20,TFT_WHITE);     // SPACE Rectangle
        M5.Display.drawRect(3,32,WIDTH-3,20,FGCOLOR); // mystring Rectangle


        if(x==0 && y==-1) { M5.Display.setTextColor(BGCOLOR, TFT_WHITE); M5.Display.fillRect(7,2,50,20,TFT_WHITE); }
        else M5.Display.setTextColor(TFT_WHITE, BGCOLOR);
        M5.Display.drawString("OK", 18, 4);


        if(x==1 && y==-1) { M5.Display.setTextColor(BGCOLOR, TFT_WHITE); M5.Display.fillRect(55,2,50,20,TFT_WHITE); }
        else if(caps) { M5.Display.fillRect(55,2,50,20,TFT_DARKGREY); M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY); }
        else M5.Display.setTextColor(TFT_WHITE, BGCOLOR);
        M5.Display.drawString("CAP", 64, 4);


        if(x==2 && y==-1) { M5.Display.setTextColor(BGCOLOR, TFT_WHITE); M5.Display.fillRect(107,2,50,20,TFT_WHITE); }
        else M5.Display.setTextColor(TFT_WHITE, BGCOLOR);
        M5.Display.drawString("DEL", 115, 4);

        if(x>2 && y==-1) { M5.Display.setTextColor(BGCOLOR, TFT_WHITE); M5.Display.fillRect(159,2,74,20,TFT_WHITE); }
        else M5.Display.setTextColor(TFT_WHITE, BGCOLOR);
        M5.Display.drawString("SPACE", 168, 4);
      }

      M5.Display.setTextSize(FP);
      M5.Display.setTextColor(TFT_WHITE, 0x5AAB);
      M5.Display.drawString(msg.substring(0,38), 3, 24);

      M5.Display.setTextSize(FM);

      // reseta o quadrado do texto
      if (mytext.length() == 19 || mytext.length() == 20 || mytext.length() == 38 || mytext.length() == 39) M5.Display.fillRect(3,32,WIDTH-3,20,BGCOLOR); // mystring Rectangle
      // escreve o texto
      M5.Display.setTextColor(TFT_WHITE);
      if(mytext.length()>19) {
        M5.Display.setTextSize(FP);
        if(mytext.length()>38) {
          M5.Display.drawString(mytext.substring(0,38), 5, 34);
          M5.Display.drawString(mytext.substring(38,mytext.length()), 5, 42);
        }
        else {
          M5.Display.drawString(mytext, 5, 34);
        }
      } else {
        M5.Display.drawString(mytext, 5, 34);
      }
      //desenha o retangulo colorido
      M5.Display.drawRect(3,32,WIDTH-3,20,FGCOLOR); // mystring Rectangle


      M5.Display.setTextColor(TFT_WHITE, BGCOLOR);
      M5.Display.setTextSize(FM);


      for(i=0;i<4;i++) {
        for(j=0;j<12;j++) {
          //use last coordenate to paint only this letter
          if(x2==j && y2==i) { M5.Display.setTextColor(~BGCOLOR, BGCOLOR); M5.Display.fillRect(j*_x,i*_y+54,_x,_y,BGCOLOR);}
          /* If selected, change font color and draw Rectangle*/
          if(x==j && y==i) { M5.Display.setTextColor(BGCOLOR, ~BGCOLOR); M5.Display.fillRect(j*_x,i*_y+54,_x,_y,~BGCOLOR);}


          /* Print the letters */
          if(!caps) M5.Display.drawChar(keys[i][j][0], (j*_x+_xo), (i*_y+56));
          else M5.Display.drawChar(keys[i][j][1], (j*_x+_xo), (i*_y+56));

          /* Return colors to normal to print the other letters */
          if(x==j && y==i) { M5.Display.setTextColor(~BGCOLOR, BGCOLOR); }
        }
      }
      // save actual key coordenate
      x2=x;
      y2=y;
      redraw = false;
    }

    //cursor handler
    if(mytext.length()>19) {
      M5.Display.setTextSize(FP);
      if(mytext.length()>38) {
        cY=42;
        cX=5+(mytext.length()-38)*LW;
      }
      else {
        cY=34;
        cX=5+mytext.length()*LW;
      }
    } else {
      cY=34;
      cX=5+mytext.length()*LW*2;
    }

    /* When Select a key in keyboard */
#ifdef CARDPUTER
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isPressed()) {
      M5.Display.setCursor(cX,cY);
      auto status = M5Cardputer.Keyboard.keysState();

      bool Fn = status.fn;
      if(Fn && M5Cardputer.Keyboard.isKeyPressed('`')) {
        mytext = _mytext; // return the old name
        break;
      }

      for (auto i : status.word) {
        if(mytext.length()<maxSize) {
          mytext += i;
          if(mytext.length()!=20 && mytext.length()!=20) M5.Display.print(i);
          cX=M5.Display.getCursorX();
          cY=M5.Display.getCursorY();
          if(mytext.length()==20) redraw = true;
          if(mytext.length()==39) redraw = true;
        }
      }

      if (status.del && mytext.length() > 0) {
        // Handle backspace key
        mytext.remove(mytext.length() - 1);
        int fS=FM;
        if(mytext.length()>19) { M5.Display.setTextSize(FP); fS=FP; }
        else M5.Display.setTextSize(FM);
        M5.Display.setCursor((cX-fS*LW),cY);
        M5.Display.setTextColor(FGCOLOR,BGCOLOR);
        M5.Display.print(" ");
        M5.Display.setTextColor(TFT_WHITE, 0x5AAB);
        M5.Display.setCursor(cX-fS*LW,cY);
        cX=M5.Display.getCursorX();
        cY=M5.Display.getCursorY();
        if(mytext.length()==19) redraw = true;
        if(mytext.length()==38) redraw = true;
      }

      if (status.enter) {
        break;
      }
      delay(200);
    }

    if(checkSelPress()) break;

#else

    int z=0;

    if(checkSelPress())  {
      M5.Display.setCursor(cX,cY);
      if(caps) z=1;
      else z=0;
      if(x==0 && y==-1) break;
      else if(x==1 && y==-1) caps=!caps;
      else if(x==2 && y==-1 && mytext.length() > 0) {
        DEL:
        mytext.remove(mytext.length()-1);
        int fS=FM;
        if(mytext.length()>19) { M5.Display.setTextSize(FP); fS=FP; }
        else M5.Display.setTextSize(FM);
        M5.Display.setCursor((cX-fS*LW),cY);
        M5.Display.setTextColor(FGCOLOR,BGCOLOR);
        M5.Display.print(" ");
        M5.Display.setTextColor(TFT_WHITE, 0x5AAB);
        M5.Display.setCursor(cX-fS*LW,cY);
        cX=M5.Display.getCursorX();
        cY=M5.Display.getCursorY();
      }
      else if(x>2 && y==-1 && mytext.length()<maxSize) mytext += " ";
      else if(y>-1 && mytext.length()<maxSize) {
        ADD:
        mytext += keys[y][x][z];
        if(mytext.length()!=20 && mytext.length()!=20) M5.Display.print(keys[y][x][z]);
        cX=M5.Display.getCursorX();
        cY=M5.Display.getCursorY();
      }
      redraw = true;
      delay(200);
    }

    /* Down Btn to move in X axis (to the right) */
    if(checkNextPress()) {
      delay(200);
      if(checkNextPress()) { x--; delay(250); } // Long Press
      else x++; // Short Press
      if(y<0 && x>3) x=0;
      if(x>11) x=0;
      else if (x<0) x=11;
      redraw = true;
    }
    /* UP Btn to move in Y axis (Downwards) */
    if(checkPrevPress()) {
      delay(200);
      if(checkPrevPress()) { y--; delay(250);  }// Long press
      else y++; // short press
      if(y>3) { y=-1; }
      else if(y<-1) y=3;
      redraw = true;
    }

#endif

  }

  //Resets screen when finished writing
  M5.Display.fillRect(0,0,WIDTH,HEIGHT,BGCOLOR);
  resetTftDisplay();

  return mytext;
}


void printTitle(String title) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(FM);
  M5.Display.setTextDatum(top_center);

  M5.Display.drawString(title, M5.Display.width() / 2, BORDER_PAD);

  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(FP);
  M5.Display.setCursor(BORDER_PAD, 25);
}

void printSubtitle(String subtitle) {
  M5.Display.setTextSize(FP);
  M5.Display.setTextDatum(top_center);

  M5.Display.drawString(subtitle, M5.Display.width() / 2, 25);

  M5.Display.setTextDatum(top_left);
  M5.Display.setCursor(BORDER_PAD, 35);
}

void padprintln(const String &s) {
  M5.Display.setCursor(BORDER_PAD, M5.Display.getCursorY());
  M5.Display.println(s);
}


void displayRedStripe(String text, uint16_t fgcolor = TFT_WHITE, uint16_t bgcolor = TFT_RED) {
  int size;
  if(text.length()*LW*FM<(M5.Display.width()-2*FM*LW)) size = FM;
  else size = FP;
  M5.Display.fillSmoothRoundRect(10,M5.Display.height()/2-13,M5.Display.width()-20,26,7,bgcolor);
  M5.Display.fillSmoothRoundRect(10,M5.Display.height()/2-13,M5.Display.width()-20,26,7,bgcolor);
  M5.Display.setTextColor(fgcolor,bgcolor);
  if(size==FM) {
    M5.Display.setTextSize(FM);
    M5.Display.setCursor(M5.Display.width()/2 - FM*3*text.length(), M5.Display.height()/2-8);
  }
  else {
    M5.Display.setTextSize(FP);
    M5.Display.setCursor(M5.Display.width()/2 - FP*3*text.length(), M5.Display.height()/2-8);
  }
  M5.Display.println(text);
  M5.Display.setTextColor(FGCOLOR, TFT_BLACK);
}

void displayError(String txt, bool waitKeyPress = false)   {
  displayRedStripe(txt);
  delay(200);
  while(waitKeyPress && !checkAnyKeyPress()) delay(100);
}

void displayWarning(String txt, bool waitKeyPress = false) {
  displayRedStripe(txt, TFT_BLACK, TFT_YELLOW);
  delay(200);
  while(waitKeyPress && !checkAnyKeyPress()) delay(100);
}

void displayInfo(String txt, bool waitKeyPress = false)    {
  displayRedStripe(txt, TFT_WHITE, TFT_BLUE);
  delay(200);
  while(waitKeyPress && !checkAnyKeyPress()) delay(100);
}

void displaySuccess(String txt, bool waitKeyPress = false) {
  displayRedStripe(txt, TFT_WHITE, TFT_DARKGREEN);
  delay(200);
  while(waitKeyPress && !checkAnyKeyPress()) delay(100);
}

int loopOptions(std::vector<Option>& options, bool submenu = false, String subText = ""){
  bool redraw = true;
  int index=0;
  int menuSize = options.size();
  if(options.size()>MAX_MENU_SIZE) menuSize = MAX_MENU_SIZE;

  while(1){
    if (redraw) {
      if(submenu) drawSubmenu(index, options, subText);
      else drawOptions(index, options, FGCOLOR, BGCOLOR);
      redraw=false;
      delay(REDRAW_DELAY);
    }

    if(checkPrevPress()) {
    #ifdef CARDPUTER
      if(index==0) index = options.size() - 1;
      else if(index>0) index--;
      redraw = true;
    #else
    long _tmp=millis();
    // while(checkPrevPress()) { if(millis()-_tmp>200) M5.Display.drawArc(WIDTH/2, HEIGHT/2, 25,15,0,360*(millis()-(_tmp+200))/500,FGCOLOR-0x2000,BGCOLOR); }
    if(millis()-_tmp>700) { // longpress detected to exit
      break;
    }
    else {
      if(index==0) index = options.size() - 1;
      else if(index>0) index--;
      redraw = true;
    }
    #endif
    }
    /* DW Btn to next item */
    if(checkNextPress()) {
      index++;
      if((index+1)>options.size()) index = 0;
      redraw = true;
    }

    /* Select and run function */
    if(checkSelPress()) {
      Serial.println("Selected: " + String(options[index].label.c_str()));
      options[index].operation();
      break;
    }
  }
  delay(200);
  return index;
}

void drawOptions(int index,std::vector<Option>& options, uint16_t fgcolor, uint16_t bgcolor) {
  int menuSize = options.size();
  if(options.size()>MAX_MENU_SIZE) {
    menuSize = MAX_MENU_SIZE;
  }

  if(index==0) M5.Display.fillRoundRect(WIDTH*0.10,HEIGHT/2-menuSize*(FM*8+4)/2 -5,WIDTH*0.8,(FM*8+4)*menuSize+10,5,bgcolor);

  M5.Display.setTextColor(fgcolor,bgcolor);
  M5.Display.setTextSize(FM);
  M5.Display.setCursor(WIDTH*0.10+5,HEIGHT/2-menuSize*(FM*8+4)/2);

  int i=0;
  int init = 0;
  int cont = 1;
  if(index==0) M5.Display.fillRoundRect(WIDTH*0.10,HEIGHT/2-menuSize*(FM*8+4)/2 -5,WIDTH*0.8,(FM*8+4)*menuSize+10,5,bgcolor);
  menuSize = options.size();
  if(index>=MAX_MENU_SIZE) init=index-MAX_MENU_SIZE+1;
  for(i=0;i<menuSize;i++) {
    if(i>=init) {
      if(options[i].selected) M5.Display.setTextColor(fgcolor-0x2000,bgcolor); // if selected, change Text color
      else M5.Display.setTextColor(fgcolor,bgcolor);

      String text="";
      if(i==index) text+=">";
      else text +=" ";
      text += String(options[i].label.c_str()) + "              ";
      M5.Display.setCursor(WIDTH*0.10+5,M5.Display.getCursorY()+4);
      M5.Display.println(text.substring(0,(WIDTH*0.8 - 10)/(LW*FM) - 1));
      cont++;
    }
    if(cont>MAX_MENU_SIZE) goto Exit;
  }
  Exit:
  if(options.size()>MAX_MENU_SIZE) menuSize = MAX_MENU_SIZE;
  M5.Display.drawRoundRect(WIDTH*0.10,HEIGHT/2-menuSize*(FM*8+4)/2 -5,WIDTH*0.8,(FM*8+4)*menuSize+10,5,fgcolor);
}

void drawSubmenu(int index,std::vector<Option>& options, String system) {
  int menuSize = options.size();
  if(index==0) resetTftDisplay(FP);
  M5.Display.setTextColor(FGCOLOR,BGCOLOR);
  M5.Display.fillRect(6,26,WIDTH-12,20,BGCOLOR);
  M5.Display.fillRoundRect(6,26,WIDTH-12,HEIGHT-32,5,BGCOLOR);
  M5.Display.setTextSize(FP);
  M5.Display.setCursor(12,30);
  M5.Display.setTextColor(FGCOLOR);
  M5.Display.println(system);

  if (index-1>=0) {
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(FGCOLOR-0x2000);
    M5.Display.drawCentreString(options[index-1].label.c_str(),WIDTH/2, 42+(HEIGHT-134)/2,SMOOTH_FONT);
  } else {
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(FGCOLOR-0x2000);
    M5.Display.drawCentreString(options[menuSize-1].label.c_str(),WIDTH/2, 42+(HEIGHT-134)/2,SMOOTH_FONT);
  }

  int selectedTextSize = options[index].label.length() <= WIDTH/(LW*FG)-1 ? FG : FM;
  M5.Display.setTextSize(selectedTextSize);
  M5.Display.setTextColor(FGCOLOR);
  M5.Display.drawCentreString(
    options[index].label.c_str(),
    WIDTH/2,
    67+(HEIGHT-134)/2+((selectedTextSize-1)%2)*LH/2,
    SMOOTH_FONT
  );

  if (index+1<menuSize) {
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(FGCOLOR-0x2000);
    M5.Display.drawCentreString(options[index+1].label.c_str(),WIDTH/2, 102+(HEIGHT-134)/2,SMOOTH_FONT);
  } else {
    M5.Display.setTextSize(FM);
    M5.Display.setTextColor(FGCOLOR-0x2000);
    M5.Display.drawCentreString(options[0].label.c_str(),WIDTH/2, 102+(HEIGHT-134)/2,SMOOTH_FONT);
  }

  M5.Display.drawFastHLine(
    WIDTH/2 - options[index].label.size()*selectedTextSize*LW/2,
    67+(HEIGHT-134)/2+((selectedTextSize-1)%2)*LH/2+selectedTextSize*LH,
    options[index].label.size()*selectedTextSize*LW,
    FGCOLOR
  );
  M5.Display.fillRect(WIDTH-5,0,5,HEIGHT,BGCOLOR);
  M5.Display.fillRect(WIDTH-5,index*HEIGHT/menuSize,5,HEIGHT/menuSize,FGCOLOR);
}

void progressHandler(int progress, size_t total, String message) {
  int barWidth = map(progress, 0, total, 0, 200);
  if(barWidth <3) {
    M5.Display.fillRect(6, 27, WIDTH-12, HEIGHT-33, BGCOLOR);
    M5.Display.drawRect(18, HEIGHT - 47, 204, 17, FGCOLOR);
    displayRedStripe(message, TFT_WHITE, FGCOLOR);
  }
  M5.Display.fillRect(20, HEIGHT - 45, barWidth, 13, FGCOLOR);
}


bool setupSdCard() {
  if(SDCARD_SCK==-1) {
    sdcardMounted = false;
    return false;
  }

  // avoid unnecessary remounting
  if(sdcardMounted) return true;

  sdcardSPI.begin(SDCARD_SCK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS); // start SPI communications
  if (!SD.begin(SDCARD_CS, sdcardSPI)) {
    #if !defined(CARDPUTER)
      sdcardSPI.end(); // Closes SPI connections and release pin header.
    #endif
    sdcardMounted = false;
    return false;
  }
  else {
    sdcardMounted = true;
    return true;
  }
}

void closeSdCard() {
  SD.end();
  #if !defined(CARDPUTER)
  sdcardSPI.end(); // Closes SPI connections and release pins.
  #endif
  sdcardMounted = false;
}

bool sortList(const FileList& a, const FileList& b) {
    // Order items alfabetically
    String fa=a.filename.c_str();
    fa.toUpperCase();
    String fb=b.filename.c_str();
    fb.toUpperCase();
    return fa < fb;
}

bool checkExt(String ext, String pattern) {
    ext.toUpperCase();
    pattern.toUpperCase();
    if (ext == pattern) return true;

    pattern = "^(" + pattern + ")$";

    char charArray[pattern.length() + 1];
    pattern.toCharArray(charArray, pattern.length() + 1);
    std::regex ext_regex(charArray);
    return std::regex_search(ext.c_str(), ext_regex);
}

bool checkLittleFsSize() {
  if((LittleFS.totalBytes() - LittleFS.usedBytes()) < 4096) {
    displayError("LittleFS is Full", true);
    return false;
  } else return true;
}

bool getFsStorage(FS *&fs) {
  if(setupSdCard()) fs=&SD;
  else if(checkLittleFsSize()) fs=&LittleFS;
  else return false;

  return true;
}

void listFiles(int index, std::vector<FileList> fileList) {
    if(index==0){
      M5.Display.fillScreen(BGCOLOR);
      M5.Display.fillScreen(BGCOLOR);
    }
    M5.Display.setCursor(10,10);
    M5.Display.setTextSize(FM);
    int i=0;
    int arraySize = fileList.size();
    int start=0;
    if(index>=MAX_ITEMS) {
        start=index-MAX_ITEMS+1;
        if(start<0) start=0;
    }
    int nchars = (WIDTH-20)/(6*M5.Display.getTextStyle().size_x);
    String txt=">";
    while(i<arraySize) {
        if(i>=start) {
            M5.Display.setCursor(10,M5.Display.getCursorY());
            if(fileList[i].folder==true) M5.Display.setTextColor(FGCOLOR-0x2000, BGCOLOR);
            else if(fileList[i].operation==true) M5.Display.setTextColor(TFT_RED, BGCOLOR);
            else { M5.Display.setTextColor(FGCOLOR,BGCOLOR); }

            if (index==i) txt=">";
            else txt=" ";
            txt+=fileList[i].filename + "                 ";
            M5.Display.println(txt.substring(0,nchars));
        }
        i++;
        if (i==(start+MAX_ITEMS) || i==arraySize) break;
    }
    M5.Display.drawRoundRect(5, 5, WIDTH - 10, HEIGHT - 10, 5, FGCOLOR);
    M5.Display.drawRoundRect(5, 5, WIDTH - 10, HEIGHT - 10, 5, FGCOLOR);

}

void readFs(FS fs, String folder, String allowed_ext) {
    int allFilesCount = 0;
    fileList.clear();
    FileList object;

    File root = fs.open(folder);
    if (!root || !root.isDirectory()) {
        //Serial.println("Não foi possível abrir o diretório");
        return; // Retornar imediatamente se não for possível abrir o diretório
    }

    //Add Folders to the list
    File file = root.openNextFile();
    while (file && ESP.getFreeHeap()>1024) {
        String fileName = file.name();
        if (file.isDirectory()) {
            object.filename = fileName.substring(fileName.lastIndexOf("/") + 1);
            object.folder = true;
            object.operation=false;
            fileList.push_back(object);
        }
        file = root.openNextFile();
    }
    file.close();
    root.close();
    // Sort folders
    std::sort(fileList.begin(), fileList.end(), sortList);
    int new_sort_start=fileList.size();

    //Add files to the list
    root = fs.open(folder);
    File file2 = root.openNextFile();
    while (file2) {
        String fileName = file2.name();
        if (!file2.isDirectory()) {
            String ext = fileName.substring(fileName.lastIndexOf(".") + 1);
            if (allowed_ext=="*" || checkExt(ext, allowed_ext)) {
              object.filename = fileName.substring(fileName.lastIndexOf("/") + 1);
              object.folder = false;
              object.operation=false;
              fileList.push_back(object);
            }
        }
        file2 = root.openNextFile();
    }
    file2.close();
    root.close();

    //
    Serial.println("Files listed with: " + String(fileList.size()) + " files/folders found");

    // Order file list
    std::sort(fileList.begin()+new_sort_start, fileList.end(), sortList);

    // Adds Operational btn at the botton
    object.filename = "> Back";
    object.folder=false;
    object.operation=true;

    fileList.push_back(object);
}

String loopSD(FS &fs, String allowed_ext) {
  String result = "";
  std::vector<Option> options;
  bool reload=false;
  bool redraw = true;
  int index = 0;
  int maxFiles = 0;
  String Folder = "/";
  String PreFolder = "/";
  M5.Display.fillScreen(BGCOLOR);
  M5.Display.drawRoundRect(5,5,WIDTH-10,HEIGHT-10,5,FGCOLOR);
  if(&fs==&SD) {
    closeSdCard();
    if(!setupSdCard()){
      displayError("Fail Mounting SD", true);
      return "";
    }
  }
  bool exit = false;

  readFs(fs, Folder, allowed_ext);

  maxFiles = fileList.size() - 1; //discount the >back operator
  while(1){
    if(exit) break; // stop this loop and retur to the previous loop

    if(redraw) {
      if(strcmp(PreFolder.c_str(),Folder.c_str()) != 0 || reload){
        M5.Display.fillScreen(BGCOLOR);
        M5.Display.drawRoundRect(5,5,WIDTH-10,HEIGHT-10,5,FGCOLOR);
        index=0;
        Serial.println("reload to read: " + Folder);
        readFs(fs, Folder, allowed_ext);
        PreFolder = Folder;
        maxFiles = fileList.size()-1;
        reload=false;
      }
      if(fileList.size()<2) readFs(fs, Folder,allowed_ext);

      listFiles(index, fileList);
      delay(REDRAW_DELAY);
      redraw = false;
    }

    #ifdef CARDPUTER
      if(checkEscPress()) break;  // quit

      const short PAGE_JUMP_SIZE = 5;
      if(checkNextPagePress()) {
        index += PAGE_JUMP_SIZE;
        if(index>maxFiles) index=maxFiles-1; // check bounds
        redraw = true;
        continue;
      }
      if(checkPrevPagePress()) {
        index -= PAGE_JUMP_SIZE;
        if(index<0) index = 0;  // check bounds
        redraw = true;
        continue;
      }
    #endif

    if(checkPrevPress()) {
      if(index==0) index = maxFiles;
      else if(index>0) index--;
      redraw = true;
    }
    /* DW Btn to next item */
    if(checkNextPress()) {
      if(index==maxFiles) index = 0;
      else index++;
      redraw = true;
    }

    /* Select to install */
    if(checkSelPress()) {
      delay(200);

      if(fileList[index].folder==true && fileList[index].operation==false) {
        Folder = Folder + (Folder=="/"? "":"/") +  fileList[index].filename; //Folder=="/"? "":"/" +
        //Debug viewer
        Serial.println(Folder);
        redraw=true;
      }
      else if (fileList[index].folder==false && fileList[index].operation==false) {
        //Save the file/folder info to Clear memory to allow other functions to work better
        String filepath=Folder + (Folder=="/"? "":"/") +  fileList[index].filename; //
        String filename=fileList[index].filename;
        //Debug viewer
        Serial.println(filepath + " --> " + filename);
        fileList.clear(); // Clear memory to allow other functions to work better

        result = filepath;
        break;
      }
      else {
        if(Folder == "/") break;
        Folder = Folder.substring(0,Folder.lastIndexOf('/'));
        if(Folder=="") Folder = "/";
        Serial.println("Going to folder: " + Folder);
        index = 0;
        redraw=true;
      }
      redraw = true;
    }
  }

  fileList.clear();
  return result;
}


/////////////;;///////////
// Amiibolink Functions //
///////////////;;/////////

void displayBanner(AppMode mode = START_MODE) {
  printTitle("AMIIBOLINK");

  switch (mode) {
    case AMIIBO_UPLOAD:
      printSubtitle("AMIIBO UPLOAD");
      break;
    case CHANGE_UID_MODE:
      printSubtitle("SET UID MODE");
      break;
    default:
      printSubtitle("");
      break;
  }

  M5.Display.setTextSize(FP);
  padprintln("");
}

bool connect() {
  displayInfo("Turn on Amiibolink device", true);

  displayBanner();
  padprintln("");
  padprintln("Searching Amiibolink Device...");

  if (!amiibolink.searchDevice()) {
    displayError("Amiibolink not found");
    delay(1000);
    return false;
  }

  if (!amiibolink.connectToDevice()) {
    displayError("Amiibolink connect error");
    delay(1000);
    return false;
  }

  displaySuccess("Amiibolink Connected");
  delay(1000);

  return true;
}

void selectMode() {
  std::vector<Option> options = {
    {"Upload Amiibo",  [=]() { uploadAmiibo(); }},
    {"Set UID Mode",   [=]() { changeUIDMode(); }},
  };

  delay(200);
  loopOptions(options);

  amiibolink.disconnectFromDevice();
  amiibolinkConnected = false;
  displayBanner();
  delay(200);
}


bool openDumpFile() {
  String filepath;
  File file;
  FS *fs;

  if(!getFsStorage(fs)) {
    displayError("Storage error");
    delay(1000);
    return false;
  }

  filepath = loopSD(*fs, "RFID|NFC");
  file = fs->open(filepath, FILE_READ);

  if (!file) {
    displayError("Dump file error");
    delay(1000);
    return false;
  }

  String line;
  String strData;
  bool pageReadSuccess = true;
  strDump = "";

  while (file.available()) {
    line = file.readStringUntil('\n');
    strData = line.substring(line.indexOf(":") + 1);
    strData.trim();
    if(line.startsWith("Device type:"))  printableUID.picc_type = strData;
    if(line.startsWith("UID:"))          printableUID.uid = strData;
    if(line.startsWith("SAK:"))          printableUID.sak = strData;
    if(line.startsWith("ATQA:"))         printableUID.atqa = strData;
    if(line.startsWith("Pages read:"))   pageReadSuccess = false;
    if(line.startsWith("Page "))         strDump += strData;
  }

  file.close();
  delay(100);

  if (!pageReadSuccess) {
    displayError("Incomplete dump file");
    delay(1000);
    return false;
  }

  printableUID.uid.trim();
  printableUID.uid.replace(" ", "");
  printableUID.sak.trim();
  printableUID.sak.replace(" ", "");
  printableUID.atqa.trim();
  printableUID.atqa.replace(" ", "");
  strDump.trim();
  strDump.replace(" ", "");

  Serial.print("Uid: "); Serial.println(printableUID.uid);
  Serial.print("Sak: "); Serial.println(printableUID.sak);
  Serial.print("Data: "); Serial.println(strDump);
  Serial.print("Data len: "); Serial.println(strDump.length()/2);

  return true;
}

bool checkEmulationTagType() {
  byte sak = strtoul(printableUID.sak.c_str(), NULL, 16);
  int dataLen = strDump.length() / 2;

  if (sak != 0x00) return false;

  if (strDump.substring(0,8) == strDump.substring(strDump.length()-8)) {
    strDump = strDump.substring(0,strDump.length()-8);
  }

  if (strDump.length() / 2 != 540) return false;  // Not an NTAG_215

  return true;
}


void uploadAmiibo() {
  if (!openDumpFile()) return;

  if (!checkEmulationTagType()) {
    displayError("Invalid tag type");
    delay(1000);
    return;
  }

  displayBanner(AMIIBO_UPLOAD);
  displayInfo("Sending commands...");

  bool success = (
    amiibolink.cmdPreUploadDump()
    && amiibolink.cmdUploadDumpData(strDump)
    && amiibolink.cmdPostUploadDump()
  );

  if (success) {
    displaySuccess("Success");
  }
  else {
    displayError("Amiibolink communication error");
  }

  delay(1000);
}

void changeUIDMode() {
  Amiibolink::UIDMode uidMode;

  std::vector<Option> options = {
    {"Random Auto",   [&]() { uidMode = Amiibolink::UIDMode_Auto; }},
    {"Random Manual", [&]() { uidMode = Amiibolink::UIDMode_Manual; }},
  };
  delay(200);
  loopOptions(options);

  displayBanner(CHANGE_UID_MODE);

  if (amiibolink.cmdSetUIDMode(uidMode)) {
    displaySuccess("Success");
  }
  else {
    displayError("Amiibolink communication error");
  }

  delay(1000);
}



void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
#ifdef CARDPUTER
  M5Cardputer.begin(cfg, true);
#else
  M5.begin(cfg);
#endif

  M5.Display.setRotation(ROT);
  M5.Display.setTextColor(FGCOLOR, 0);

  if(!LittleFS.begin(true)) { LittleFS.format(), LittleFS.begin();}
  setupSdCard();

  displayBanner();

  delay(500);
}


void loop() {
  if (!amiibolinkConnected){
    if (!connect()) return;
    amiibolinkConnected = true;
    selectMode();
  }

  if (checkEscPress()) {
    checkReboot();
  }

}
