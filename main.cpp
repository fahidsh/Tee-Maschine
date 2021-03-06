#include <mbed.h>
#include <string>

#define YIELD_TIME 100ms
#define SLEEP_TIME 100ms
#define SLEEP_TIME_BETWEEN_INIT_STEPS 200ms
bool critical_failure = false;

// Debugger
#define SHOW_SERIAL_DEBUG_MESSAGES true
#include "serial-debugger.h"

// WLANN
#include "ESP8266Interface.h"
ESP8266Interface
    wifi(PC_12,
         PD_2); // ANPASSEN: bitte die PINS for WLAN Module setzen (TX, RX)

// extra thread for mqtt
Thread mqtt_thread;

// socket address
SocketAddress socket_address;

// MQTT
#include <MQTTClient.h>
#include <MQTTNetwork.h>
#include <MQTTmbed.h> // Countdown
const std::string MQTT_SERVER =
    "192.168.43.1"; // ANPASSEN: MQTT-Server domain oder IP
const unsigned int MQTT_PORT = 1883;
const std::string MQTT_USER =
    ""; // ANPASSEN: MQTT-Benutzername, falls erforderlich
const std::string MQTT_PASSWORD =
    ""; // ANPASSEN: MQTT-Passwort, falls erforderlich
const std::string MQTT_TOPIC =
    "public/mbed/teemaschine/cmd"; // ANPASSEN: MQTT-Topic
std::string mqtt_client_name =
    "esp8266_01"; // ANPASSEN: Client Name, womit diese app sich bei MQTT-Server
                  // identifiziert
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
typedef int (*Step)();
int execute_step(Step what_step, int max_attempts = 5);
int connect_to_wifi();
int do_dns_lookup();
int initialize_mqtt_client();
void show_mqtt_options();
int connect_to_mqtt_server();
int subscribe_to_mqtt_topic();
void restart_controller();

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
  if (!ist_befehl(mqtt_message)) {
    LOG(MessageType::INFO, "MSG RCVD: {%d, %d, %s}", mqtt_messages_received,
        msg_length, mqtt_message.c_str());
  }
}

/*
    This function executes a given function and examine its return value
    and keeps executing the function until a return value of 0 is received
    or max_attempts have been made
    the function name to execute is passed as parameter
    parameter function must have a return type of int
*/

typedef int (*Step)();
int execute_step(Step what_step, int max_attempts) {
  int attempts = 0;
  int status = -1;
  if (!critical_failure) {
    while (status != 0 && attempts < max_attempts) {
      status = what_step();
      // DEBUG_LOG(0,".");
      ThisThread::sleep_for(SLEEP_TIME_BETWEEN_INIT_STEPS);
      attempts++;
    }
  }

  // ThisThread::sleep_for(SLEEP_TIME_BETWEEN_STEPS);
  // if critical failure has not occured and status is not 0/OK
  if (status != 0 && !critical_failure) {
    critical_failure = true;
  }
  return status;
}

int connect_to_wifi() {

  int status = -1;
  // WLAN Verbindung herstellen
  status = wifi.connect(MBED_CONF_NSAPI_DEFAULT_WIFI_SSID,
                        MBED_CONF_NSAPI_DEFAULT_WIFI_PASSWORD,
                        NSAPI_SECURITY_WPA_WPA2);
  if (status == 0) {
    wifi.set_as_default();
    LOG(MessageType::INFO, "WLAN Verbindung hergestellt");
    wifi.get_ip_address(&socket_address);
    LOG(MessageType::INFO, "IP Adresse: %s", socket_address.get_ip_address());
  } else {
    LOG(MessageType::ERROR,
        "WLAN Verbindung konnte nicht hergestellt werden, Status: %d", status);
    return status;
  }
  return status;
}

int do_dns_lookup() {
  // MQTT Server DNS Lookup
  int status = -1;
  status = wifi.gethostbyname(MQTT_SERVER.c_str(), &socket_address);
  if (status == 0) {
    LOG(MessageType::INFO, "DNS Query Result of [%s]: %s", MQTT_SERVER.c_str(),
        socket_address.get_ip_address());
  } else {
    LOG(MessageType::ERROR, "DNS Query failed for [%s], status: %d",
        MQTT_SERVER.c_str(), status);
    return status;
  }
  return status;
}

