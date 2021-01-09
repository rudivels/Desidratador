// Desidratador de laboratorio com medição de peso, temperatura e humidade.
// Monitoramento via MODBUS-IP em página web
// 
//  06/12/2020 Primeira Versao com medicao de peso, temperatura testando o ESP12
//             usando HX711, testando com ScadaBR na bancada local 
//             e sensor de temperatura  onewire
// 28/12/2020  segunda versão com celula de carga montado no forno da Britania
//             calibrando com suporte de bandeja em aluminio branco pendurado 
//             nos resistores superiores.
//             Precisa ainda verificar sentido, pois sinal é invertida.
//             Configuracao do IDE Arduino Placa NodeMCU 0.9 (ESP-12 Module)
//             Porta cu.SLAB_USBtoUART
// 29/12/2020  Preparando para usar display TM1637 para depuração e operacao
//             simplificado. OK
// 30/12/2020  Usando o AM2302 Temperatura e humidade como dht2
//             Usando o DH11   Temperatura e humidade como dht1
//  
//  
//  Baseado em Exemplo de https://domoticx.com/esp8266-wifi-modbus-tcp-ip-slave/
//  Hardware ESP12F 
//             
//  Hardware 
//  D2  CLK TM1637
//  D3  DIO TM1637
//  D4  DHTPIN11 DHT11  // ONeWire Temperatura
//  D5  DOUT HX711
//  D6  SCK  HX711
//  D7  DHTPIN AM2302 

#include "TM1637.h"
#define CLK D2//pins definitions for TM1637 and can be changed to other ports
#define DIO D3
TM1637 tm1637(CLK,DIO);


#include <ESP8266WiFi.h>
#include "acesso_wifi.h"  // define ssid & password

int ModbusTCP_port = 502;

#define TIMER_INTERRUPT_DEBUG      1
#include "ESP8266TimerInterrupt.h"
#define TIMER_INTERVAL_MS  1000
ESP8266Timer ITimer;

/* Celula de carga */
#include "HX711.h"
const int LOADCELL_DOUT_PIN = D5;
const int LOADCELL_SCK_PIN  = D6;
HX711 scale;
float gramas;

/* Temperatura */
/* 
 *  
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS D4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float Temperatura; */

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
uint32_t delayMS;
#define DHTPIN2 D7 
#define DHTTYPE2    DHT22     // DHT 22 (AM2302)
DHT_Unified dht2(DHTPIN2, DHTTYPE2);

#define DHTPIN1 D4 
#define DHTTYPE1    DHT11     // DHT 11
DHT_Unified dht1(DHTPIN1, DHTTYPE1);

//////// Required for Modbus TCP / IP /// Requerido para Modbus TCP/IP /////////
#define maxInputRegister 20
#define maxHoldingRegister 20
 
#define MB_FC_NONE 0
#define MB_FC_READ_REGISTERS 3 //implemented
#define MB_FC_WRITE_REGISTER 6 //implemented
#define MB_FC_WRITE_MULTIPLE_REGISTERS 16 //implemented
//
// MODBUS Error Codes
//
#define MB_EC_NONE 0
#define MB_EC_ILLEGAL_FUNCTION 1
#define MB_EC_ILLEGAL_DATA_ADDRESS 2
#define MB_EC_ILLEGAL_DATA_VALUE 3
#define MB_EC_SLAVE_DEVICE_FAILURE 4
//
// MODBUS MBAP offsets
//
#define MB_TCP_TID 0
#define MB_TCP_PID 2
#define MB_TCP_LEN 4
#define MB_TCP_UID 6
#define MB_TCP_FUNC 7
#define MB_TCP_REGISTER_START 8
#define MB_TCP_REGISTER_NUMBER 10
 
byte ByteArray[260];
unsigned int MBHoldingRegister[maxHoldingRegister];
 
//////////////////////////////////////////////////////////////////////////

WiFiServer MBServer(ModbusTCP_port);

void display_peso(int b)
{
 int a;
 a=b;
 if (a < 0) { tm1637.display(0,-1); tm1637.display(1,-1); tm1637.display(2,0xF); tm1637.display(3,0xF);} 
 else 
 {
  if (b<1000) tm1637.display(0,-1); else tm1637.display(0,a/1000); 
  a=a%1000;
  if (b<100) tm1637.display(1,-1); else tm1637.display(1,a/100);  
  a=a%100;
  if (b<10) tm1637.display(2,-1); else tm1637.display(2,a/10); 
  a=a%10;
  tm1637.display(3,a);
 }
}


