// Desidratador de laboratorio com medição de peso, temperatura e humidade.
// Versao simplificado com ESP webserver 
// 
// 06/12/2020  Primeira Versao com medicao de peso, temperatura testando o ESP12
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
// 20/01/2021  Soft interrupt usando ticker para medir a frequencia 
//             Re-estruturou o programa separando MODBUS-IP, com mostra de dados pelos serial 
//             Funcionamento também sem Internet pelo serial, 
//             leitura de sensores e escrita pwm no loop principal
//             Falta acertar a fonte e ruidos nos sensores.
// 25/01/2021  Retirada do display, Instalação do sensor DS18B20 no pino D1
//             Resolveu calibraçao do HX711 com set_offset()
//             Resolveu ruído HX711 com resistor pull down de 10K no sinal de Dados
//             Filtro de dados fora do range na temperatura, humidade e massa
//             Versão estável com pouca interferencia de ruidos (Curva de secagem Banana com ScadaBR)
//             Falta 1) trocar DHT11 na saida por um DHT22 pois fica fora da faixa de operacao
//             Falta 2) implementar rotina de controle de velocidade
//             Falta 3) implementar rotina de controle de temperatura
// 31/01/2021  Usando EEPROM para gravar a calibração da balanca eletronica
//             O pino D0 será usada para habilitar a calibracao online 
//             Quando D0 for 0 ele entrará numa rotina de calibracao via serial (ou wifi no futuro)
// 02/02/2021  Implementacao PID com taxa de amastragem de 1 segundo 
//             Parametros PID q0, q1, q2 via ScadaBR base 100
//             Monitoracao Erros e Saida via ScadaBR
//             Habilitacao rotina PID via ScadaBR
//             PID não funcionou muito bem. Vamos ter que encarar outro algoritmo
// 12/02/2021  MODBUS Registradores Saidas
//             0 Temperatura1 
//             1 Humidade1
//             2 Temperatura2 
//             3 Humidade2  
//             4 gramas
//             5 Temperatura3
//             6 velocidade 
//             7 potmeter 
//             8 Erro 
//             9 Saida 
//  
//             MODBUS Registradores Entradas
//             pwm_saida_fan  10
//             pwm_saida_res  11
//             pwm_frequencia 12
//             controle_PI    13
//             q0_PD          14
//             q0             15
//             q1_PD          16
//             q1             17
//             q2_PD          18
//             q2             19
//
// 13/02/2021  Mudando os sensores de temperaturas para trabalhar com décimas de graus.
//             Trabalhando com inteiros multiplicados por 10.
//             Opcao para trabalhar com P, PI, PD e PID selecionado pelo ScadaBR
//
// 22/02/2021  Colocando display OLE e usando TX e RX como IO
//             Impressao de boot OK.. Precisa ainda detalhar WiFi 
//             Impressao de valores no display OK
//             Botao de inicio de secagem OK..
//             Falta ver display na sequencia de calibracao
//             Controle padrao PD com q0=3 e q1=-1 e fan em 600  
//               
//  Hardware
//  A0  potmeter
//  D0  calibracao
//  D1  temperatura one wire
//  D2  PWM_FAN       
//  D3  DHTPIN1 DHT11 (1)   
//  D4  DHTPIN1 DHT11 (2)      
//  D5  DOUT HX711
//  D6  SCK  HX711
//  D7  SENS_ROT
//  D8  PWM_RES
//  TX  OLE_SDA
//  RX  OLE_SCL

// Incluindo display 
//
#include "SSD1306Wire.h"  
// #include "images.h"  
SSD1306Wire display(0x3c, 3, 1);  // (RX=SDA,TX=SCL)

// Temporizadores
// 

#include<Ticker.h>
//Ticker tick_mostra_display;
Ticker tick_temporizador1s;

/* Configurando FAN */
#define PWM_FAN  D2
#define PWM_RES  D8
#define SENS_ROT D7
#define potmeterpin A0

