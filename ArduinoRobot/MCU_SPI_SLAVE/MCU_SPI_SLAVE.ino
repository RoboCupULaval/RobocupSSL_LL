
// Written by Philippe Babin 
// Based on a script by Nick Gammon
// Created : 20 November 2013


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

const int	ARDUINO_BAT_MONITOR_1       =      0xA1;
const int	ARDUINO_BAT_MONITOR_2       =      0xA2;
const int       ARDUINO_GPIO_2              =	   0xB2;
const int       ARDUINO_GPIO_3              =	   0xB3;
const int       ARDUINO_GPIO_4              =	   0xB4;
const int       ARDUINO_GPIO_5              =	   0xB5;
const int       ARDUINO_GPIO_6              =	   0xB6;
const int       ARDUINO_GPIO_7              =	   0xB7;
const int       ARDUINO_GPIO_8              =	   0xB8;
const int       ARDUINO_GPIO_9              =	   0xB9;
const int       ARDUINO_RECALL              =	   0xC0;

const int       ARDUINO_LOW                 =	   0;
const int       ARDUINO_HIGH                =	   1;
const int       ARDUINO_RECALL_MESSAGE      =	   0xEE;


const int greenPin = 8; 
const int redPin = 4; 
const int bluePin = 9; 
const int initialize_delay = 2000; 

byte reg;
byte value;
float batV7;
float batV16;
float batP7;
float batP16;

float MIN = 0;
float MAX = 4.0;

boolean gotAReg = false;
boolean gotAValue = false;

// Arduino Promini communication SPI:
/// 10 (SS), 11 (MOSI), 12 (MISO), 13 (SCK)

// INA219 I2C address:
/***
 * A1  |  A0  |  adresse | Hex
 * GND    GND    1000000   40
 * GND    Vs     1000001   41
 * GND    SDA    1000010   42
 * GND    SCL    1000011   43
 * Vs     GND    1000100   44
 * Vs     Vs     1000101   45
 */
Adafruit_INA219 monV7(0x45);
Adafruit_INA219 monV16(0x40);

void setup (void)
{
  Serial.begin (115200);   // Serial debugging

  // have to send on master in, *slave out*
  pinMode(MISO, OUTPUT);


 Serial.println("Séquence de démarrage");
  //initsequence(); //Sequence d'initialisation
  //delay(initialize_delay);
 
  // GPIO
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
  digitalWrite(7, HIGH);
  digitalWrite(8, LOW);
  digitalWrite(9, LOW);
  Serial.println("Relais déclenché");

  // turn on SPI in slave mode
  SPCR |= _BV(SPE);

  SPCR |= _BV(SPIE);

  // now turn on interrupts for Master request
  SPI.attachInterrupt();

  // We start the battery monitoring
  monV7.begin();
  monV16.begin();
}  

// Call for each Master request
ISR (SPI_STC_vect){
  // Master request (SPDR = Registre hardware SPI)
  byte c = SPDR;  
  // We check that this is not the start of a request
  if (!gotAValue){
    if(!gotAReg){
      reg = c;
      gotAReg = true;
    }
    else{
      value = c;
	
       // Register that change the GPIO state start by "0xBX"
      if(reg & 0xF0 == 0xB0){
	      if(reg == ARDUINO_GPIO_2)
		updateGPIO(2, value);
	      else if(reg == ARDUINO_GPIO_3)
		updateGPIO(3, value);
	      else if(reg == ARDUINO_GPIO_4)
		updateGPIO(4, value);
	      else if(reg == ARDUINO_GPIO_5)
		updateGPIO(5, value);
	      else if(reg == ARDUINO_GPIO_6)
		updateGPIO(6, value);
	      else if(reg == ARDUINO_GPIO_7)
		updateGPIO(7, value);
	      else if(reg == ARDUINO_GPIO_8)
		updateGPIO(8, value);
	      else if(reg == ARDUINO_GPIO_9)
		updateGPIO(9, value);
      }
      else if(reg == ARDUINO_BAT_MONITOR_1)
        SPDR = batV7;
      else if(reg == ARDUINO_RECALL)
          SPDR = ARDUINO_RECALL_MESSAGE;
      gotAValue = true;
    } 
    //SPDR = 0xE5;
  }
}  // end of interrupt routine SPI_STC_vect

void updateGPIO(int n, int state){
  if( state == ARDUINO_LOW)
    digitalWrite(n, LOW);
  else if( state == ARDUINO_HIGH)
    digitalWrite(n, HIGH);
  else
    Serial.println("Error");
}

void loop (void){
  // - 32768 a 32767
  // float current_mA = 0;
  //float loadvoltage = 0;
  //current_mA = ina219.getCurrent_mA();
  //batV7 = map(monV7.getBusVoltage_V() + (monV7.getShuntVoltage_mV() / 1000), MIN, MAX, 0,255);
  
  batV7 = monV7.getBusVoltage_V() + (monV7.getShuntVoltage_mV() / 1000);
  batV16 = monV16.getBusVoltage_V() + (monV16.getShuntVoltage_mV() / 1000);
  batP7 = batV7 * monV7.getCurrent_mA();
  batP16 = batV16 * monV16.getCurrent_mA();
  
  Serial.print(batV7);
  Serial.print(" ");
  Serial.println(batV16);
  if(batV7 > 6 && batV16 > 12 && batV16 < 30)//Vérifie que la batterie a une tension suppérieure à 12 V
    setColor(0, 170, 0); // Green
  else if(batV16 <= 12){  //Vérifie que la batterie a une tension suppérieure à 12 V
    setColor(170, 0, 0); // Red
    digitalWrite(7, LOW);
  }
  else
    setColor(170, 0, 0); // Red
    

/*
  Serial.print("BatV7: v=");
  Serial.print(batV7);
  Serial.print(" BatV16: v=");
  Serial.println(batV16);
  Serial.print("BatV7: p=");
  Serial.print(batP7);
  Serial.print(" BatV16: I=");
  Serial.print(monV16.getCurrent_mA());
  Serial.print(" BatV16: p=");
  Serial.println(batP16);
*/
  if (gotAValue && gotAReg){

    Serial.print(reg);
    Serial.print("-");
    if(reg == ARDUINO_GPIO_4){
      Serial.print("-");
    }
    Serial.println(value);
    reg = 0;
    value = 0;
    gotAReg = false;
    gotAValue = false;
  }  // end of flag set
  

}  // end of loop


//sequence d'initialisation (just for fun)
void initsequence()
{
  setColor(0, 0, 170); // blue
  delay(initialize_delay);

  setColor(0, 170, 0); // Green
  delay(initialize_delay);

  setColor(170, 0, 0); // Red
  delay(initialize_delay);    
}//----FIN--- de l'inialisation

//Fonction afin de faire allumer la led RGB
void setColor(int red, int green, int blue) 
{
  analogWrite(redPin, red); 
  analogWrite(greenPin, green); 
  analogWrite(bluePin, blue); 
}