void setup() 
{
 int intser;
 float calib;
 tm1637.init();
 tm1637.set(BRIGHT_TYPICAL);

 tm1637.display(0,1);
 tm1637.display(1,2);
 tm1637.display(2,3);
 tm1637.display(3,4);

 pinMode(14, OUTPUT);  // ainda não descobri a funcao deste pino...
 Serial.begin(9600);

 dht2.begin();
 dht1.begin();
 sensor_t sensor;
 dht2.temperature().getSensor(&sensor);
 delayMS = sensor.min_delay / 1000;  

 // sensors.begin();
 delay(100);
 scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

 /* Rotina para calibrar 
 Serial.println(" ");
 Serial.println("Calibrando .. ");

 scale.set_scale();
 scale.tare();
 Serial.println("Coloca um peso conhecido e digita algo");
 
 while (Serial.available() <= 0) ;
 intser=Serial.read();
 
 Serial.println("Lendo ");
 calib=scale.get_units(10);
 Serial.println(calib);  
 // pega dez medidas e retorna float
 // divide o valor retornado pelo peso conhecido
 // passa a resposta no set_scale()
 Serial.print("calibrando com ");
 Serial.println(calib);  
 Serial.println("Digita algo para continuar ");
 while (Serial.available() <= 0) ;
 /* Fim calibracao */
 
 scale.set_scale(271794/130); // 88110/42);
 Serial.print("Valor calibrado = "); 
 Serial.println( 271794/130); // 88110/42);

 scale.tare(); 

 // calibrado 

 /*WiFi.begin(ssid, password);
 Serial.println(".");
 while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
 MBServer.begin();
 Serial.println("Connected ");
 Serial.print("ESP8266 Slave Modbus TCP/IP ");
 Serial.print(WiFi.localIP()); Serial.print(":"); Serial.println(String(ModbusTCP_port));
 Serial.println("Modbus TCP/IP Online"); */
 Serial.println("Pronto  ");
}


