//Written By John Jobin McAuliffe, W1DRF, for the American Radio Relay League. Credit to OZ1JHM for the implementation of the Goetzel Algorithm used for decoding. 

#include <LCD-I2C.h>
#include <string.h>
#include <math.h>
#include <Arduino.h>
#include <HardwareSerial.h>
LCD_I2C lcd(0x27,20,4);
int cursor[2]= {0,0};
int cursor2[2]= {7,2};
byte U_umlaut[8] =   {B01010,B00000,B10001,B10001,B10001,B10001,B01110,B00000}; // 'Ü'  
byte O_umlaut[8] =   {B01010,B00000,B01110,B10001,B10001,B10001,B01110,B00000}; // 'Ö'  
byte A_umlaut[8] =   {B01010,B00000,B01110,B10001,B11111,B10001,B10001,B00000}; // 'Ä'    
byte AE_capital[8] = {B01111,B10100,B10100,B11110,B10100,B10100,B10111,B00000}; // 'Æ' 
byte OE_capital[8] = {B00001,B01110,B10011,B10101,B11001,B01110,B10000,B00000}; // 'Ø' 
byte fullblock[8] =  {B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111};  
byte AA_capital[8] = {B00100,B00000,B01110,B10001,B11111,B10001,B10001,B00000}; // 'Å'   
byte emtyblock[8] =  {B00000,B00000,B00000,B00000,B00000,B00000,B00000,B00000};  

int audioInPin = A1;
int keyspeedpin = A0;
//int audioOutPin = 1;
//int ledPin = 13;

float magnitude ;
int magnitudelimit = 100;
int magnitudelimit_low = 100;
int realstate = LOW;
int realstatebefore = LOW;
int filteredstate = LOW;
int filteredstatebefore = LOW;

float coeff;
float Q1 = 0;
float Q2 = 0;
float sine;
float cosine;  
float sampling_freq=8928.0;
float target_freq=558.0; /// adjust for your needs see above
float n=48.0;  //// if you change  her please change next line also 
int testData[48];

int nbtime = 6;  /// ms noise blanker         

long starttimehigh;
long highduration;
long lasthighduration;
long hightimesavg;
long lowtimesavg;
long startttimelow;
long lowduration;
long laststarttime = 0;

char code[20];
int stop = LOW;
int wpm;

