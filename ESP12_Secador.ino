// Desidratador de laboratorio com medição de peso, temperatura e humidade.
// Versao simplificado com ESP webserver 
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
// 09/01/2021  Versao funcionando com webserver simples
// 12/01/2021  Potenciometro ok, Medicao de velocidade da FAN Ok com interrupcao no pino D7
//             Tem uma instabilidade no DHT22-AM2302 (mais fácil trocar e colocar o DH11).
//             Tentei fazer um controlador proporcional da velocidade da FAN com taxa de amostragem
//             de 1 e 0,5 segundo, mas não funcionou. Vai ter que sintonizar um PID.
//             A interrupcao do Timer não funcionou..  Fiz um soft interrupt no loop 
//  
//  Hardware ESP12F 
//             
//  Hardware
//  A0  potmeter
//  D0  CLK TM1637
//  D1  DIO TM1637
//  D2  PWM_FAN       
//  D3  DHTPIN11 DHT11   
//  D4  DHTPIN22 DHT22-AM2302      
//  D5  DOUT HX711
//  D6  SCK  HX711
//  D7  SENS_ROT
//  D8  PWM_RES 

/* Configurando FAN */
#define PWM_FAN  D2
#define SENS_ROT D7
#define potmeter A0

/* Configuracao Display
 *  
 */
#include "TM1637.h"
#define CLK D0 //pins definitions for TM1637 and can be changed to other ports
#define DIO D1  //  conferir era D1
TM1637 tm1637(CLK,DIO);

/* Configuracao Wifi e webserver
 *  
 */

#include <ESP8266WiFi.h>
#include "acesso_wifi.h"  // define ssid & password
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>




#define TIMER_INTERRUPT_DEBUG      1
#include "ESP8266TimerInterrupt.h"
#define TIMER_INTERVAL_MS  1000
ESP8266Timer ITimer;
void ICACHE_RAM_ATTR handleInterrupt();

/* Configuracao Celula de carga 
*/
#include "HX711.h"
const int LOADCELL_DOUT_PIN = D5;
const int LOADCELL_SCK_PIN  = D6;
HX711 scale;


/* Temperatura */
/* 
 *
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 6 // D4//
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float Temperatura3; */

/* Configuracao sensores DHT11 e DHT22
 *  
 */

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
uint32_t delayMS;


#define DHTPIN1 D3  // 3 
#define DHTTYPE1    DHT11     // DHT 11
DHT_Unified dht1(DHTPIN1, DHTTYPE1);

#define DHTPIN2 D4    // 4  conferir
#define DHTTYPE2    DHT11     // DHT 22 (AM2302)
DHT_Unified dht2(DHTPIN2, DHTTYPE2);

/* Variaveis globais 
 *  
 */
float gramas;
float Temperatura1, Humidade1, Temperatura2, Humidade2;
int velocidade=0;
volatile byte interruptCounter =0;
int pot=0;


void ICACHE_RAM_ATTR handleInterrupt() {
  interruptCounter++;
}

/*void ICACHE_RAM_ATTR TimerHandler()
{
 //velocidade++; // =interruptCounter;
 //interruptCounter=0;
}*/


ESP8266WebServer server(80);

int ModbusTCP_port = 502;
WiFiServer MBServer(ModbusTCP_port);