/* Configuracao Wifi e webserver
 *  
 */

#include <ESP8266WiFi.h>
#include "acesso_wifi.h"  // define ssid & password
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

/* https://circuits4you.com/2018/01/02/esp8266-timer-ticker-example/
#define LED 2
void ICACHE_RAM_ATTR onTimerISR(){
    digitalWrite(LED,!(digitalRead(LED)));  //Toggle LED Pin
    timer1_write(600000);//12us
} */


/* Configuracao Celula de carga 
*/
#include "HX711.h"
const int LOADCELL_DOUT_PIN = D5;
const int LOADCELL_SCK_PIN  = D6;

HX711 scale;
/* calibracao balanca */
#include <EEPROM.h>
#define pin_calibracao D0


/* Temperatura DS18B20*/
/* 
 */
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS D1 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor_ds18(&oneWire);

/* Configuracao sensores DHT11 e DHT22
 *  
 */

//#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
uint32_t delayMS;

#define DHTPIN1 D3  // mudou  
#define DHTTYPE1    DHT11     // DHT 11
DHT_Unified dht1(DHTPIN1, DHTTYPE1);

#define DHTPIN2 D4    // 4  conferir
#define DHTTYPE2    DHT22  // 11     // DHT 22 (AM2302)
// DHT_Unified dht2(DHTPIN2, DHTTYPE2);
DHT dht2(DHTPIN2, DHTTYPE2);

/* Variaveis globais 
 *  
 */
float gramas;
float Temperatura1, Humidade1, Temperatura2, Humidade2;
float Temperatura3; 
int velocidade=0;
volatile byte interruptCounter =0;
int potmeter=0;
unsigned int pwm_frequencia;
int pwm_saida_fan;
int pwm_saida_res;
int segundos=0;


void ICACHE_RAM_ATTR handleInterrupt() {
  interruptCounter++;
}


ESP8266WebServer server(80);

////////////////////////////////////////////
// Configuracao do Modbus

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

int ModbusTCP_port = 502;
WiFiServer MBServer(ModbusTCP_port);

//////////////////////////////////////////////////////////////////////////

