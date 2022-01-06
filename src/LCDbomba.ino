/*******************************************************************
 * Gerenciador de partidas de Airsoft
 * Timbó-SC
 * Versão: Em lançamento
 * Data: 21/09/2021
 * Projeto feito em uma esp32 doit-devkit-v1
 *******************************************************************/
//Bibliotecas utilizadas:
#include <LiquidCrystal.h>                      //Controlar e printar no LCD
#include <WiFi.h>                               //COnectar numa rede usando o WIFI/Criar uma rede de wifi
#include "ESPAsyncWebServer.h"                  //Permite criar um WEbserver assíncrono   
#include <AsyncTCP.h>                           //Gerencia e permite múltiplas conexões
#include "painlessMesh.h"                       //Bibliota responsavel pelo broadcast de informações entre ESPs
#include "painlessmesh/mesh.hpp"
#include "painlessmesh/tcp.hpp"
#include "plugin/performance.hpp"
#include "SPIFFS.h"


/*******************************************************************
 * Necessários para o MESH (Vide documentação)
 *******************************************************************/
#define   MESH_PREFIX     "BOMBA"
#define   MESH_PASSWORD   "Siam2k19"
#define   MESH_PORT       5555

/*******************************************************************
 * Necessários para o MESH (Vide documentação)
 *******************************************************************/
#define   HOSTNAME "Host do projeto"
#define   STATION_SSID     "SSID do projeto"
#define   STATION_PASSWORD "projeto"

/*******************************************************************
 * Definindo os dados do Wifi          
 *******************************************************************/
AsyncWebServer server(80);                    //porta de acesso 
Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;
IPAddress myIP(10,1,1,1);
IPAddress myAPIP(10,2,2,2);

/*******************************************************************
 *Copiado do exemplo :D        
 *******************************************************************/
void sendMessage() ; // Prototype so PlatformIO doesn't complain
Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

/*******************************************************************
 *Copiado do exemplo :D        
 *******************************************************************/
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}
void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}
void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

/*******************************************************************
 * Definindo qual o nome dos pinos da ESP32             
 *******************************************************************/
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(19, 23, 18, 17, 16, 15);                     
const int BOTINC = 4 ;                          //Botão do incremento                          
const int BOTDEC = 2;                           //BOtão do decremento  
int CLEAR = 27 ;                                //Clock no 74164 VERMELHO
int CRED = 12;                                  //Limpa todos os leds (todos os clear dos 74164 estão neste pino
int CBLUE = 13;                                 //Define se o que vai ser incrementado é 1 ou 0

/*******************************************************************
 * Definindo estado inicial das variáveis               
 *******************************************************************/
int counter = 0;                                //registrador de bandeira virada, neg azul e pos = vermelho. Valor = numero de led acessos
int BTSTS = HIGH;                               //Low= continua contando   High= Resete
int BTLSTS = 0;                                 //Ultimo estado do botão (boolean) 
int REG2 = 0;  //                               //Registrador geral de dados (Volátil)
unsigned long currentMillis =0;                 //tempo atual
unsigned long previousMillis = 0;               // ultimo tempo quando passou na rotina (Se a diferenca for maior que interval, atualiza os displays
const long interval = 1000;                     // interval at which to blink (milliseconds)
unsigned long lastwifistatus;                   //registra o ultimo estado analisado do wifi para saber se ouve mudança do estado
unsigned long wifistatus;                       // status atual da conexão
unsigned long meuip = 0;                        //IP do ESP para printar na tela
long rssi = 0;                                  //armazena o dado do ganho do sinal de wifi
char att[] = "1";                                 //Char que vai manter o valor de counter no xhtml enviado para a página
 
/*******************************************************************
 *Esta função inteira vai ser enviada no HTML para trocar o valor de Value pela String desejada. Ou então não altera nada
 *******************************************************************/

String ReadCounter(const String& value){  
  if(value == "PROGRESS"){                        //Se for essa palavra
    float counterx;
    counterx =map(counter, -8, 8, 0, 16);
    counterx = counterx * 6.25;
    return String(counterx);                      //troca por essa
  }
    return String();                              //Se não altera nada
}
/*******************************************************************
 * Função que tranforma o valor do contador em string
 *******************************************************************/
String AttCounter(){
  float counterx;
  counterx =map(counter, -8, 8, 0, 16);
  counterx = counterx * 6.25;
  itoa(counterx, att, 10);
  return String (att);
}
/*******************************************************************
 *Qual a mensagem enviada no Broad Cast?
 *******************************************************************/
