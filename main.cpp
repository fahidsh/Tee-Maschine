#include <mbed.h>
#include <string>

#define YIELD_TIME 100ms
#define SLEEP_TIME 100ms
#define SLEEP_TIME_BETWEEN_INIT_STEPS 200ms

// Debugger
#define SHOW_SERIAL_DEBUG_MESSAGES true
#include "serial-debugger.h"

// WLANN
#include "ESP8266Interface.h"
// ANPASSEN: bitte die PINS for WLAN Module setzen (TX, RX)
ESP8266Interface wifi(PC_12, PD_2);

// extra thread for mqtt
Thread mqtt_thread;

// socket address
SocketAddress socket_address;

// MQTT
#include <MQTTClient.h>
#include <MQTTNetwork.h>
#include <MQTTmbed.h> // Countdown
// ANPASSEN: MQTT-Server domain oder IP
const std::string MQTT_SERVER = "192.168.43.1";
const unsigned int MQTT_PORT = 1883;
// ANPASSEN: MQTT-Benutzername, falls erforderlich
const std::string MQTT_USER = "";
// ANPASSEN: MQTT-Passwort, falls erforderlich
const std::string MQTT_PASSWORD = "";
// ANPASSEN: MQTT-Topic
const std::string MQTT_TOPIC = "public/mbed/teemaschine/cmd";
// ANPASSEN: Client Name, womit diese app sich bei MQTT-Server identifiziert
std::string mqtt_client_name = "esp8266_01";
const MQTT::QoS MQTT_SUB_QOS = MQTT::QOS2;
MQTTPacket_connectData MQTT_CONNECTION = MQTTPacket_connectData_initializer;
unsigned int mqtt_messages_received = 0;
MQTTNetwork mqttNet(&wifi);
MQTT::Client<MQTTNetwork, Countdown> mqtt_client(mqttNet);

// Allgemein Control
unsigned int anzahl_fehler = 0;

// Funktionen
void check_mqtt_message();
void process_incoming_mqtt_message(MQTT::MessageData &md);
bool ist_befehl(string befehl);
int initialize_mqtt_client();

void check_mqtt_message() {
  while (true) {
    mqtt_client.yield();
    ThisThread::sleep_for(YIELD_TIME);
  }
}

void process_incoming_mqtt_message(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  mqtt_messages_received++;
  int msg_length = message.payloadlen;

  std::string mqtt_message = (char *)message.payload;
  mqtt_message = mqtt_message.substr(0, msg_length);
  ist_befehl(mqtt_message);
}

int initialize_mqtt_client() {
  // MQTT Client Initialisierung
  char *client_id = (char *)mqtt_client_name.c_str();
  if (MQTT_CONNECTION.clientID.cstring != client_id) {
    MQTT_CONNECTION.MQTTVersion = 3;
    MQTT_CONNECTION.struct_version = 0;
    MQTT_CONNECTION.clientID.cstring = client_id;
    if (strlen(MQTT_USER.c_str()) > 1) {
      char *user = (char *)MQTT_USER.c_str();
      char *pass = (char *)MQTT_PASSWORD.c_str();
      MQTT_CONNECTION.username.cstring = user;
      MQTT_CONNECTION.password.cstring = pass;
    }
  }
  return 0;
}

void restart_controller() { NVIC_SystemReset(); }

int init() {
  int status = 0;
  status = wifi.connect(MBED_CONF_NSAPI_DEFAULT_WIFI_SSID,
                        MBED_CONF_NSAPI_DEFAULT_WIFI_PASSWORD,
                        NSAPI_SECURITY_WPA_WPA2);
  ThisThread::sleep_for(4000ms); // vier sekunden
  status = wifi.gethostbyname(MQTT_SERVER.c_str(), &socket_address);
  ThisThread::sleep_for(SLEEP_TIME_BETWEEN_INIT_STEPS);
  initialize_mqtt_client();
  ThisThread::sleep_for(SLEEP_TIME_BETWEEN_INIT_STEPS);
  status = mqttNet.connect(MQTT_SERVER.c_str(), MQTT_PORT);
  ThisThread::sleep_for(SLEEP_TIME_BETWEEN_INIT_STEPS);
  status = mqtt_client.connect(MQTT_CONNECTION);
  status = mqtt_client.subscribe(MQTT_TOPIC.c_str(), MQTT_SUB_QOS,
                                 process_incoming_mqtt_message);
  /******************************************************************
  ##################################################################
  ##################################################################
  CODE BIS HIER IST WICHTIG,
  BITTE NUR DASS ÄNDERN WAS SIE WISSEN WAS UND WARUM SIE ES ÄNDERN
  ******************************************************************/

  return 0;
}
/******************************************************************
 *        MAIN FUNKTION/ ENTRY-POINT                               *
 ******************************************************************/