void loop() {
 /*
 WiFiClient client = MBServer.available();
 if (!client) { return; } 
 */
 float Temperatura1, Humidade1, Temperatura2, Humidade2;
 boolean flagClientConnected = 0;
 byte byteFN = MB_FC_NONE;
 int Start;
 int WordDataLength;
 int ByteDataLength;
 int MessageLength;
 
 // Debugando
 while(1)
 {
  //sensors.requestTemperatures();
  //Temperatura=sensors.getTempCByIndex(0);
  gramas=scale.get_units(3);

  sensors_event_t event;
  dht2.temperature().getEvent(&event);
  if (isnan(event.temperature)) { Serial.println(F("Error reading temperature!"));}
   else Temperatura2 = event.temperature;
  dht2.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {Serial.println(F("Error reading humidity!"));}
   else Humidade2=event.relative_humidity;

  dht1.temperature().getEvent(&event);
  if (isnan(event.temperature)) { Serial.println(F("Error reading temperature!"));}
   else Temperatura1 = event.temperature;
  dht1.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {Serial.println(F("Error reading humidity!"));}
   else Humidade1=event.relative_humidity;

  
  Serial.print("Temperatura1 = ");    Serial.print(Temperatura1,1);   
  Serial.print(" ; Humidade1 = ");    Serial.print(Humidade1,1); 
  Serial.print(" ; Temperatura2 = "); Serial.print(Temperatura2,1);   
  Serial.print(" ; Humidade2 = ");    Serial.print(Humidade2,1); 

  Serial.print(" ; Gramas = ");   Serial.print(gramas,1); 
  Serial.println(" ");
  
  display_peso(gramas);
  delay(1000);
 }
} 
 
 /*
 // Modbus TCP/IP
 while (client.connected()) {
   if(client.available()) {
      flagClientConnected = 1;
      int i = 0;
      while(client.available()) {
        ByteArray[i] = client.read();
        i++;
      }
      client.flush();

      sensors.requestTemperatures();
      Temperatura=sensors.getTempCByIndex(0);
      gramas=scale.get_units(3);

      //sensorValue = analogRead(A0);
      //Temperatura=sensorValue*3.3/10.24;
      //gramas=scale.get_units();
      //rotacao=digitalRead(interruptPin);

      //Serial.print("Sens Value = "); Serial.print(sensorValue);
      Serial.print("  , Temperatura = "); Serial.print(Temperatura); 
      Serial.print("  , Gramas = "); Serial.println(gramas,1);
      // Serial.print("  , counter = "); Serial.println(rotacao); 
      
      ///// code here --- codigo aqui
 
      ///////// Holding Register [0] A [9] = 10 Holding Registers Escritura
      ///////// Holding Register [0] A [9] = 10 Holding Registers Writing
 
      MBHoldingRegister[0] = (int)Temperatura; //sensorValue; // random(0,12);
      MBHoldingRegister[1] = (int)gramas;      //(int)Temperatura; // dec_gramas; // random(0,12);
      MBHoldingRegister[2] = 0; //(int)gramas; //random(0,12);
      MBHoldingRegister[3] = 0; // rotacao; // random(0,12);
      MBHoldingRegister[4] = 0; //random(0,12);
      MBHoldingRegister[5] = 0; //random(0,12);
      MBHoldingRegister[6] = 0; //random(0,12);
      MBHoldingRegister[7] = 0; //random(0,12);
      MBHoldingRegister[8] = 0; //random(0,12);
      MBHoldingRegister[9] = 0; //random(0,12);
 
      ///////// Holding Register [10] A [19] = 10 Holding Registers Lectura
      ///// Holding Register [10] A [19] = 10 Holding Registers Reading
 
      int Temporal[10];
 
      Temporal[0] = MBHoldingRegister[10];
      Temporal[1] = MBHoldingRegister[11];
      Temporal[2] = MBHoldingRegister[12];
      Temporal[3] = MBHoldingRegister[13];
      Temporal[4] = MBHoldingRegister[14];
      Temporal[5] = MBHoldingRegister[15];
      Temporal[6] = MBHoldingRegister[16];
      Temporal[7] = MBHoldingRegister[17];
      Temporal[8] = MBHoldingRegister[18];
      Temporal[9] = MBHoldingRegister[19];
 
      /// Enable Output 14
      digitalWrite(14, MBHoldingRegister[14] );
 
      //// debug
      for (int i = 0; i < 10; i++) {
       Serial.print("[");
        Serial.print(i);
        Serial.print("] ");
        Serial.print(Temporal[i]);
      }
      Serial.println("");
 
      //// end code - fin 
 
      //// routine Modbus TCP
      byteFN = ByteArray[MB_TCP_FUNC];
      Start = word(ByteArray[MB_TCP_REGISTER_START],ByteArray[MB_TCP_REGISTER_START+1]);
      WordDataLength = word(ByteArray[MB_TCP_REGISTER_NUMBER],ByteArray[MB_TCP_REGISTER_NUMBER+1]);
    }
 
    // Handle request
    switch(byteFN) {
 
      case MB_FC_NONE:
        break;
 
      case MB_FC_READ_REGISTERS: // 03 Read Holding Registers
        ByteDataLength = WordDataLength * 2;
        ByteArray[5] = ByteDataLength + 3; //Number of bytes after this one.
        ByteArray[8] = ByteDataLength; //Number of bytes after this one (or number of bytes of data).
        for(int i = 0; i < WordDataLength; i++) {
          ByteArray[ 9 + i * 2] = highByte(MBHoldingRegister[Start + i]);
          ByteArray[10 + i * 2] = lowByte(MBHoldingRegister[Start + i]);
        }
        MessageLength = ByteDataLength + 9;
        client.write((const uint8_t *)ByteArray,MessageLength);
        byteFN = MB_FC_NONE;
        break;
  
      case MB_FC_WRITE_REGISTER: // 06 Write Holding Register
        MBHoldingRegister[Start] = word(ByteArray[MB_TCP_REGISTER_NUMBER],ByteArray[MB_TCP_REGISTER_NUMBER+1]);
        ByteArray[5] = 6; //Number of bytes after this one.
        MessageLength = 12;
        client.write((const uint8_t *)ByteArray,MessageLength);
        byteFN = MB_FC_NONE;
        break;
 
      case MB_FC_WRITE_MULTIPLE_REGISTERS: //16 Write Holding Registers
        ByteDataLength = WordDataLength * 2;
        ByteArray[5] = ByteDataLength + 3; //Number of bytes after this one.
        for(int i = 0; i < WordDataLength; i++) {
          MBHoldingRegister[Start + i] = word(ByteArray[ 13 + i * 2],ByteArray[14 + i * 2]);
        }
        MessageLength = 12;
        client.write((const uint8_t *)ByteArray,MessageLength); 
        byteFN = MB_FC_NONE;
        break;
    }
  }
} */