void sendMessage() {
  String msg = "Hello from node da mesa.";
  msg += "--- ID =";
  msg += mesh.getNodeId();
  msg += "Counter =";
  msg += counter; 
  Serial.println("DEBUG---    " + msg);
  mesh.sendBroadcast( msg , true );
  taskSendMessage.setInterval( random( TASK_SECOND * 1 , TASK_SECOND * 5 ));
}

/*******************************************************************
 * SETUP INICIAL (OCORRE UMA VEZ)
 *******************************************************************/
void setup()
{
//Declarando o modo dos Pinos
//Entradas:
   pinMode(BOTINC, INPUT_PULLUP); //Botão do time A
   pinMode(BOTDEC, INPUT_PULLUP); //Botão do time B
//Saídas:
   pinMode(CRED, OUTPUT);         //Clock do 74hc164 dos leds vermelhos
   pinMode(CBLUE, OUTPUT);        //Clock do 74hc164 dos leds Azuis
   pinMode(CLEAR, OUTPUT);        //Clear de ambos os 74hc164, apagando todos os leds ao mesmo tempo

//Iniciando display  
    lcd.begin(16, 2); //16 colunas 2 linhas

//Iniciando serial monitor 
    Serial.begin(9600);            //inicia a comunicação com o serial terminal                      
  
// Iniciando o Wifi
    WiFi.mode(WIFI_AP);           //STA == Station mode == modo de operação para que o dispositivo funcione como um cliente wireless 
                                  //AP == Acess point == é o modo de operação para que o dispositivo funcione como um servidor/roteador
                                  //APSTA == coexistência entre os modos STA e AP. Note que, nesse modo, o ESP32  prioriza o modo STA sob o AP.                                 
    WiFi.disconnect();            //Disconecta possível wifis  

 /*******************************************************************
 *Copiado do exemplo :D        
 *******************************************************************/
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT ,WIFI_AP_STA,  6);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();


/*******************************************************************
 * Criando caracteres para o display
 *******************************************************************/
//Criando o padrão de bits que fazem os caracteres    
   byte pos[8] = {   //Cria um grupo de bits personalizado chamado pos
    0b11000,
    0b11100,
    0b11110,
    0b11111,
    0b11111,
    0b11110,
    0b11100,
    0b11000
 }; 
 byte neg[8] = {   //Cria um grupo de bits personalizado chamado pos
    0b00011,
    0b00111,
    0b01111,
    0b11111,
    0b11111,
    0b01111,
    0b00111,
    0b00011
 }; 
   byte tip[8] = { 
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111
 }; 
   byte empty[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b11111,
    0b00000,
    0b00000,
    0b00000,
    0b00000
 };
    byte mid2 [8] = {
    0b10000,
    0b10000,
    0b10000,
    0b11111,
    0b10000,
    0b10000,
    0b10000,
    0b10000
 };
    byte mid1[8] = {
    0b00001,
    0b00001,
    0b00001,
    0b11111,
    0b00001,
    0b00001,
    0b00001,
    0b00001
 };
     byte wifis[8] = {
    0b00001,
    0b00001,
    0b00001,
    0b00001,
    0b00101,
    0b00101,
    0b10101,
    0b10101
 };
//define os grupos de bits acima como caracteres para o LCD, onde basta chamar o número que o caractér será gravado na tela
  lcd.createChar(0, pos);  //quando chamar o byte zero para LCD WRITE sempre usar "byte(0)" e não somente "0"
  lcd.createChar(1, neg);
  lcd.createChar(2, tip);
  lcd.createChar(3, empty);  
  lcd.createChar(4, mid1);
  lcd.createChar(5, mid2);
  lcd.createChar(6, wifis);

/*******************************************************************
 *Primeiras comunicações no painel
 *******************************************************************/
