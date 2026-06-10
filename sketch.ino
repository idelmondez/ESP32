#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// =====================================================
// MQTT CLOUD
// =====================================================

const char* mqttServer =
"d796bc3f870f467c85b9fca2b48150e0.s1.eu.hivemq.cloud";

const int mqttPort = 8883;

const char* mqttUser = "Ivan-CEUB";
const char* mqttPassword = "1234Abcd";

// =====================================================
// TOPICOS MQTT
// =====================================================

const char* topicoTelemetria =
"ceub/str-embarcados/turma-a/grupo07/esp32-01/telemetria";

const char* topicoAlerta =
"ceub/str-embarcados/turma-a/grupo07/esp32-01/alerta";

const char* topicoStatus =
"ceub/str-embarcados/turma-a/grupo07/esp32-01/status";

const char* topicoComando =
"ceub/str-embarcados/turma-a/grupo07/esp32-01/comando";

// =====================================================
// PINOS
// =====================================================

#define LED_PIN 2
#define BUZZER_PIN 15

// =====================================================
// OBJETOS
// =====================================================

WiFiClientSecure espClient;
PubSubClient client(espClient);

// =====================================================
// VARIAVEIS
// =====================================================

float temperatura = 20.0;
float umidade = 50.0;

float limiteTemperatura = 30.0;

bool alarme = false;
bool ledRemoto = false;

// =====================================================
// TIMERS
// =====================================================

unsigned long tSensor = 0;
unsigned long tControle = 0;
unsigned long tTelemetria = 0;
unsigned long tLog = 0;
unsigned long tReconexao = 0;

// =====================================================
// CALLBACK MQTT
// =====================================================

void callback(char* topic,
              byte* payload,
              unsigned int length) {

  String msg = "";

  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.println();
  Serial.println("Comando recebido:");
  Serial.println(msg);

  if (msg.indexOf("\"led\":\"on\"") >= 0) {
    ledRemoto = true;
  }

  if (msg.indexOf("\"led\":\"off\"") >= 0) {
    ledRemoto = false;
  }

  int pos = msg.indexOf("limite");

  if (pos >= 0) {

    int inicio = msg.indexOf(":", pos);

    if (inicio > 0) {

      String valor =
      msg.substring(inicio + 1);

      valor.replace("}", "");
      valor.replace("\"", "");

      limiteTemperatura =
      valor.toFloat();

      Serial.print("Novo limite: ");
      Serial.println(limiteTemperatura);
    }
  }
}

// =====================================================
// WIFI
// =====================================================

void conectarWiFi() {

  Serial.println("Conectando WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// MQTT
// =====================================================

void conectarMQTT() {

  String clientId =
  "grupo07-" +
  String(random(100000));

  Serial.println("Tentando MQTT...");

  if (client.connect(
      clientId.c_str(),
      mqttUser,
      mqttPassword)) {

    Serial.println("MQTT conectado");

    client.subscribe(topicoComando);

    client.publish(
      topicoStatus,
      "{\"status\":\"online\"}"
    );

  } else {

    Serial.print("Falha MQTT. Codigo=");
    Serial.println(client.state());
  }
}

void manterMQTT() {

  if (client.connected()) {

    client.loop();
    return;
  }

  unsigned long agora = millis();

  if (agora - tReconexao >= 5000) {

    tReconexao = agora;

    conectarMQTT();
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  conectarWiFi();

  espClient.setInsecure();

  client.setServer(
    mqttServer,
    mqttPort
  );

  client.setCallback(callback);

  randomSeed(micros());

  Serial.println("Sistema iniciado");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  unsigned long agora = millis();

  // ---------------------------------
  // MQTT
  // ---------------------------------

  manterMQTT();

  // ---------------------------------
  // SENSOR SIMULADO
  // 500 ms
  // ---------------------------------

  if (agora - tSensor >= 500) {

    tSensor = agora;

    temperatura += 0.5;

    if (temperatura > 40.0) {
      temperatura = 20.0;
    }

    umidade =
    40 + random(0, 500) / 10.0;
  }

  // ---------------------------------
  // CONTROLE
  // 100 ms
  // ---------------------------------

  if (agora - tControle >= 100) {

    tControle = agora;

    alarme =
    temperatura >= limiteTemperatura;

    if (alarme) {

      digitalWrite(
        LED_PIN,
        HIGH
      );

      digitalWrite(
        BUZZER_PIN,
        HIGH
      );

    } else {

      digitalWrite(
        LED_PIN,
        ledRemoto ? HIGH : LOW
      );

      digitalWrite(
        BUZZER_PIN,
        LOW
      );
    }
  }

  // ---------------------------------
  // TELEMETRIA
  // 2 s
  // ---------------------------------

  if (
      client.connected() &&
      agora - tTelemetria >= 2000
     ) {

    tTelemetria = agora;

    String payload =
      "{"
      "\"temperatura\":"
      + String(temperatura,1)
      + ","
      "\"umidade\":"
      + String(umidade,1)
      + ","
      "\"limite\":"
      + String(limiteTemperatura,1)
      + ","
      "\"alarme\":"
      + String(alarme ? "true" : "false")
      + ","
      "\"timestamp_ms\":"
      + String(agora)
      + "}";

    bool enviado =
    client.publish(
      topicoTelemetria,
      payload.c_str()
    );

    if (enviado) {

      Serial.println();
      Serial.println("Telemetria enviada:");
      Serial.println(payload);

    } else {

      Serial.println(
        "Falha ao publicar telemetria"
      );
    }

    if (alarme) {

      client.publish(
        topicoAlerta,
        "{\"alerta\":\"temperatura_alta\"}"
      );
    }
  }

  // ---------------------------------
  // LOG LOCAL
  // 1 s
  // ---------------------------------

  if (agora - tLog >= 1000) {

    tLog = agora;

    Serial.println();
    Serial.println("==============");

    Serial.print("Temperatura: ");
    Serial.println(temperatura);

    Serial.print("Umidade: ");
    Serial.println(umidade);

    Serial.print("Limite: ");
    Serial.println(limiteTemperatura);

    Serial.print("Alarme: ");
    Serial.println(
      alarme ? "SIM" : "NAO"
    );

    Serial.print("MQTT: ");
    Serial.println(
      client.connected()
      ? "Conectado"
      : "Desconectado"
    );

    Serial.println("==============");
  }
}