void handleRoot() {
  server.send(200, "text/plain", "Webserver Desidratador esp8266 \n Temperatura 1 = " + String(Temperatura1) + 
    " graus \n Umidade 1     = " + String(Humidade1) + 
  " porcento\n Velocidade    = " + String(velocidade) + 
    " rps   \n Potmeter      = " + String(pot) + 
 " unidades \n Peso          = " + String(gramas) + " gramas"  );
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


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

void configura_webserver(void)
{
 WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.on("/gif", []() {
    static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = millis() % 256;
    gif_colored[17] = millis() % 256;
    gif_colored[18] = millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void setup() 
{
 pinMode(PWM_FAN, OUTPUT);
 pinMode(SENS_ROT, INPUT_PULLUP);
 pinMode(D8, OUTPUT);

 digitalWrite(PWM_FAN, 0); 
 
 Serial.begin(57600);
 delay(1000);
 attachInterrupt(digitalPinToInterrupt(SENS_ROT), handleInterrupt, FALLING);
 
 tm1637.init();
 tm1637.set(BRIGHT_TYPICAL);
 tm1637.display(0,1);
 tm1637.display(1,2);
 tm1637.display(2,3);
 tm1637.display(3,4);

 pinMode(14, OUTPUT);  // ainda não descobri a funcao deste pino...

 configura_webserver();
 
 // Serial.println("Configurando sensores");

 /*
  Sensor DS qeu da problema
  sensors.begin();
 Serial.print("Found ");
 Serial.print(sensors.getDeviceCount(), DEC); */ 
 
 dht2.begin();
 dht1.begin();
 sensor_t sensor1;
 sensor_t sensor2;

 dht2.temperature().getSensor(&sensor2);
 delayMS = sensor2.min_delay / 1000;  
 //Serial.print("Delay DHT2="); 
 //Serial.println(delayMS);

 dht1.temperature().getSensor(&sensor1);
 delayMS = sensor1.min_delay / 1000;  
 //Serial.print("Delay DHT1=");
 //Serial.println(delayMS);

 // sensors.begin();
 delay(100);
 scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

 /* Rotina para calibrar 

 int intser;
 float calib;
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
 //Serial.println("Pronto  ");

 
}

unsigned long previousMillis = 0;
int erro =0;
int saida =0;
float pot2;

void loop() {


 pot2 =  (float(analogRead(potmeter))*3.3/(1023))/0.01; //  analogRead(potmeter); ///10;
 //analogWrite(PWM_FAN, pot); //saida);
 
 unsigned long currentMillis = millis();
 if (currentMillis - previousMillis >= 1000)
 {  
  previousMillis = currentMillis;
  velocidade=interruptCounter;
  interruptCounter=0;

  //pot = analogRead(potmeter)/10;
  //erro=pot-velocidade;
  //saida=erro*6;
  //if (saida<0) erro=0;
  //if (saida>1024) saida=1024;
  analogWrite(PWM_FAN, saida);

  Serial.print("Temp1= \t");       Serial.print(Temperatura1,1);  
  Serial.print("\t Humid1= \t");   Serial.print(Humidade1,1); 
  Serial.print("\t Temp2= \t"); Serial.print(Temperatura2,1);   
  Serial.print("\t Humid2= \t");    Serial.print(Humidade2,1); 
  Serial.print("\t Gramas= \t");   Serial.print(gramas,1); 
  Serial.print("\t Pot= ");        Serial.print(pot2,1); 
  Serial.print("\t Velo= ");       Serial.print(velocidade); 
 /* Serial.print("Erro= "); 
  Serial.print(erro);Serial.print(" \t");
  Serial.print("Saida= "); 
  Serial.print(saida);*/
  Serial.println(" "); 
  
 } 
 gramas=scale.get_units(3);

 sensors_event_t event1;
 sensors_event_t event2;
  
 dht2.temperature().getEvent(&event2);
 if (isnan(event2.temperature)) { Serial.println(F("Error reading temperature 2!"));}
   else Temperatura2 = event2.temperature;
 dht2.humidity().getEvent(&event2);
 if (isnan(event2.relative_humidity)) {Serial.println(F("Error reading humidity 2!"));}
   else Humidade2=event2.relative_humidity; 
  
 dht1.temperature().getEvent(&event1);
 if (isnan(event1.temperature)) { Serial.println(F("Error reading temperature 1!"));}
  else Temperatura1 = event1.temperature;
 dht1.humidity().getEvent(&event1);
 if (isnan(event1.relative_humidity)) {Serial.println(F("Error reading humidity 1!"));}
  else Humidade1=event1.relative_humidity; 

  
 display_peso(gramas);

 server.handleClient();
 MDNS.update();
 delay(10); 
} 
 
