
#include <JeeLib.h>  //from jeelabs.org
#include <util/crc16.h>
#include <avr/eeprom.h>

// RF12 configuration setup code

typedef struct {
  byte nodeId;
  byte group;
  byte hasMail;
  word crc;
} 
RF12Config;

static RF12Config config;


static void saveConfig () {
  // save to EEPROM

  eeprom_write_byte(RF12_EEPROM_ADDR ,config.nodeId);
  eeprom_write_byte(RF12_EEPROM_ADDR +1 ,config.group);
  eeprom_write_byte(RF12_EEPROM_ADDR +2 ,config.hasMail);

  config.crc = ~0;
  config.crc = _crc16_update(config.crc, config.nodeId);     
  config.crc = _crc16_update(config.crc, config.group);        
  config.crc = _crc16_update(config.crc, config.hasMail);        

  for (int i=3; i < RF12_EEPROM_SIZE-2; i++) {
    eeprom_write_byte(RF12_EEPROM_ADDR + i, 0);
    config.crc = _crc16_update(config.crc, 0);        
  }

  eeprom_write_byte(RF12_EEPROM_ADDR + RF12_EEPROM_SIZE-2 ,config.crc);
  eeprom_write_byte(RF12_EEPROM_ADDR + RF12_EEPROM_SIZE-1 ,config.crc>>8);

  if (!rf12_config())
    Serial.println("config failed");

}



//#define COLLECT 0x20 // collect mode, i.e. pass incoming without sending acks


typedef struct { int hasMail, battery; } PayloadTX;      // create structure - a neat way of packaging data for RF comms
PayloadTX emontx; 

// constants won't change. They're used here to 
// set pin numbers:
const int buttonPin = 3;     // the number of the pushbutton pin
const int ledPin =  5;      // the number of the LED pin
const int pirPin = 7;    //the digital pin connected to the PIR sensor's output

//the time we give the sensor to calibrate (10-60 secs according to the datasheet)
int calibrationTime = 10;        

//the time when the sensor outputs a low impulse
long unsigned int lowIn;         

//the amount of milliseconds the sensor has to be low 
//before we assume all motion has stopped
long unsigned int pause = 5000;  

boolean lockLow = true;
boolean takeLowTime; 

// variables will change:
int buttonState = 0;         // variable for reading the pushbutton status

unsigned long firstTime, secondTime;   // how long since the button was first pressed 
int configurar=0;

static void emitData () {
  emontx.hasMail=config.hasMail;
  emontx.battery=100;
   
  int i = 0; 
  while (!rf12_canSend() && i<10) {
    rf12_recvDone(); i++;
  }
  rf12_sendStart(0, &emontx, sizeof emontx);
  Serial.println("Notify");
  Serial.print("  hasMail: "); Serial.println(emontx.hasMail);
  Serial.print("  battery: "); Serial.println(emontx.battery);
  Serial.println("  ");
   
  delay(2000);
}

static void showHelp () {
  //showString(helpText1);
  Serial.println("Current configuration:\n");
  config.nodeId = eeprom_read_byte(RF12_EEPROM_ADDR);
  config.group = eeprom_read_byte(RF12_EEPROM_ADDR + 1);
  config.hasMail = eeprom_read_byte(RF12_EEPROM_ADDR + 2);

  byte id = config.nodeId & 0x1F;
  Serial.print(" i");
  Serial.print( id,DEC);
  //if (config.nodeId & COLLECT)
   // Serial.print("*");

  Serial.print(" g");
  Serial.print(config.group,DEC);

  Serial.print(" @ 433");
  Serial.print(" MHz ");
  
  Serial.print(" hasMail ");
  Serial.print(config.hasMail, DEC);


  rf12_config();
}

void setup() {
  Serial.begin(9600);    // Use serial for debugging
  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);      
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
 
 if (rf12_config()) {
    config.nodeId = eeprom_read_byte(RF12_EEPROM_ADDR);
    config.group = eeprom_read_byte(RF12_EEPROM_ADDR + 1);
    config.hasMail = eeprom_read_byte(RF12_EEPROM_ADDR + 2);
  }else {
    config.nodeId = 0x81; // node 10
    config.group = 0xD2;  //210
    config.hasMail=0;   //No mail  
  rf12_initialize(config.nodeId&0x1F, RF12_433MHZ ,config.group);
    saveConfig();
  }