#include "LCD.h"

// LCD
lcd tm_lcd;
bool lcd_aktiv = true;

// Enum
enum COMMANDS {
  STOP,
  MACH_TEE,
  TB_UNTEN,
  TB_HOCH,
  TB_SHAKE,
  STAND_WEGWERF,
  STAND_HOME,
  FAHR_RECHTS,
  FAHR_LINKS,
  FAHR_OBEN,
  FAHR_UNTEN,
  KALIBRIEREN
};

// std::map<std::string, COMMANDS> command_map;

// Motoren
PortOut motor_teebeutel(PortC, 0b1111); // C 0-3
int motor_teebeutel_verschiebung = 0;
PortOut motor_stand(PortC, 0b111100000000); // C 8-11
int motor_stand_verschiebung = 8;
bool ist_motor1_aktiv = true;
int motorlauf[] = {0b0001, 0b0011, 0b0010, 0b0110,
                   0b0100, 0b1100, 0b1000, 0b1001};
/*
  unsigned int in vergleich zur normal (signed) int verstehe ich nicht, weil
  z.B. (int und unsigned int sind 32-bit/4-Byte)
    int var_a = 0 - 1 ;
    - Dezimal Wert: var_a = -1;
    - in Binär es ist: 11111111 11111111 11111111 11111111

    unsigned int var_b = 0 - 1;
    - Dezimal Wert: var_b = 4294967295;
    - in Binär es ist: 11111111 11111111 11111111 11111111

  aber weil Herr Kohler es so sagt!, nehmen wir unsigned int für motor_pos
*/
unsigned int position_motor_teebeutel = 0;
unsigned int position_motor_stand = 0;
#define MOTOR_SPEED 1ms

// control vars
std::string aktueller_befehl = "0";
int stand_pos_home = 0;
int stand_pos_wegwerf = -5100; // differenz 5100
int teebeutel_hoch = 0;
int teebeutel_unten = -3500; // differenz 4000
int teebeutel_shake = 1500;
int step_size = 1000;
bool erste_befehl = true;
bool zeige_motor_pos = false;

// Methoden
void show_on_lcd(const char *msg);
void fahr_teebeutel_unten();
void fahr_teebeutel_hoch();
void shake_teebeutel();
void fahr_stand_zu_wegwerf_pos();
void fahr_stand_home();
void mache_tee();
void fahr_rechts();
void fahr_links();
void fahr_oben();
void fahr_unten();
void kalibrieren();

int main() {
  show_on_lcd("Tee Maschine v2");
  int init_result = init(); // Wichtig, nicht ändern
  if (init_result == 0) {

    mqtt_thread.start(
        check_mqtt_message); // MQTT Message Handler in eigner Thread
    LOG(MessageType::INFO, "Init Erfolgreich");
    tm_lcd.clear();
    tm_lcd.cursorpos(0);
    show_on_lcd("Init Erfolgreich");

  } else { // init war nicht erfolgreicht

    LOG(MessageType::ERROR, "Init fehlgeschlagen");
    LOG(MessageType::ERROR,
        "Mikrocontroller wird in 5 Sekunden neugestartet...");
    show_on_lcd("Init fehlschlag!Neustart in 5S");
    ThisThread::sleep_for(5s); // warte 5 sekunden
    restart_controller();      // MC neustarten

  } // if(init_result == 0)

  while (true) {

    if (zeige_motor_pos) {
      LOG(MessageType::DEBUG, "%d, %d", position_motor_teebeutel,
          position_motor_stand);
    }

    ThisThread::sleep_for(500ms);

  } // while(true)

  return 0;
}