int initialize_mqtt_client() {
  // MQTT Client Initialisierung
  char *client_id = (char *)mqtt_client_name.c_str();

  if (MQTT_CONNECTION.clientID.cstring != client_id) {

    MQTT_CONNECTION.MQTTVersion = 3;
    MQTT_CONNECTION.struct_version = 0;
    MQTT_CONNECTION.clientID.cstring = client_id;

    // MQTT_CONNECTION.cleansession = 1;
    if (strlen(MQTT_USER.c_str()) > 1) {
      char *user = (char *)MQTT_USER.c_str();
      char *pass = (char *)MQTT_PASSWORD.c_str();
      MQTT_CONNECTION.username.cstring = user;
      MQTT_CONNECTION.password.cstring = pass;
    }
  }
  return 0;
}

void show_mqtt_options() {
  LOG("");
  LOG("Client Name: %s", MQTT_CONNECTION.clientID.cstring);
  LOG("Cleansession: %d", MQTT_CONNECTION.cleansession);
  LOG("Mqtt Version: %d", MQTT_CONNECTION.MQTTVersion);
  LOG("Mqtt Struct Version: %d", MQTT_CONNECTION.struct_version);
  LOG("Mqtt User: %s", MQTT_CONNECTION.username.cstring);
  LOG("Mqtt Pass: %s", MQTT_CONNECTION.password.cstring);
  LOG("");
}

int connect_to_mqtt_server() {
  // MQTT Client Verbindung herstellen
  int status = -1;
  status = mqttNet.connect(MQTT_SERVER.c_str(),
                           MQTT_PORT); // tcp_socket.connect(socket_address);
  status = mqtt_client.connect(MQTT_CONNECTION);
  if (status == 0) {
    LOG(MessageType::INFO, "MQTT Client Verbindung hergestellt");
  } else {
    LOG(MessageType::ERROR,
        "MQTT Client Verbindung konnte nicht hergestellt werden, Status: %d",
        status);
    return status;
  }
  return status;
}

int subscribe_to_mqtt_topic() {
  // MQTT Topic Abonieren
  int status = mqtt_client.subscribe(MQTT_TOPIC.c_str(), MQTT_SUB_QOS,
                                     process_incoming_mqtt_message);
  if (status == 0) {
    LOG(MessageType::INFO, "MQTT Topic [%s] abonniert", MQTT_TOPIC.c_str());
  } else {
    LOG(MessageType::ERROR,
        "MQTT Topic konnte nicht abonniert werden, Status: %d", status);
    return status;
  }
  return status;
}

void restart_controller() { NVIC_SystemReset(); }

