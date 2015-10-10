#include <OneWire.h>
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>
#define StartConvert 0
#define ReadTemperature 1

const byte SR_EEPROM_FLAG_POS = 0;
const byte CONF_EEPROM_FLAG_POS = 2;

const float FACTORY_COND_M = 35.813; //factory default gradient from calibration
const float FACTORY_COND_C = 148.47; //factory default y intercept from calibration

float m_conductivity = FACTORY_COND_M; //gradient from calibration
float c_conductivity = FACTORY_COND_C; //y intercept from calibration


byte ECsensorPin = A1;  //EC Meter analog output,pin on analog 1
byte DS18B20_Pin = 2;  //DS18B20 signal (temp sensor), pin on digital 2
byte nMOS_Pin = 5; // FET for disconnecting sensors in between samples
const int chipSelect = 4; // for SD
const byte numReadings = 0;
byte index = 0; // pos of current reading in array
unsigned int readings[numReadings]; 
unsigned long analogSampleTime, tempSampleTime;

//int fileIndex = 1;
int hasData = 0; // flag to set when first writing data to SD
int cmd = 0;
int sampleNumber = 1;
String device_id = "fluid_sol_01";
unsigned int analogSampleInterval=25,tempSampleInterval=850; 

unsigned long lastTime = 0;
unsigned long analogValTotal = 0; // for averaging conductivity
unsigned int analogAv = 0, avVoltage=0;
float temperature, conductivity;
//Temperature chip i/o
OneWire ds(DS18B20_Pin);