void handleRoot() {
  server.send(200, "text/plain", "Webserver Desidratador esp8266 \n Temperatura 1 = " + String(Temperatura1) + 
    " graus \n Umidade 1     = " + String(Humidade1) + 
  " porcento\n Velocidade    = " + String(velocidade) + 
    " rps   \n Potmeter      = " + String(potmeter) + 
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

unsigned int controle=0;
int Referencia;
float Saida, Saida_1, Erro, Erro_1, Erro_2;
float q0=0.125;
float q1=0.12375;
float q2=0;
unsigned int qPontosDecimais;

void temporizador1s(void)
{
 velocidade=interruptCounter;
 interruptCounter=0;
 segundos++;
 if (controle>0) 
 {
  Saida_1 = Saida;
  Erro_2 = Erro_1;
  Erro_1 = Erro;
  Erro = Referencia - Temperatura2; 
  switch(controle)
  {
   case 1: // Proportional
         Saida = q0 * Erro;
         break;
   case 2: // PI
         Saida = Saida_1 + q0 * Erro;
         break;
   case 3: // PD;
         Saida = q0 * Erro  + q1 * Erro_1;
         break;
   case 4: // PID 
         Saida = Saida_1 + q0 * Erro  + q1 * Erro_1 + q2 * Erro_2;
         break;                      
  }
  if (Saida < 0) { analogWrite(PWM_RES,0); Saida=0; } else
   if (Saida > 1023) { analogWrite(PWM_RES,1023); Saida=1023; } else
    analogWrite(PWM_RES, (unsigned int)Saida);
 }
}

void imprime_serial(void)
{
 display.clear();
 /*Serial.print("Temp1= ");     Serial.print(Temperatura1,1);  
 Serial.print("\t Humid1= "); Serial.print(Humidade1,1); 
 Serial.print("\t Temp2= ");  Serial.print(Temperatura2,1);   
 Serial.print("\t Humid2= "); Serial.print(Humidade2,1); 
 Serial.print("\t Gramas= "); Serial.print(gramas,1); 
 Serial.print("\t Velo= ");   Serial.print(velocidade);
 Serial.print("\t temp3= ");  Serial.print(Temperatura3,1);
 Serial.print("\t pot= ");     Serial.print(potmeter);
 Serial.println(" ");*/
 display.setFont(ArialMT_Plain_10);
 display.drawString( 0, 0, "T1="); display.drawString(20, 0, String(Temperatura1/10,1));   
 display.drawString(50, 0, "H1="); display.drawString(70, 0, String(Humidade1,0));
 display.drawString(85, 0, "H2="); display.drawString(105,0, String(Humidade2,0));
 //T3=
 //Temperatura3/10,1
 //display.drawString( 0,20, "T2="); display.drawString(25,20, String(Temperatura2/10,1)); 
 //display.drawString(60,20, "H2="); display.drawString(85,20, String(Humidade2,0));
 display.setFont(ArialMT_Plain_16);
 display.drawString( 0,17, "Temp="); display.drawString(55,17, String(Temperatura2/10,1));  
 display.drawString( 0,33, "Setp="); display.drawString(55,33, String(potmeter/10));
 display.drawString( 0,49, "Mass="); display.drawString(55,49, String(gramas,1));
 if (controle>0) 
 {
  display.drawString(95,33, "CTR"); 
  display.drawString(105,49, "On");
 } 
 display.display();
}

void configura_webserver(void)
{
 WiFi.mode(WIFI_STA);
 WiFi.begin(ssid, password);
 // Serial.println("");
 // Wait for connection
 segundos=0;
 while ((WiFi.status() != WL_CONNECTED) && (segundos<20)){
    delay(500);
    //Serial.print(".");
 }
 if (WiFi.status() == WL_CONNECTED)
 {
  display.drawString(0, 30, "WiFi OK");
  display.drawString(0, 40, ssid);  
  //display.drawString(0, 50, WiFi.localIP());
  /*Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());*/
  MBServer.begin();
 }
 else   display.drawString(0, 30, "Sem WiFi "); //Serial.println("Sem conexao internet "); 
  /*if (MDNS.begin("esp8266")) {
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
  Serial.println("HTTP server started"); */
  display.display();
}

void setup() 
{
 // Configurando Display
 //GPIO 1 (TX) swap the pin to a GPIO.
 pinMode(1, FUNCTION_3);  // para voltar FUNCTION_0
 //GPIO 3 (RX) swap the pin to a GPIO.
 pinMode(3, FUNCTION_3); 
 pinMode(1, OUTPUT);  
 pinMode(3, OUTPUT);  
 
 // Initialising the UI will init the display too.
 display.init();
 display.flipScreenVertically();
 display.setFont(ArialMT_Plain_10);
 display.setTextAlignment(TEXT_ALIGN_LEFT);
 display.drawString(0, 0, "Secador V1");
 display.display();
 // Final configurando display
 // 
 pinMode(PWM_FAN, OUTPUT);
 pinMode(SENS_ROT, INPUT_PULLUP);
 pinMode(PWM_RES, OUTPUT);
 pinMode(pin_calibracao, INPUT_PULLUP);

 digitalWrite(PWM_FAN, 0); 
 digitalWrite(PWM_RES, 0); 
 pwm_frequencia=6;
 analogWriteFreq(pwm_frequencia);
 
 // Serial.begin(57600);
 delay(1000);
 attachInterrupt(digitalPinToInterrupt(SENS_ROT), handleInterrupt, FALLING);

 tick_temporizador1s.attach(1,temporizador1s);
 
 /* inicializa o sensor DS18B20 */
 sensor_ds18.begin();
 /* inicializa sensores DHT11 */
 dht2.begin();
 dht1.begin();
 sensor_t sensor1;
 sensor_t sensor2;

 //dht2.temperature().getSensor(&sensor2);
 //delayMS = sensor2.min_delay / 1000;  

 dht1.temperature().getSensor(&sensor1);
 delayMS = sensor1.min_delay / 1000;  

 delay(100);
 scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

 /* Rotina para calibrar */
 int offset;
 float calib;
 if (digitalRead(pin_calibracao)==0)
 {
  float massa;
  String ss;
  String string_Value;
  display.drawString(0, 10, "Calibracao - Liga o USB");
  //display.drawString(0, 20, "Liga serial o cabo serial");
  display.display();
  delay(1000);
  
  pinMode(1, FUNCTION_0);  // para voltar FUNCTION_0
  pinMode(3, FUNCTION_0); 
  Serial.begin(9600);
  delay(1000); 

  while (Serial.available() <= 0) ;
  ss =Serial.readString();
  delay(500);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("Rotina de calibracao  ");
  Serial.println("Verifica se a tela estah no lugar sem nenhum peso digita algo.. ");
  while (Serial.available() <= 0) ;
  ss =Serial.readString();
  delay(500);
  scale.set_scale();
  scale.tare();
  Serial.println("Coloca um peso conhecido e digita a sua massa em gramas terminando ");
  while(Serial.available() <= 0);
  massa = Serial.parseFloat(); 
  Serial.print("Massa = ");
  Serial.println(massa);
  calib=scale.get_units(10)/massa;
  // pega dez medidas e retorna float
  // divide o valor retornado pelo peso conhecido
  // passa a resposta no set_scale()
  Serial.print("Valor calibracao = ");
  Serial.println(calib);  
  scale.set_scale(calib); 
  Serial.println("Retira o peso para pegar o valor do offset e digite algo "); 
  while (Serial.available() <= 0) ;
  ss =Serial.readString();
  offset=scale.get_offset(); 
  Serial.print("Valor Offset =");
  Serial.println(offset); 
  // Grava dados no E2prom
  EEPROM.begin(100);
  int n=0;
  string_Value=String(calib,1);
  string_Value=string_Value+";";
  for(n=0; n< string_Value.length();n++) { EEPROM.write(n,string_Value[n]); }
  EEPROM.commit();
  string_Value=String(offset);
  string_Value=string_Value+";";
  Serial.print("n = ");
  Serial.println(n);
  for(int m=0; m< string_Value.length();m++) { EEPROM.write(n,string_Value[m]); n++;}
  EEPROM.commit();
  Serial.println("Fim rotina calibracao - tira o USB e digita algo e enter");
  while (Serial.available() <= 0) ;
  ss =Serial.readString();
  delay(1000);
  pinMode(1, FUNCTION_3);  // para voltar FUNCTION_0
  pinMode(3, FUNCTION_3); 
  pinMode(1, OUTPUT);  
  pinMode(3, OUTPUT);  
 
 // Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Continuando");
  display.display();  
 } 
 else
 { // leia dados de calib do E2prom
  display.drawString(0, 10, "lendo e2prom");
  display.display();
  // Serial.println("Lendo dados de calibracao do e2prom");
  String string_Value1="";
  //calib= 2097.27;
  //offset= 338624; // 418671;
  int n=0;
  EEPROM.begin(100);
  while (char(EEPROM.read(n))!=';')
  {
   string_Value1 += String(char(EEPROM.read(n)));
   n++; 
  }  
  //Serial.println("Calibracao string = "+String(string_Value1));
  calib=string_Value1.toFloat();  
  n++;
  //Serial.print("n = ");
  //Serial.println(n);
  string_Value1="";
   
  while (char(EEPROM.read(n))!=';')
  {
   string_Value1 += String(char(EEPROM.read(n)));
   n++; 
  }  
  //Serial.println("Offset string = "+String(string_Value1));
  offset=string_Value1.toInt();  
 }

 /* Fim calibracao */
 scale.set_scale(calib);  
 /*Serial.print("Calibrando com  = "); 
 Serial.println(calib);  
 Serial.print("Ajustando offset com = ");
 Serial.println(offset); */

 display.drawString(0,20, "Off set");
 display.display();
 
 scale.set_offset(offset); 
 configura_webserver();

 MBHoldingRegister[10]=0;  //             pwm_saida_fan  10
 MBHoldingRegister[11]=0;  //             pwm_saida_res  11
 MBHoldingRegister[12]=6;  //             pwm_frequencia 12
 MBHoldingRegister[13]=0;  //             controle_PI    13
 MBHoldingRegister[14]=0;  //             q0_PD          14
 MBHoldingRegister[15]=3;  //             q0             15
 MBHoldingRegister[16]=0;  //             q1_PD          16
 MBHoldingRegister[17]=-1; //             q1             17
 MBHoldingRegister[18]=0;  //             q2_PD          18
 MBHoldingRegister[19]=1;  //             q2             19
 display.display(); 
 delay(3000);
 display.clear();
}


int erro =0;

void loop() 
{
 float T1,T2,T3;
 /* Saida acionamento */

 if (digitalRead(pin_calibracao)==0)
 {
  controle=3;
  pwm_saida_fan=600;
  delay(500);
 }
 
 analogWriteFreq(pwm_frequencia);
 analogWrite(PWM_FAN, pwm_saida_fan);

 if (controle==0) analogWrite(PWM_RES, pwm_saida_res);  
 /* le sensores */
 potmeter =  10*map(analogRead(potmeterpin),0,1024,80,0);  // escalando entre 1023/10  entre 0 - 102 graus
 Referencia=potmeter; // MBHoldingRegister[14];


 if (scale.wait_ready_timeout(500)) { gramas=scale.get_units(3);}
   else { /*Serial.println("Erro no HX711 ");*/ gramas = 0; } 
 if ((gramas > 1000) || (gramas < -200)) gramas=0;
   
 sensors_event_t event1;
 sensors_event_t event2;  

 // Sensor DHT11
 //dht2.temperature().getEvent(&event2);
 //if (isnan(event2.temperature)) { /*Serial.println(F("Error reading temperature 2!"));*/ Temperatura2=-1;}
 //   else Temperatura2 = 10*event2.temperature;
 //if ((Temperatura2 > 1000) || (Temperatura2 < -1)) Temperatura2=0;
    
 //dht2.humidity().getEvent(&event2);
 //if (isnan(event2.relative_humidity)) {/*Serial.println(F("Error reading humidity 2!"));*/ Humidade2=-1;}
 //   else Humidade2=event2.relative_humidity;   
 //if ((Humidade2 > 100) || (Humidade2 < -1)) Humidade2=0;

 T2 = 10*dht2.readTemperature();
 Humidade2    = dht2.readHumidity();
 if (isnan(T2) || isnan(Humidade2) ) {
    //Serial.println(F("Failed to read from DHT sensor!"));
    Humidade2=-1;
  } else Temperatura2=T2;

 
 dht1.temperature().getEvent(&event1);
 if (isnan(event1.temperature)) { /*Serial.println(F("Error reading temperature 1!"));*/ Temperatura1=-1;}
    else Temperatura1 = 10*event1.temperature;
 if ((Temperatura1 > 1000) || (Temperatura1 < -1)) Temperatura1=0;
 
 dht1.humidity().getEvent(&event1);
 if (isnan(event1.relative_humidity)) {/*Serial.println(F("Error reading humidity 1!"));*/ Humidade1=-1;}
    else Humidade1=event1.relative_humidity; 
 if ((Humidade1 > 100) || (Humidade1 < -1)) Humidade1=0;

 sensor_ds18.requestTemperatures();

 if(T3 == DEVICE_DISCONNECTED_C) {/*Serial.println("Error: Could not read temperature data");*/T3=-1;}
  else Temperatura3 = 10*sensor_ds18.getTempCByIndex(0);
 // if ((Temperatura3 > 1000) || (Temperatura3 < -1)) Temperatura3=0;
 
 /* */
    
 /*  fim leitura de sensores */   
 imprime_serial();
  
 WiFiClient client = MBServer.available();
 if (client) 
 { 
  
  boolean flagClientConnected = 0;
  byte byteFN = MB_FC_NONE;
  int Start;
  int WordDataLength;
  int ByteDataLength;
  int MessageLength;

  if (client.connected()) {
    if(client.available()) 
     {    
      flagClientConnected = 1;
      int i = 0;
      while(client.available()) {
        ByteArray[i] = client.read();
        i++;
      }
      client.flush();
  
      ///// code here --- codigo aqui
 
      ///////// Holding Register [0] A [9] = 10 Holding Registers Escritura
      ///////// Holding Register [0] A [9] = 10 Holding Registers Writing
 
      MBHoldingRegister[0] = (int)Temperatura1; 
      MBHoldingRegister[1] = (int)Humidade1; 
      MBHoldingRegister[2] = (int)Temperatura2; 
      MBHoldingRegister[3] = (int)Humidade2;  
      MBHoldingRegister[4] = (int)gramas;
      MBHoldingRegister[5] = (int)Temperatura3;
      MBHoldingRegister[6] = velocidade;  
      MBHoldingRegister[7] = potmeter; 
      MBHoldingRegister[8] = int(Erro); 
      MBHoldingRegister[9] = int(Saida); 

      ///////// Holding Register [10] A [19] = 10 Holding Registers Lectura
      ///// Holding Register [10] A [19] = 10 Holding Registers Reading
 
      unsigned int q0_PD, q1_PD, q2_PD;
      int temp;
       
      pwm_saida_fan=MBHoldingRegister[10];   // de 0 a 1023
      pwm_saida_res=MBHoldingRegister[11];   // de 0 a 1023
      pwm_frequencia=MBHoldingRegister[12];  // de 1 a max..    
       
      controle=MBHoldingRegister[13]; 
      
      q0_PD=MBHoldingRegister[14];     //pode ser 0,1 a 4
      temp=int(MBHoldingRegister[15]);         
      switch(q0_PD)
      {
        case 0:q0=(float)temp;break;
        case 1:q0=temp/10;    break;
        case 2:q0=temp/100;   break;
        case 3:q0=temp/1000;  break;
        case 4:q0=temp/10000; break;
      }
      
      q1_PD=MBHoldingRegister[16];   //pode ser 0,1 a 4
      temp=int(MBHoldingRegister[17]);         
      switch(q1_PD)
      {
        case 0:q1=(float)temp;break;
        case 1:q1=temp/10;    break;
        case 2:q1=temp/100;   break;
        case 3:q1=temp/1000;  break;
        case 4:q1=temp/10000; break;
      }

      q2_PD=MBHoldingRegister[18];  //pode ser 0,1 a 4
      temp=int(MBHoldingRegister[19]);         
      switch(q2_PD)
      {
        case 0:q2=(float)temp;break;
        case 1:q2=temp/10;    break;
        case 2:q2=temp/100;   break;
        case 3:q2=temp/1000;  break;
        case 4:q2=temp/10000; break;
      }
      
      /*
      //// debug
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
      for (int i = 0; i < 10; i++) {
       Serial.print("[");
        Serial.print(i);
        Serial.print("] ");
        Serial.print(Temporal[i]);
      }
      Serial.println(""); */
 
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
    }  // fim handle request switch 
  }
 }
}

 
 