/*  
Serial.print(rf12_config());
Serial.print(" ------------ ");
Serial.println(config.nodeId&0x1F);
*/
  showHelp();
  delay(2000);

  //rf12_control(0xC049);   

 
 delay(1000); 
 
 pinMode(pirPin, INPUT);
 digitalWrite(pirPin, LOW);
 //give the sensor some time to calibrate
  Serial.print("calibrating sensor ");
    for(int i = 0; i < calibrationTime; i++){
      Serial.print(".");
      delay(1000);
      }
    Serial.println(" done");
    Serial.println("SENSOR ACTIVE");
    delay(50);
   
  
 Serial.println("Setup done!");
}

void loop(){
  
  configurar=0;

  // check if the pushbutton is pressed.
  // if it is, the buttonState is HIGH:
  firstTime = millis();
  //emitData();
  while (digitalRead(buttonPin) == HIGH) {
    if(config.hasMail){
      // remove email and notify the house
      config.hasMail = 0;
      saveConfig();
      emitData();
    }
    // turn LED on:    
    digitalWrite(ledPin, HIGH);
    if((firstTime+5000)<=millis()){
      
      configurar=1;
      break;
    }
    Serial.println(configurar);
    delay(500);
    digitalWrite(ledPin, LOW); 
    delay(500);
  } 
    
  if(configurar==1){
    digitalWrite(ledPin, HIGH);
    Serial.println("Entering setup mode");
    
    //delay(5000);
    //int i = 0;
    firstTime = millis();
    int configDone = 0;
    while(firstTime+60000 > millis() && configDone == 0){
      if (rf12_recvDone() ){
        if(rf12_crc == 0){
        byte n = rf12_len;
        
        char cmd = (char) rf12_data[n-1];
        
        Serial.print("--commnad: ");
        // we could use this if we want get the last char?
        //Serial.print((char) rf12_data[n-1]);
        switch (cmd){
          case 'i':
            Serial.print("changing id to ");
            Serial.println((int) rf12_data[0]);
            config.nodeId = (config.nodeId & 0xE0) + (rf12_data[0] & 0x1F);
            saveConfig();
            delay(500);
            configDone = 1;
            break;
          case 'g':
            Serial.print("changing group to ");
            Serial.println((int) rf12_data[0]);
            config.group = rf12_data[0];
            saveConfig();
            delay(500);
            configDone = 1;
            break;
          default:
            break;
        }
        
        }else{
          Serial.println("Bad CRC");
        }
      } 
    }
    digitalWrite(ledPin, LOW); 
    Serial.println("Exiting setup mode");
    showHelp();
    configurar=0;
  }
  if(config.hasMail == 0){
  //Sensor
   if(digitalRead(pirPin) == HIGH){
       //digitalWrite(ledPin, HIGH);   //the led visualizes the sensors output pin state
       if(lockLow){  
         //makes sure we wait for a transition to LOW before any further output is made:
         lockLow = false;            
         Serial.println("---");
         Serial.print("motion detected at ");
         Serial.print(millis()/1000);
         Serial.println(" sec"); 
         config.hasMail = 1;
         saveConfig();
         delay(50);
       }
       takeLowTime = true;
     }
  }
     if(digitalRead(pirPin) == LOW){       
       //digitalWrite(ledPin, LOW);  //the led visualizes the sensors output pin state

       if(takeLowTime){
        lowIn = millis();          //save the time of the transition from high to LOW
        takeLowTime = false;       //make sure this is only done at the start of a LOW phase
       }
       //if the sensor is low for more than the given pause, 
       //we assume that no more motion is going to happen
       if(!lockLow && millis() - lowIn > pause){  
           //makes sure this block of code is only executed again after 
           //a new motion sequence has been detected
           lockLow = true;                        
           Serial.print("motion ended at ");      //output
           Serial.print((millis() - pause)/1000);
           Serial.println(" sec");
           
           
         
           emitData();
       }
    }
  
  
}