// Prints iniciais:
   Serial.println("Rebooting");
   lcd.setCursor(0, 0);                                  //coluna,linha
   lcd.print("Rebooting");

   lcd.print(".");
   Serial.println(".");

   lcd.print(".");
   Serial.println(".");

   lcd.print(".");
   Serial.println(".");

   lcd.print(".");     
   Serial.println(".");                                 // trecho por questões aesteticas


 /*******************************************************************
 * Requisições do Webserver Assíncrono são configuradas aqui
 *******************************************************************/
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){                //quando houver conexão só com IP, manda a estrutura da página toda (index)
  request->send_P(200, "text/html", index_html, ReadCounter);                //200= thttpstatus OK, como a página será enviada, página Web, rotina te alteração automática
     Serial.println("REquisição de /");
     Serial.println(WiFi.localIP());
});
server.on("/att", HTTP_GET, [](AsyncWebServerRequest *request){                //quando houver conexão só com IP, manda a estrutura da página toda (index)
  request->send_P(200, "text/plain", AttCounter().c_str());                //200= thttpstatus OK, como a página será enviada, página Web, rotina te alteração automática
       Serial.println("REquisição de /att");
       Serial.println(AttCounter().c_str());
});
server.begin(); 
 } 

/*******************************************************************
 * Rotina principal
 *******************************************************************/
 void loop()
{
//Leitura dos botões:  Se pressionar somente INC, vai incrementar
                     // Se pressionar somente DEC, vai decrementar
                     // Se pressionar ambos os botões, zera e não autera o valor
   if (digitalRead(BOTDEC) == HIGH  && digitalRead(BOTINC) == LOW)  {
    BTLSTS = HIGH;      //seta que deve incrementar
    BTSTS = LOW;        //Indica que deve alterar o estado constantemente
   }   
   if (digitalRead(BOTDEC) == LOW && digitalRead(BOTINC) == HIGH)  {
    BTLSTS = LOW;      //seta que deve incrementar
    BTSTS = LOW;        //Indica que deve alterar o estado constantemente
   }
   if (digitalRead(BOTDEC) == LOW && digitalRead(BOTINC) == LOW)  {   
    BTSTS = HIGH;        //Indica que houve um ressete e não deve alterar o valor de couter até que seja pressionado um botão
    counter = 0;         //zera contador
   } 
 // Roda o Mesh    
    mesh.update();
//atualiza os timers se caso a bandeira esteja capturada
                     // Se pressionar somente DEC, vai decrementar
                     // Se pressionar ambos os botões, zera e não autera o valor

//************* trecho acima roda a todo momento*****************
 currentMillis = millis();                                //registra o tempo que passou no reg atual
 if (currentMillis - previousMillis >= interval)          //compara se a diferenca de tempo atual e ultima é maior que intervalo      
 {
        previousMillis = currentMillis;                   // atualizar o current para se repetir
    
//************* Trecho abaixo roda a cada 1 segundo**************

 /*******************************************************************
 * Lista todos os Wifi no monitor
 *******************************************************************/                                  
// Serial.println("scan start");
//
//    // WiFi.scanNetworks will return the number of networks found
 //   int n = WiFi.scanNetworks();
//    Serial.println("scan done");
//    if (n == 0) {
//        Serial.println("no networks found");
//    } else {
//        Serial.print(n);
//        Serial.println(" networks found");
//        for (int i = 0; i < n; ++i) {
//            // Print SSID and RSSI for each network found
//            Serial.print(i + 1);
//            Serial.print(": ");
//            Serial.print(WiFi.SSID(i));
//            Serial.print(" (");
//            Serial.print(WiFi.RSSI(i));
//            Serial.print(")");
//            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
//            delay(10);
//        }
//    }
 
 
 
 
 
 
 
 //Verificar se deve incrementar (Caso btsts seja zero não faz nada, caso seja um, analisa qual o ultimo botão pressionado)
  if (BTLSTS == HIGH  && BTSTS == LOW && counter <=7){  //Ultimo botão pressionado foi somente incrementar e o contador não chegou no limite?
    counter++;     //Incrementa contador                    
  }
  if (BTLSTS == LOW  && BTSTS == LOW && counter >=-7)
    {  //Ultimo botão pressionado foi somente DECREMENTAR e o contador não chegou no limite?
    counter--;     //Decrementa contador     
    }  
    
//Escreve os led
        digitalWrite(CLEAR, LOW);         // Limpa Leds
        delayMicroseconds(100);             //Delay para o clock
        digitalWrite(CLEAR, HIGH);        // Limpa Leds
        delayMicroseconds(100);             //Delay para o clock
        REG2 = counter ;                  // Salva o valor do contador em um registrador volatil
        if (REG2 < 0)                     //Caso Reg2 seja negativo (Azul
        {                                 
          while ( REG2!= 0)
          {                               //Subrotina para dar o numero de REG2 no Clock dos Leds, reg2 = -1 logo 1 led Azul acesso, reg2 = -2 logo 2 leds azuis acesos
          digitalWrite(CBLUE, HIGH);     //Pulso alto do clock
          REG2 ++;                        //remove um valor de reg para saber o valor
          delayMicroseconds(100);             //Delay para o clock
          digitalWrite (CBLUE, LOW);      //Pulso baixo do clock
          delayMicroseconds(100);             //Delay para o clock
          }                             
        }
        if (REG2 > 0) 
        {                                 //Caso Reg2 seja negativo (Azul)
          while ( REG2!= 0)
         {                               //Subrotina para dar o numero de REG2 no Clock dos Leds, reg2 = -1 logo 1 led vermelho acesso, reg2 = -2 logo 2 leds vermelho acesos
           digitalWrite (CRED, HIGH);    //Pulso alto do clock
           REG2 --;                      //remove um valor de reg para saber o valor
           delayMicroseconds(100);             //Delay para o clock
           digitalWrite (CRED, LOW);     //Pulso baixo do clock
           delayMicroseconds(100);             //Delay para o clock
         }                             
        }
 // Escrevendo a primeira Linha: 'Contador= "Valor de contador"'
      lcd.clear();             //limpa o lcd para evitar que existam dados perdidos
      lcd.setCursor(0, 0);
      lcd.print("blue   vs   Red");
      //lcd.setCursor(4, 0);
      //lcd.print(REG2);    
 //Escrevendo a segunda linha (coluna, Linha):
       if (counter == -9){
        lcd.setCursor(0, 1);
        lcd.write(2); //1
        lcd.write(2); //2
        lcd.write(2); //3
        lcd.write(2); //4
        lcd.write(2); //5
        lcd.write(2); //6
        lcd.write(2); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
     
        
       }
       if (counter == -8){
        lcd.setCursor(0, 1);
        lcd.write(1); //1
        lcd.write(2); //2
        lcd.write(2); //3
        lcd.write(2); //4
        lcd.write(2); //5
        lcd.write(2); //6
        lcd.write(2); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == -7){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(1); //2
        lcd.write(2); //3
        lcd.write(2); //4
        lcd.write(2); //5
        lcd.write(2); //6
        lcd.write(2); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == -6){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(1); //3
        lcd.write(2); //4
        lcd.write(2); //5
        lcd.write(2); //6
        lcd.write(2); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == -5){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(1); //4
        lcd.write(2); //5
        lcd.write(2); //6
        lcd.write(2); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == -4){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(1); //5
        lcd.write(2); //6
        lcd.write(2); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == -3){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(1); //6
        lcd.write(2); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == -2){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(1); //7
        lcd.write(2); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == -1){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(1); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == 0){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(5); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }  
        if (counter == 1){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(byte(0)); //9
        lcd.write(3); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }  
      if (counter == 2){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(byte(0)); //10
        lcd.write(3); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == 3){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(2); //10
        lcd.write(byte(0)); //11
        lcd.write(3); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      } 
      if (counter == 4){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(2); //10
        lcd.write(2); //11
        lcd.write(byte(0)); //12
        lcd.write(3); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }  
      if (counter == 5){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(2); //10
        lcd.write(2); //11
        lcd.write(2); //12
        lcd.write(byte(0)); //13
        lcd.write(3); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == 6){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(2); //10
        lcd.write(2); //11
        lcd.write(2); //12
        lcd.write(2); //13
        lcd.write(byte(0)); //14
        lcd.write(3); //15
        lcd.write(3); //16
      }
      if (counter == 7){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(2); //10
        lcd.write(2); //11
        lcd.write(2); //12
        lcd.write(2); //13
        lcd.write(2); //14
        lcd.write(byte(0)); //15
        lcd.write(3); //16
      }
      if (counter == 8){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(2); //10
        lcd.write(2); //11
        lcd.write(2); //12
        lcd.write(2); //13
        lcd.write(2); //14
        lcd.write(2); //15
        lcd.write(byte(0)); //16
      }  
        if (counter == 9){
        lcd.setCursor(0, 1);
        lcd.write(3); //1
        lcd.write(3); //2
        lcd.write(3); //3
        lcd.write(3); //4
        lcd.write(3); //5
        lcd.write(3); //6
        lcd.write(3); //7
        lcd.write(4); //8
        lcd.write(2); //9
        lcd.write(2); //10
        lcd.write(2); //11
        lcd.write(2); //12
        lcd.write(2); //13
        lcd.write(2); //14
        lcd.write(2); //15
        lcd.write(2); //16
      }
     }
    }
  