/**
 * @brief Prüft ob gegebenen String ein Befehl ist
 * Wenn ja, wird der Befehl ausgeführt und true zurückgegeben
 * Wenn nein, wird false zurückgegeben
 *
 * @param befehl ein string welche geprüft werden soll
 * @return true wenn der parameter ein befehl war
 * @return false wenn der parameter kein befehl war
 */
bool ist_befehl(string befehl) {
  // die letzte befehl von mqtt-server wird nachdem start geholt
  // die erste befehl ignorieren
  if (erste_befehl) {
    erste_befehl = false;
    return true;
  }

  // für jede gewünschte befehl, eine neue if/else if block hinzufügen
  // in der if/else if block, eure Function aufrufen und dann return true machen
  if (befehl == "STOP") {

    LOG(MessageType::INFO, "STOP");
    show_on_lcd("STOP");
    return true;

  } else if (befehl == "MACH_TEE") {

    LOG(MessageType::INFO, "MACH_TEE");
    show_on_lcd("MACH_TEE");
    mache_tee();
    return true;

  } else if (befehl == "TB_UNTEN") {

    LOG(MessageType::INFO, "TB_UNTEN");
    show_on_lcd("TB_UNTEN");
    fahr_teebeutel_unten();
    return true;

  } else if (befehl == "TB_HOCH") {

    LOG(MessageType::INFO, "TB_HOCH");
    show_on_lcd("TB_HOCH");
    fahr_teebeutel_hoch();
    return true;

  } else if (befehl == "TB_SHAKE") {

    LOG(MessageType::INFO, "TB_SHAKE");
    show_on_lcd("TB_SHAKE");
    shake_teebeutel();
    return true;

  } else if (befehl == "STAND_WEGWERF") {

    LOG(MessageType::INFO, "STAND_WEGWERF");
    show_on_lcd("STAND_WEGWERF");
    fahr_stand_zu_wegwerf_pos();
    return true;

  } else if (befehl == "STAND_HOME") {

    LOG(MessageType::INFO, "STAND_HOME");
    show_on_lcd("STAND_HOME");
    fahr_stand_home();
    return true;
  } else if (befehl == "FAHR_RECHTS") {

    LOG(MessageType::INFO, "FAHR_RECHTS");
    show_on_lcd("FAHR_RECHTS");
    fahr_rechts();
    return true;
  } else if (befehl == "FAHR_LINKS") {

    LOG(MessageType::INFO, "FAHR_LINKS");
    show_on_lcd("FAHR_LINKS");
    fahr_links();
    return true;
  } else if (befehl == "FAHR_OBEN") {

    LOG(MessageType::INFO, "FAHR_OBEN");
    show_on_lcd("FAHR_OBEN");
    fahr_oben();
    return true;
  } else if (befehl == "FAHR_UNTEN") {

    LOG(MessageType::INFO, "FAHR_UNTEN");
    show_on_lcd("FAHR_UNTEN");
    fahr_unten();
    return true;
  } else if (befehl == "KALIBRIEREN") {

    LOG(MessageType::INFO, "KALIBRIEREN");
    show_on_lcd("KALIBRIEREN");
    kalibrieren();
    return true;
  }

  return false;
}

void show_on_lcd(const char *msg) {

  if (!lcd_aktiv) {
    return;
  }

  tm_lcd.clear();
  tm_lcd.cursorpos(0);
  if (strlen(msg) <= 16) {
    tm_lcd.printf("%s", msg);
  } else {
    char print_msg[16];
    // die erste 16 zeichen an lcd zeile 1 ausgeben
    memcpy(print_msg, &msg[0], 16);
    tm_lcd.printf("%s", msg);
    // die nächste 16 zeichen an lcd zeile 1 ausgeben
    tm_lcd.cursorpos(64);
    memcpy(print_msg, &msg[16], 16);
    tm_lcd.printf("%s", msg);
  }
}