void setup(){
  Serial.begin(115200);
  pinMode(10, OUTPUT); // need this to be set to output for SD module
  pinMode(nMOS_Pin, OUTPUT);
  digitalWrite(nMOS_Pin, LOW);
  for(byte thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0;
  tempProcess(StartConvert);
  analogSampleTime=millis();
  tempSampleTime=millis();
  // see if the card is present and can be initialised
  if (SD.begin(chipSelect)){
    Serial.println("SD initialised");
  }
  else{
    Serial.println("SD initialisation failed");
  }
}

void loop(){
  //if phone is paired over bluetooth, get command from App
  if(Serial.available()){
    readSerial();
  }
  //if 20 minutes has elapsed since last sample, take a new one
  if(millis() - lastTime > 30000){
    digitalWrite(nMOS_Pin, HIGH); // turn on sensors
    String newSample = takeSample();
    digitalWrite(nMOS_Pin, LOW); // turn off sensors
    saveToSD(newSample); // store sample on SD card
    lastTime = millis();
  }
}

void readSerial(){
  byte b[1];
  int i = 0;
  unsigned long startTime = millis();
  while(Serial.available() > 0){
     
     if(i==0){
            b[i] =  Serial.read(); // Read char
            if(b[0]==99) {  //recalibration
                 float measured = (float)Serial.parseInt();
                 float solution = (float)Serial.parseInt();
                 Serial.println(measured);
                 delay(200);
                 Serial.println(solution);
                 byte buf[1];
                 buf[0] = recalibrate(measured,solution);
                 Serial.write(buf,sizeof(buf));
            }
            cmd += b[i];
            i++;
      }else{
       cmd += Serial.read(); // Read char, add to ASCII sum
      }
    // if greater than 10 seconds have passed, break out (timeout)
    if((millis() - startTime) > 10000){
      break;
    }
  }
  
  if(cmd == 416){// Test
    //take single sample, return it
    sendSingleSample();
    cmd = 0;
  }
  else if(cmd == 1216){// RetrieveData
    sendAllSamples();
    cmd = 0;
  }
  else if(cmd == 644){// Status
    if(hasData == 1){
      Serial.println("{\"status\":\"ready\"}");
    }
    else{
      Serial.println("{\"status\":\"nodata\"}");
    }
    cmd = 0;
  }
  else if (b[0]==109){
  //measure
   int condReading = measureConductivity();
   byte buffer[3] = {0xAD, 
                    (byte)condReading,
                    (byte)(condReading >> 8)
                   };
   Serial.write(buffer, sizeof(buffer));
  }
  else if(b[0]==114){ //resetCalibration to factory default
 Serial.print("Reset");
  byte buf[1];
    buf[0] = resetConductivity();
    Serial.write(buf,sizeof(buf));
  }
}
void saveToSD(String sample){
  File dataFile = SD.open("data.txt", FILE_WRITE);
 
  if (dataFile){
    if(hasData == 0){ hasData = 1;} 
    Serial.println("saving to data.txt");
    dataFile.println(sample); // write sample to file
    dataFile.close();
  }  
  else {
    // if the file isn't open, print error
    Serial.println("error opening data.txt");
  } 
}

void sendSingleSample(){
    Serial.println("[databegin]");
    Serial.println("{\"status\":\"ready\"}");

    digitalWrite(nMOS_Pin, HIGH); 
    String newSample = takeSample();
    digitalWrite(nMOS_Pin, LOW);
    Serial.println(newSample);
    Serial.println("[dataend]");
}

void sendAllSamples(){

  String currSample;
  int count = 0;
  File dataFile = SD.open("data.txt");
  
  if (dataFile){ // file opened successfully
    if(hasData == 1){ // the file has had sample(s) written to it
      Serial.println("[databegin] {");
      Serial.println("\"Status\": \"ready\",");
      Serial.println("\"samples\": [");
      // read all samples off File
      while(dataFile.available()){
        
        char c = dataFile.read();
        if(c != 125){ // 125 = }
          currSample+= c;
        }
        else{
          currSample += "}";
          Serial.println(currSample);
          delay(200); // delay to avoid phone JSON buffer overflow
          currSample= ""; //reset temporary string for next sample
          count++;
        }
      }
      dataFile.close(); // close the file:

      String endTime = "\n\"TimeSinceLast\": ";
      endTime += (String)(ms_to_min(millis()-lastTime));
      Serial.println(endTime);
      Serial.println("} [dataend]");
    }
    else{Serial.println("\"Error\": \"nodata\"");} //file hasn't had samples written to it
    
  } 
  else{
    // if the file didn't open, print an error:
    Serial.println("\"Error\": \"could not read SD\"");
  }
}

String takeSample(){
  //these timing constraints should always be true since we're waiting 20 mins,
  // but keep them in anyway for the time being
  if(millis()-analogSampleTime >= analogSampleInterval)  {
    analogSampleTime=millis();
     // subtract the last reading:
    analogValTotal = analogValTotal - readings[index];
    // read from the sensor:
    readings[index] = analogRead(ECsensorPin);
    // add the reading to the total:
    analogValTotal = analogValTotal + readings[index];
    // advance to the next position in the array:
    index = index + 1;
    // at end of the array
    if (index >= numReadings)
    // ...wrap around to the beginning:
    index = 0;
    // calculate the average:
    analogAv = analogValTotal/numReadings;
  }

  if(millis()-tempSampleTime >= tempSampleInterval){
    tempSampleTime=millis();
    temperature = tempProcess(ReadTemperature);  // read the current temperature from the  DS18B20
    tempProcess(StartConvert);                   //after the reading,start the convert for next reading
  }
  avVoltage=analogAv*(float)5000/1024;
  //linear function obtained from measurements, valid from ~200uS/cm to 1.4mS/cm
  conductivity = m_conductivity*avVoltage+c_conductivity; 
  unsigned long timeSinceLast = ms_to_min(millis()- lastTime);

  return serialiseToJson(temperature, conductivity, timeSinceLast);
}

String serialiseToJson(float temp, float cond, unsigned long timeSinceLast){
  // construct JSON obj for individual sample
  String sample;
  sample += "{\"DeviceID\":\"";
  sample += device_id;

  sample += "\",\n\"SampleID\":";
  sample += (String)sampleNumber;

  sample += ",\n\"TimeSinceLast\":";
  sample += (String)timeSinceLast;

  sample += ",\n\"Temperature\":";
  sample += (String)temp;

  sample += ",\n\"Conductivity\":";
  sample += (String)cond;
  sample+="\n}";

  lastTime = millis();
  sampleNumber++;

  return sample;
}

unsigned long ms_to_min(unsigned long ms){
  return ms/60000;
}

float tempProcess(bool ch){ //returns temperature in degrees C
  static byte data[12];
  static byte addr[8];
  static float tempSum;
  if(!ch){
    if ( !ds.search(addr)) {
      Serial.println("no more sensors on chain, reset search");
      ds.reset_search();
      return 0;
    }      
    if ( OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC not valid");
      return 0;
    }        
    if ( addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.print("Device not recognized");
      return 0;
    }      
    ds.reset();
    ds.select(addr);
    ds.write(0x44,1); // start conversion, with parasite power on at the end
  }
  else{  
    byte present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE); // Read Scratchpad            
    for (int i = 0; i < 9; i++) { // need 9 bytes
      data[i] = ds.read();
    }         
    ds.reset_search();           
    byte MSB = data[1];
    byte LSB = data[0];        
    float tempRead = ((MSB << 8) | LSB); //using twos complement
    tempSum = tempRead / 16;
  }
  return tempSum;
}


//---------------------------------------------------------------------------------