int init() {

  LOG(MessageType::INFO, "Setup started...");
  int status = 0;

  LOG("Connecting to Wifi");
  status = execute_step(connect_to_wifi, 10);
  if (status != 0)
    return status;

  LOG("Resolving Mqtt-Host to IP-Address");
  status = execute_step(do_dns_lookup);
  if (status != 0)
    return status;

  LOG("Setting Mqtt-Connection Parameters");
  status = execute_step(initialize_mqtt_client);
  if (status != 0)
    return status;
  // show_mqtt_options();

  LOG("Connecting to Mqtt Server");
  status = execute_step(connect_to_mqtt_server, 1);
  if (status != 0)
    return status;

  LOG("Subscribing to Mqtt-Topic: %s", MQTT_TOPIC.c_str());
  status = execute_step(subscribe_to_mqtt_topic);
  if (status != 0)
    return status;

  /******************************************************************
  ##################################################################
  ##################################################################
  ##################################################################
  ##################################################################
  ##################################################################
  ##################################################################
  ##################################################################
  ##################################################################
  CODE BIS HIER IST WICHTIG,
  BITTE NUR DASS ??NDERN WAS SIE WISSEN WAS UND WARUM SIE ES ??NDERN
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
enum COMMANDS{
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
    - in Bin??r es ist: 11111111 11111111 11111111 11111111

    unsigned int var_b = 0 - 1;
    - Dezimal Wert: var_b = 4294967295;
    - in Bin??r es ist: 11111111 11111111 11111111 11111111
  
  aber weil Herr Kohler es so sagt!, nehmen wir unsigned int f??r motor_pos
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
int step_size = 700;
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
  int init_result = init(); // Wichtig, nicht ??ndern
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

    if(zeige_motor_pos){
      LOG(MessageType::DEBUG, "%d, %d", 
        position_motor_teebeutel,
        position_motor_stand);
    }

    ThisThread::sleep_for(500ms);

  } // while(true)

  return 0;
}

/**
 * @brief Pr??ft ob gegebenen String ein Befehl ist
 * Wenn ja, wird der Befehl ausgef??hrt und true zur??ckgegeben
 * Wenn nein, wird false zur??ckgegeben
 *
 * @param befehl ein string welche gepr??ft werden soll
 * @return true wenn der parameter ein befehl war
 * @return false wenn der parameter kein befehl war
 */
bool ist_befehl(string befehl) {
  // die letzte befehl von mqtt-server wird nachdem start geholt
  // die erste befehl ignorieren
  if(erste_befehl){ erste_befehl = false; return true; }

  // f??r jede gew??nschte befehl, eine neue if/else if block hinzuf??gen
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
    // die n??chste 16 zeichen an lcd zeile 1 ausgeben
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
  if(position_motor_stand != stand_pos_home) { return; }
  // teebeutel muss unten sein
  if(position_motor_teebeutel != teebeutel_unten) { return; }

  while ((int)position_motor_teebeutel < (teebeutel_unten + teebeutel_shake) ) {
    position_motor_teebeutel++;
    ThisThread::sleep_for(MOTOR_SPEED);
    motor_teebeutel = motorlauf[position_motor_teebeutel % 8]
                      << motor_teebeutel_verschiebung;
  }

  fahr_teebeutel_unten();
  show_on_lcd("Fertig");
}

void fahr_stand_zu_wegwerf_pos() {
  fahr_teebeutel_hoch();
  while ((int)position_motor_stand > stand_pos_wegwerf) {
    position_motor_stand--;
    ThisThread::sleep_for(MOTOR_SPEED);
    motor_stand = motorlauf[position_motor_stand % 8]
                  << motor_stand_verschiebung;
  }
  show_on_lcd("Fertig");
}

void fahr_stand_home() {
  fahr_teebeutel_hoch();
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
  show_on_lcd("MACH_TEE");
  fahr_teebeutel_unten();
  for (int i = 0; i < 3; i++) {
    shake_teebeutel();
  }
  show_on_lcd("MACH_TEE");
  fahr_teebeutel_hoch();
  show_on_lcd("MACH_TEE");
  fahr_stand_zu_wegwerf_pos();
  show_on_lcd("Fertig");
}

void fahr_rechts() {
  for(int i=0; i < step_size;i++){
    position_motor_stand++;
    motor_stand = motorlauf[position_motor_stand % 8]
                  << motor_stand_verschiebung;
  ThisThread::sleep_for(MOTOR_SPEED);
  }
  show_on_lcd("Fertig");
}
void fahr_links() {
  for(int i=0; i < step_size;i++){
    position_motor_stand--;
    motor_stand = motorlauf[position_motor_stand % 8]
                  << motor_stand_verschiebung;
  ThisThread::sleep_for(MOTOR_SPEED);
  }
  show_on_lcd("Fertig");
}
void fahr_oben() {
for(int i=0; i < step_size;i++){
    position_motor_teebeutel++;
    motor_teebeutel = motorlauf[position_motor_teebeutel % 8]
                      << motor_teebeutel_verschiebung;
  ThisThread::sleep_for(MOTOR_SPEED);
  } 
  show_on_lcd("Fertig"); 
}
void fahr_unten() {
for(int i=0; i < step_size;i++){
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