void fahr_teebeutel_unten() { // unten_pos: -4000
  /*
      unsigned int a = 0 - 1;
      dann a ist jetzt: 4294967295
      aber (int)a ist: -1
  */
  while ((int)position_motor_teebeutel > teebeutel_unten) {
    position_motor_teebeutel--;
    ThisThread::sleep_for(MOTOR_SPEED);
    motor_teebeutel = motorlauf[position_motor_teebeutel % 8]
                      << motor_teebeutel_verschiebung;
  }
  show_on_lcd("Fertig");
}

void fahr_teebeutel_hoch() { // teebeutal_hoch: 0
  while ((int)position_motor_teebeutel < teebeutel_hoch) {
    position_motor_teebeutel++;
    ThisThread::sleep_for(MOTOR_SPEED);
    motor_teebeutel = motorlauf[position_motor_teebeutel % 8]
                      << motor_teebeutel_verschiebung;
  }
  show_on_lcd("Fertig");
}

void shake_teebeutel() {
  // der stand muss auf home-pos sein
  if (position_motor_stand != stand_pos_home) {
    return;
  }
  // teebeutel muss unten sein
  if (position_motor_teebeutel != teebeutel_unten) {
    return;
  }

  while ((int)position_motor_teebeutel < (teebeutel_unten + teebeutel_shake)) {
    position_motor_teebeutel++;
    ThisThread::sleep_for(MOTOR_SPEED);
    motor_teebeutel = motorlauf[position_motor_teebeutel % 8]
                      << motor_teebeutel_verschiebung;
  }

  fahr_teebeutel_unten();
  show_on_lcd("Fertig");
}

void fahr_stand_zu_wegwerf_pos() {
  while ((int)position_motor_stand > stand_pos_wegwerf) {
    position_motor_stand--;
    ThisThread::sleep_for(MOTOR_SPEED);
    motor_stand = motorlauf[position_motor_stand % 8]
                  << motor_stand_verschiebung;
  }
  show_on_lcd("Fertig");
}

void fahr_stand_home() {
  while ((int)position_motor_stand < stand_pos_home) {
    position_motor_stand++;
    ThisThread::sleep_for(MOTOR_SPEED);
    motor_stand = motorlauf[position_motor_stand % 8]
                  << motor_stand_verschiebung;
  }
  show_on_lcd("Fertig");
}

void mache_tee() {
  fahr_stand_home();
  fahr_teebeutel_unten();
  for (int i = 0; i < 5; i++) {
    shake_teebeutel();
  }
  fahr_teebeutel_hoch();
  fahr_stand_zu_wegwerf_pos();
  show_on_lcd("Fertig");
}

void fahr_rechts() {
  for (int i = 0; i < step_size; i++) {
    position_motor_stand++;
    motor_stand = motorlauf[position_motor_stand % 8]
                  << motor_stand_verschiebung;
    ThisThread::sleep_for(MOTOR_SPEED);
  }
  show_on_lcd("Fertig");
}
void fahr_links() {
  for (int i = 0; i < step_size; i++) {
    position_motor_stand--;
    motor_stand = motorlauf[position_motor_stand % 8]
                  << motor_stand_verschiebung;
    ThisThread::sleep_for(MOTOR_SPEED);
  }
  show_on_lcd("Fertig");
}
void fahr_oben() {
  for (int i = 0; i < step_size; i++) {
    position_motor_teebeutel++;
    motor_teebeutel = motorlauf[position_motor_teebeutel % 8]
                      << motor_teebeutel_verschiebung;
    ThisThread::sleep_for(MOTOR_SPEED);
  }
  show_on_lcd("Fertig");
}
void fahr_unten() {
  for (int i = 0; i < step_size; i++) {
    position_motor_teebeutel--;
    motor_teebeutel = motorlauf[position_motor_teebeutel % 8]
                      << motor_teebeutel_verschiebung;
    ThisThread::sleep_for(MOTOR_SPEED);
  }
  show_on_lcd("Fertig");
}

void kalibrieren() {
  position_motor_stand = 0;
  position_motor_teebeutel = 0;
  show_on_lcd("Fertig");
}