byte recalibrate(float measured, float solution){
float factor = 0;
factor =((float)(measured)/((float)solution));
//Serial.print(factor);
m_conductivity = m_conductivity*factor;
c_conductivity = c_conductivity*factor;
return (byte)0xAE;
}
byte resetConductivity(){ //reset to factory default

  m_conductivity = FACTORY_COND_M;
  c_conductivity = FACTORY_COND_C;
  return (byte)0xAF;
}
//not touching the wqmd code so just cut and paste temporarily
int measureConductivity(){
  //these timing constraints should always be true since we're waiting 20 mins,
  // but keep them in anyway for the time being
  if(millis()-analogSampleTime >= analogSampleInterval)  {
    analogSampleTime=millis();
     // subtract the last reading:
    analogValTotal = analogValTotal - readings[index];
    // read from the sensor:
    readings[index] = analogRead(ECsensorPin);
    // add the reading to the total:
    analogValTotal = analogValTotal + readings[index];
    // advance to the next position in the array:
    index = index + 1;
    // at end of the array
    if (index >= numReadings)
    // ...wrap around to the beginning:
    index = 0;
    // calculate the average:
    analogAv = analogValTotal/numReadings;
  }

  if(millis()-tempSampleTime >= tempSampleInterval){
    tempSampleTime=millis();
    temperature = tempProcess(ReadTemperature);  // read the current temperature from the  DS18B20
    tempProcess(StartConvert);                   //after the reading,start the convert for next reading
  }
  avVoltage=analogAv*(float)5000/1024;
  //linear function obtained from measurements, valid from ~200uS/cm to 1.4mS/cm
  conductivity = m_conductivity*avVoltage+c_conductivity; 

return (int) conductivity;
}

//----------------EEPROM access - should put in library-----------------------------


//sampling rate flag set to 0 or 1. If 1 then an updated sample rate exists.
 byte eepromReadSRFlag(){
   return EEPROM.read(SR_EEPROM_FLAG_POS);
}


//sampling rate flag set to 0 or 1. If 1 then an updated sample rate exists.
 void eepromWriteSRFlag(byte flag){
    EEPROM.write(SR_EEPROM_FLAG_POS,flag);
}

//configuration flag. If set to 1 config m and c need to be updated
 byte eepromReadCFlag(){
   return EEPROM.read(SR_EEPROM_FLAG_POS);
}

//configuration flag. If set to 1 config m and c need to be updated
 void eepromWriteCFlag(byte flag){

   EEPROM.write(CONF_EEPROM_FLAG_POS,flag);
}


/*
 * Functions to allow data measurements and other variables to be written and read from EEPROM
 */

float eepromReadFloat(int address){
   union u_tag {
     byte b[4];
     float fval;
   } u;   
   u.b[0] = EEPROM.read(address);
   u.b[1] = EEPROM.read(address+1);
   u.b[2] = EEPROM.read(address+2);
   u.b[3] = EEPROM.read(address+3);
   return u.fval;
}
 
void eepromWriteFloat(int address, float value){
   union u_tag {
     byte b[4];
     float fval;
   } u;
   u.fval=value;
 
   EEPROM.write(address  , u.b[0]);
   EEPROM.write(address+1, u.b[1]);
   EEPROM.write(address+2, u.b[2]);
   EEPROM.write(address+3, u.b[3]);
}

 
void eepromWriteInt(int address, int value){
   union u_tag {
     byte b[2];
     int ival;
   } u;
   u.ival=value;
 
   EEPROM.write(address  , u.b[0]);
   EEPROM.write(address+1, u.b[1]);
 
}

 long eepromReadLong(int address){
   union u_tag {
     byte b[4];
     long lval;
   } u;   
   u.b[0] = EEPROM.read(address);
   u.b[1] = EEPROM.read(address+1);
   u.b[2] = EEPROM.read(address+2);
   u.b[3] = EEPROM.read(address+3);
   return u.lval;
}
 
void eepromWriteLong(int address, long value){
   union u_tag {
     byte b[4];
     long lval;
   } u;
   u.lval=value;
 
   EEPROM.write(address  , u.b[0]);
   EEPROM.write(address+1, u.b[1]);
   EEPROM.write(address+2, u.b[2]);
   EEPROM.write(address+3, u.b[3]);
}

void saveToEEPROM(float conductivity, float temp,unsigned long tsl ){

int eIndex=0; 
  eepromWriteFloat(eIndex, conductivity);
  eIndex=+4;
  eepromWriteFloat(eIndex, temp);
  eIndex=+4;
  eepromWriteLong(eIndex,tsl);
  eIndex=+4;
  unsigned long tmp1 = eepromReadLong(eIndex-4);
  Serial.println(tmp1);
  delay(1000);

}


