#include <SPI.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <analogWrite.h>
#include <MFRC522.h>

#define RST_PIN 22
#define SS_PIN 21

#define redLed 13
#define greenLed 14
#define blueLed 12
#define botao 27

//WiFi
const char* SSID = "BIN 2.4";
const char* PASSWORD = "95609560";
WiFiClient wifiClient;

//MQTT Server
const char* BROKER_MQTT = "broker.hivemq.com";
int BROKER_PORT = 1883;

#define ID_MQTT  "disp_oneid" // Codigo do Dispositivo
#define TOPIC_PUB "bgmbnewgen8462/oneid/empresa/request"
#define TOPIC_SUB "bgmbnewgen8462/oneid/empresa/response"
//#define userJuridico "NewGen"
//String TOPIC_PUB = String(PUB) + String(userJuridico);
//String TOPIC_SUB = String(SUB) + String(userJuridico);
PubSubClient MQTT(wifiClient);

//Declaração das Funções
void mantemConexoes();
void conectaWiFi();
void conectaMQTT();
void enviaPacote();
void recebePacote(char* topic, byte* payload, unsigned int length);

MFRC522 mfrc522(SS_PIN, RST_PIN);
unsigned long millisWifi, tempoResposta, tempoAguradaWifi = millis();
String tag = "";
int estado = 1;
String servico = "1";

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(recebePacote);
  pinMode(botao, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(botao), verificaBotaoServico, RISING);
}

void loop() {
  if (canInterrupt()) {
    mantemConexoes();
    switch (estado)
    {
      case 1:// Aguandando Tag
        estadoLedAguadandoTag();
        leRfid();
        pubMessage();
        estado = 2;
        break;
      case 2://Aguarda Resposta
        MQTT.loop();
        break;
      case 3://Sucesso
        estadoLedPositivoTag();
        if ((millis() - tempoResposta) > 3000) {
          tempoResposta = millis();
          estado = 1;
        }
        break;
      case 4://Negativo
        estadoLedNegativoTag();
        if ((millis() - tempoResposta) > 3000) {
          tempoResposta = millis();
          estado = 1;
        }
        break;
    }
  }
}

void verificaBotaoServico()
{
  if (canInterrupt()) {
    if (servico == "1") {
      servico = "2";
      for (int i = 1; i >= 2; i++)
      {
        estadoLedPositivoTag();
        delayMicroseconds(1000);
        estadoLedAguadandoTag();
        delayMicroseconds(1000);
        estadoLedPositivoTag();
        delayMicroseconds(1000);
        estadoLedAguadandoTag();
      }
    } else {
      servico = "1";
      estadoLedPositivoTag();
      delayMicroseconds(1000);
      estadoLedAguadandoTag();
    }
    Serial.println(servico);
  }
}

void pubMessage()
{
  StaticJsonDocument<96> doc;
  doc["idTag"] = tag;
  doc["idDispositivo"] = ID_MQTT;
  doc["servico"] = servico;
  String output;
  serializeJson(doc, output);
  char Buf[100];
  output.toCharArray(Buf, 100);
  Serial.println(output);
  MQTT.publish(TOPIC_PUB, Buf, 1);
}

void leRfid() {
  tag = "";
  while ( ! mfrc522.PICC_IsNewCardPresent())
  {
    delay(100);
  }
  if ( ! mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    tag.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  tag.toUpperCase();
}

bool canInterrupt() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  // Verifica se a ultima interrupção foi à 200 ms
  if (interrupt_time - last_interrupt_time > 1500)
  {
    last_interrupt_time = interrupt_time;
    return true;
  }
  return false;
}

void mantemConexoes() {
  if (!MQTT.connected()) {
    estado = 1;
    conectaMQTT();
  }

  conectaWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

void conectaWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  estado = 1;
  Serial.print("Conectando-se na rede: ");
  Serial.print(SSID);
  Serial.println("  Aguarde!");

  WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI
  while (WiFi.status() != WL_CONNECTED) {
    if ((millis() - millisWifi) < 1000) {
      estadoLedPositivoTag();
    } else {
      estadoLedNegativoTag();
    }
    if ((millis() - millisWifi) > 2000) {
      millisWifi = millis();
    }
    if ((millis() - tempoAguradaWifi) > 10000) {
      tempoAguradaWifi = millis();
      conectaWiFi();
    }
  }

  Serial.println();
  Serial.print("Conectado com sucesso, na rede: ");
  Serial.print(SSID);
  Serial.print("  IP obtido: ");
  Serial.println(WiFi.localIP());
}

void conectaMQTT() {
  while (!MQTT.connected()) {
    Serial.print("Conectando ao Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado ao Broker com sucesso!");
      MQTT.subscribe(TOPIC_SUB);
    }
    else {
      estado = 1;
      Serial.println("Nao foi possivel se conectar ao broker.");
      Serial.println("Nova tentatica de conexao em 10s");
      delay(10000);
    }
  }
}

void recebePacote(char* topic, byte* payload, unsigned int length)
{
  String msg;
  //obtem a string do payload recebido
  for (int i = 0; i < length; i++)
  {
    char c = (char)payload[i];
    msg += c;
  }
  Serial.println(msg);
  deserializeJsonMessage(msg);
}

void deserializeJsonMessage(String msg)
{
  StaticJsonDocument<96> doc;

  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  String codDispositivo = doc["codDispositivo"]; // 0
  int resultado = doc["resultado"];
  controlaResultado(resultado, codDispositivo);
}

void controlaResultado(int resultado, String codDispositivo) {
  if (codDispositivo == ID_MQTT) {
    switch (resultado)
    {
      case 1:
        estado = 3;
        break;
      case 0:
        estado = 4;
        break;
    }
  }
}

void limpaLedRGB() {
  analogWrite(redLed, 0);
  analogWrite(greenLed, 0);
  analogWrite(blueLed, 0);
}

void estadoLedAguadandoTag() {
  limpaLedRGB();
  analogWrite(redLed, 255);
  analogWrite(greenLed, 0);
  analogWrite(blueLed, 255);
}

void estadoLedPositivoTag() {
  limpaLedRGB();
  analogWrite(redLed, 0);
  analogWrite(greenLed, 255);
  analogWrite(blueLed, 0);
}

void estadoLedNegativoTag() {
  limpaLedRGB();
  analogWrite(redLed, 255);
  analogWrite(greenLed, 0);
  analogWrite(blueLed, 0);
}