String wordin;
int keySpeed;
void setup()
{
  int	k;
  float	omega;
  k = (int) (0.5 + ((n * target_freq) / sampling_freq));
  omega = (2.0 * PI * k) / n;
  sine = sin(omega);
  cosine = cos(omega);
  coeff = 2.0 * cosine;
  lcd.createChar(0, U_umlaut); //     German
  lcd.createChar(1, O_umlaut); //     German, Swedish
  lcd.createChar(2, A_umlaut); //     German, Swedish 
  lcd.createChar(3, AE_capital); //   Danish, Norwegian
  lcd.createChar(4, OE_capital); //   Danish, Norwegian
  lcd.createChar(5, fullblock);        
  lcd.createChar(6, AA_capital); //   Danish, Norwegian, Swedish
  lcd.createChar(7, emtyblock); 

  lcd.begin();
  lcd.display();
  lcd.backlight();
  lcd.clear();
  Serial.begin(115200);
  //pinMode(ledPin, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  pinMode(13, OUTPUT);
  
};
void loop() {

  for (char index = 0; index < n; index++){
    testData[index] = analogRead(audioInPin);
  }
  for (char index = 0; index < n; index++){
	  float Q0;
	  Q0 = coeff * Q1 - Q2 + (float) testData[index];
	  Q2 = Q1;
	  Q1 = Q0;	
  }
  float magnitudeSquared = (Q1*Q1)+(Q2*Q2)-Q1*Q2*coeff;  // we do only need the real part //
  magnitude = sqrt(magnitudeSquared);
  Q2 = 0;
  Q1 = 0;
  if (magnitude > magnitudelimit_low){
    magnitudelimit = (magnitudelimit +((magnitude - magnitudelimit)/6));  /// moving average filter
  }
 
  if (magnitudelimit < magnitudelimit_low) magnitudelimit = magnitudelimit_low;
  if(magnitude > magnitudelimit*0.6) realstate = HIGH; // just to have some space up  
  else realstate = LOW; 
   
  if (realstate != realstatebefore){
	laststarttime = millis();
  }
  if ((millis()-laststarttime)> nbtime){
	  if (realstate != filteredstate){
		filteredstate = realstate;
	  }
  }
 if (filteredstate != filteredstatebefore){
	if (filteredstate == HIGH){
		starttimehigh = millis();
		lowduration = (millis() - startttimelow);
	}

	if (filteredstate == LOW){
		startttimelow = millis();
		highduration = (millis() - starttimehigh);
        if (highduration < (2*hightimesavg) || hightimesavg == 0){
			hightimesavg = (highduration+hightimesavg+hightimesavg)/3;     // now we know avg dit time ( rolling 3 avg)
		}
		if (highduration > (5*hightimesavg) ){
			hightimesavg = highduration+hightimesavg;     // if speed decrease fast ..
		}
	}
  }
 
 if (filteredstate != filteredstatebefore){
  stop = LOW;
  if (filteredstate == LOW){  //// we did end a HIGH
   if (highduration < (hightimesavg*2) && highduration > (hightimesavg*0.6)){ /// 0.6 filter out false dits
	strcat(code,".");
	Serial.print(".");
   }
   if (highduration > (hightimesavg*2) && highduration < (hightimesavg*6)){ 
	strcat(code,"-");
	Serial.print("-");
	wpm = (wpm + (1200/((highduration)/3)))/2;  //// the most precise we can do ;o)
   }
  }
 
  if (filteredstate == HIGH){  //// we did end a LOW
   
    float lacktime = 1;
    if(wpm > 25)lacktime=1.0; ///  when high speeds we have to have a little more pause before new letter or new word 
    if(wpm > 30)lacktime=1.2;
    if(wpm > 35)lacktime=1.5;
   
    if (lowduration > (hightimesavg*(2*lacktime)) && lowduration < hightimesavg*(5*lacktime)){ // letter space
     docode();
	   code[0] = '\0';
	   Serial.print("/");
    }
    if (lowduration >= hightimesavg*(5*lacktime)){ // word space
     docode();
	   code[0] = '\0';
	   writeWord(wordin);
	   wordin="";
    }
  }
 }
  if ((millis() - startttimelow) > (highduration * 6) && stop == LOW){
   docode();
   code[0] = '\0';
   stop = HIGH;
  }
/*  if(filteredstate == HIGH){ 
    digitalWrite(ledPin, HIGH);
	  tone(audioOutPin,target_freq);
  }
  else{
    digitalWrite(ledPin, LOW);
	  noTone(audioOutPin);
  }  */

  realstatebefore = realstate;
  lasthighduration = highduration;
  filteredstatebefore = filteredstate;
  
  
  if (Serial.available()>0){
    writeWord(Serial.readString());
    
  }
  keySpeed=(analogRead(keyspeedpin)-20)/25;
  
  keySpeedDisp(keySpeed);
}
void docode(){
  if (strcmp(code,".-") == 0) wordin+='A';
	if (strcmp(code,"-...") == 0) wordin+='B';
	if (strcmp(code,"-.-.") == 0) wordin+='C';
	if (strcmp(code,"-..") == 0) wordin+='D';
	if (strcmp(code,".") == 0) wordin+='E';
	if (strcmp(code,"..-.") == 0) wordin+='F';
	if (strcmp(code,"--.") == 0) wordin+='G';
	if (strcmp(code,"....") == 0) wordin+='H';
	if (strcmp(code,"..") == 0) wordin+='I';
	if (strcmp(code,".---") == 0) wordin+='J';
	if (strcmp(code,"-.-") == 0) wordin+='K';
	if (strcmp(code,".-..") == 0) wordin+='L';
	if (strcmp(code,"--") == 0) wordin+='M';
	if (strcmp(code,"-.") == 0) wordin+='N';
	if (strcmp(code,"---") == 0) wordin+='O';
	if (strcmp(code,".--.") == 0) wordin+='P';
	if (strcmp(code,"--.-") == 0) wordin+='Q';
	if (strcmp(code,".-.") == 0) wordin+='R';
	if (strcmp(code,"...") == 0) wordin+='S';
	if (strcmp(code,"-") == 0) wordin+='T';
	if (strcmp(code,"..-") == 0) wordin+='U';
	if (strcmp(code,"...-") == 0) wordin+='V';
	if (strcmp(code,".--") == 0) wordin+='W';
	if (strcmp(code,"-..-") == 0) wordin+='K';
	if (strcmp(code,"-.--") == 0) wordin+='Y';
	if (strcmp(code,"--..") == 0) wordin+='Z';

	if (strcmp(code,".----") == 0) wordin+='1';
	if (strcmp(code,"..---") == 0) wordin+='2';
	if (strcmp(code,"...--") == 0) wordin+='3';
	if (strcmp(code,"....-") == 0) wordin+='4';
	if (strcmp(code,".....") == 0) wordin+='5';
	if (strcmp(code,"-....") == 0) wordin+='6';
	if (strcmp(code,"--...") == 0) wordin+='7';
	if (strcmp(code,"---..") == 0) wordin+='8';
	if (strcmp(code,"----.") == 0) wordin+='9';
	if (strcmp(code,"-----") == 0) wordin+='0';

	if (strcmp(code,"..--..") == 0) {
    wordin+='?';
    writeWord(wordin);
    wordin="";
  }
	if (strcmp(code,".-.-.-") == 0) {
    wordin+='.';
    writeWord(wordin);
    wordin="";
  }
	if (strcmp(code,"--..--") == 0) {
    wordin+=',';
    writeWord(wordin);
    wordin="";
  }
	if (strcmp(code,"-.-.--") == 0) {
    wordin+='!';
    writeWord(wordin);
    wordin="";
  }
	if (strcmp(code,".--.-.") == 0) {
    wordin+='@';
    writeWord(wordin);
    wordin="";
  }
	if (strcmp(code,"---...") == 0) {
    wordin+=':';
    writeWord(wordin);
    wordin="";
  }
	if (strcmp(code,"-....-") == 0) {
    wordin+='-';
  }
	if (strcmp(code,"-..-.") == 0) wordin+='/';

	if (strcmp(code,"-.--.") == 0) wordin+='(';
	if (strcmp(code,"-.--.-") == 0) {
    wordin+=')';
    writeWord(wordin);
    wordin="";
  }
	if (strcmp(code,".-...") == 0) wordin+='_';
	if (strcmp(code,"...-..-") == 0) wordin+='$';
	if (strcmp(code,"...-.-") == 0) wordin+='>';
	if (strcmp(code,".-.-.") == 0) wordin+='<';
	if (strcmp(code,"...-.") == 0) wordin+='~';
	//////////////////
	// The specials //
	//////////////////
	if (strcmp(code,".-.-") == 0) {
    writeWord(wordin);
    lcd.setCursor(cursor[0]-1, cursor[1]);
    lcd.write(3);
    wordin="";
  }
	if (strcmp(code,"---.") == 0)   {
    writeWord(wordin);
    lcd.setCursor(cursor[0]-1, cursor[1]);
    lcd.write(4);
    wordin="";
  }
	if (strcmp(code,".--.-") == 0)   {
    writeWord(wordin);
    lcd.setCursor(cursor[0]-1, cursor[1]);
    lcd.write(6);
    wordin="";
  }

}
void keySpeedDisp(int keyspeed){
  lcd.setCursor(0,2);
  lcd.print(String(keyspeed)+" WPM ");
}
void displayMSG(char word){
  if(1+cursor2[0]<=20){
    lcd.setCursor(cursor2[0],cursor2[1]);
    lcd.print(word);
    cursor2[0]=cursor2[0]+1;
  } else if(cursor2[1]==2) {
    cursor2[0]=0;
    cursor2[1]=3;
    lcd.setCursor(cursor2[0],cursor2[1]);
    lcd.print("                    ");
    lcd.setCursor(cursor2[0],cursor2[1]);
    lcd.print(word);
    cursor2[0]=cursor2[0]+1;
  } else if (cursor2[1]==3){
    cursor2[0]=7;
    cursor2[1]=2;
    lcd.setCursor(cursor2[0],cursor2[1]);
    lcd.print("             ");
    lcd.setCursor(cursor2[0],cursor2[1]);
    lcd.print(word);
    cursor2[0]=cursor2[0]+1;
  }
}
void writeWord(String word){
  if(word.length()+cursor[0]<=20){
    lcd.setCursor(cursor[0],cursor[1]);
    lcd.print(word);
    lcd.setCursor(cursor[0]+word.length()-1, cursor[1]);
    lcd.print(" ");
    cursor[0]=cursor[0]+word.length();
  } else if(cursor[1]==0) {
    cursor[0]=0;
    cursor[1]=1;
    lcd.setCursor(cursor[0],cursor[1]);
    lcd.print("                    ");
    lcd.setCursor(cursor[0],cursor[1]);
    lcd.print(word);
    lcd.setCursor(cursor[0]+word.length()-1, cursor[1]);
    lcd.print(" ");
    cursor[0]=cursor[0]+word.length();
  } else if (cursor[1]==1){
    cursor[0]=0;
    cursor[1]=0;
    lcd.setCursor(cursor[0],cursor[1]);
    lcd.print("                    ");
    lcd.setCursor(cursor[0],cursor[1]);
    lcd.print(word);
    lcd.setCursor(cursor[0]+word.length()-1, cursor[1]);
    lcd.print(" ");
    cursor[0]=cursor[0]+word.length();
  }
}
