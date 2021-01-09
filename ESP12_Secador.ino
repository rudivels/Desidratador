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
//  
//  Hardware ESP12F 
//             
//  Hardware 
//  D2  CLK TM1637
//  D3  DIO TM1637
//  D4  DHTPIN11 DHT11  // ONeWire Temperatura
//  D5  DOUT HX711
//  D6  SCK  HX711
//  D7  DHTPIN AM2302 


/* Configuracao Display
 *  
 */
#include "TM1637.h"
#define CLK D2//pins definitions for TM1637 and can be changed to other ports
#define DIO D3
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
#define ONE_WIRE_BUS D4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float Temperatura; */

/* Configuracao sensores DHT11 e DHT22
 *  
 */

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

/* Variaveis globais 
 *  
 */
float gramas;
float Temperatura1, Humidade1, Temperatura2, Humidade2;

ESP8266WebServer server(80);

void handleRoot() {
  server.send(200, "text/plain", "Webserver Desidratador esp8266 \n Temperatura 1 = " + String(Temperatura1) + 
    " graus \n Umidade 1     = " + String(Humidade1) + 
  " porcento\n Temperatura 2 = " + String(Temperatura2) + 
    " graus \n Umidade 2     = " +  String(Humidade2) + 
 " porcento \n Peso          = " + String(gramas) + " gramas"  );
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

 configura_webserver();
 // calibrado 
 Serial.println("Pronto  ");
}


void loop() {

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
  server.handleClient();
  MDNS.update();
  
} 
 
