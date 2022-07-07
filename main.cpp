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
ESP8266Interface wifi(PC_12, PD_2); // ANPASSEN: bitte die PINS for WLAN Module setzen (TX, RX)

// extra thread for mqtt
Thread mqtt_thread;

// socket address
SocketAddress socket_address;

// MQTT
#include <MQTTNetwork.h>
#include <MQTTClient.h>
#include <MQTTmbed.h> // Countdown
const std::string MQTT_SERVER = "192.168.43.1";  // ANPASSEN: MQTT-Server domain oder IP
const unsigned int MQTT_PORT = 1883;
const std::string MQTT_USER = "";  // ANPASSEN: MQTT-Benutzername, falls erforderlich
const std::string MQTT_PASSWORD = "";  // ANPASSEN: MQTT-Passwort, falls erforderlich
const std::string MQTT_TOPIC = "mbed/cmd";  // ANPASSEN: MQTT-Topic
std::string mqtt_client_name = "esp8266_01";  // ANPASSEN: Client Name, womit diese app sich bei MQTT-Server identifiziert
const MQTT::QoS MQTT_SUB_QOS = MQTT::QOS2;
MQTTPacket_connectData MQTT_CONNECTION = MQTTPacket_connectData_initializer;
unsigned int mqtt_messages_received = 0;
MQTTNetwork mqttNet(&wifi);
MQTT::Client<MQTTNetwork, Countdown> mqtt_client(mqttNet);

// Allgemein Control
unsigned int anzahl_fehler = 0;

// Funktionen
void check_mqtt_message();
void process_incoming_mqtt_message(MQTT::MessageData& md);
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
  while(true) {
    mqtt_client.yield();
    ThisThread::sleep_for(YIELD_TIME);
  }
}

void process_incoming_mqtt_message(MQTT::MessageData& md) {
  MQTT::Message &message = md.message;
  mqtt_messages_received++;
  int msg_length = message.payloadlen;

  std::string mqtt_message = (char*)message.payload;
  mqtt_message = mqtt_message.substr(0, msg_length);
  if(!ist_befehl(mqtt_message)) {
    LOG(MessageType::INFO, "MSG RCVD: {%d, %d, %s}", mqtt_messages_received, msg_length, mqtt_message.c_str());
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
  status = wifi.connect(MBED_CONF_NSAPI_DEFAULT_WIFI_SSID, MBED_CONF_NSAPI_DEFAULT_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
  if (status == 0) {
    wifi.set_as_default();
    LOG(MessageType::INFO, "WLAN Verbindung hergestellt");
    wifi.get_ip_address(&socket_address);
    LOG(MessageType::INFO, "IP Adresse: %s", socket_address.get_ip_address());
  } else {
    LOG(MessageType::ERROR, "WLAN Verbindung konnte nicht hergestellt werden, Status: %d", status);
    return status;
  }
  return status;
}

int do_dns_lookup() {
  // MQTT Server DNS Lookup
  int status = -1;
  status = wifi.gethostbyname(MQTT_SERVER.c_str(), &socket_address);
  if(status == 0){
    LOG(MessageType::INFO, "DNS Query Result of [%s]: %s", MQTT_SERVER.c_str(), socket_address.get_ip_address());
  } else {
    LOG(MessageType::ERROR, "DNS Query failed for [%s], status: %d", MQTT_SERVER.c_str(), status);
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

void show_mqtt_options(){
    LOG("");
    LOG("Client Name: %s", MQTT_CONNECTION.clientID.cstring );
    LOG("Cleansession: %d", MQTT_CONNECTION.cleansession );
    LOG("Mqtt Version: %d", MQTT_CONNECTION.MQTTVersion );
    LOG("Mqtt Struct Version: %d", MQTT_CONNECTION.struct_version );    
    LOG("Mqtt User: %s", MQTT_CONNECTION.username.cstring );
    LOG("Mqtt Pass: %s", MQTT_CONNECTION.password.cstring );
    LOG("");
}

int connect_to_mqtt_server() {
  // MQTT Client Verbindung herstellen
  int status = -1;
  status = mqttNet.connect(MQTT_SERVER.c_str(), MQTT_PORT); //tcp_socket.connect(socket_address);
  status = mqtt_client.connect(MQTT_CONNECTION);
  if(status == 0){
    LOG(MessageType::INFO, "MQTT Client Verbindung hergestellt");
  } else {
    LOG(MessageType::ERROR, "MQTT Client Verbindung konnte nicht hergestellt werden, Status: %d", status);
    return status;
  }
  return status;
}

int subscribe_to_mqtt_topic() {
  // MQTT Topic Abonieren
  int status = mqtt_client.subscribe(MQTT_TOPIC.c_str(), MQTT_SUB_QOS, process_incoming_mqtt_message);
  if(status == 0){
    LOG(MessageType::INFO, "MQTT Topic [%s] abonniert", MQTT_TOPIC.c_str());
  } else {
    LOG(MessageType::ERROR, "MQTT Topic konnte nicht abonniert werden, Status: %d", status);
    return status;
  }
  return status;
}

void restart_controller() {
  NVIC_SystemReset();
}



int init() {

  LOG(MessageType::INFO, "Setup started...");
  int status = 0;

  LOG("Connecting to Wifi");
  status = execute_step(connect_to_wifi, 10);
  if(status != 0) return false;
  
  LOG("Resolving Mqtt-Host to IP-Address");
  status = execute_step(do_dns_lookup);
  if(status != 0) return false ;

  LOG("Setting Mqtt-Connection Parameters");
  status = execute_step(initialize_mqtt_client);
  if(status != 0) return false ;
  //show_mqtt_options();

  LOG("Connecting to Mqtt Server");
  status = execute_step(connect_to_mqtt_server, 1);
  if(status != 0) return false ;

  LOG("Subscribing to Mqtt-Topic: %s", MQTT_TOPIC.c_str());
  status = execute_step(subscribe_to_mqtt_topic);
  if(status != 0) return false ;

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
BITTE NUR DASS ÄNDERN WAS SIE WISSEN WAS UND WARUM SIE ES ÄNDERN
******************************************************************/


  return 0;
}
/******************************************************************
*        MAIN FUNKTION/ ENTRY-POINT                               *
******************************************************************/
// Variables
enum Zustand{STOP, FaHR_VOR, FAHR_ZURUEK, DREHE_RECHTS, DREHE_LINKS};
volatile Zustand aktueller_zustand = STOP;
std::string aktueller_befehl = "0";

// ALLE LEDS AN MF-SHIELD
bool leds_aktiv = false;
#define LED_REFRESH_TIME 200ms
DigitalOut leds[] = {PA_5, PA_6, PA_7, PB_6};
const int LED_AN = 0;
const int LED_AUS = 1;

#include "LCD.h"
lcd tm_lcd;
bool lcd_aktiv = true;

// Motoren
PortOut motor1(PortC,0b1111); // C 0-3
int motor1_verschiebung = 0;
PortOut motor2(PortC,0b111100000000); // C 8-11
int motor2_verschiebung = 8;
bool ist_motor1_aktiv = true;
int motorlauf[]={0b0001,0b0011,0b0010,0b0110,0b0100,0b1100,0b1000,0b1001};
unsigned int position_motor1 = 0;
unsigned int position_motor2 = 0;
Ticker motor_steurung_ticker;
//Ticker motor2_ticker;
#define MOTOR_SPEED 1ms

// control vars
int stand_pos_home = 0;
int stand_pos_wegwerf = 5100;
int teebeutel_home = 0;
int teebeutel_unten = 4000;
int teebeutel_jiggle = 1500;

// Methoden
void show_on_lcd(const char* msg);
void einschalte_led(int which_led);
void do_set_zustand(Zustand zustand);

void fahr_teebeautel_unten();
void fahr_teebeautel_home();
void jiggle_teebeautel();
void fahr_stand_zu_wegwerf_pos();
void fahr_stand_home();

void do_stop();
void do_fahr_vor();
void do_fahr_zuruek();
void do_drehe_rechts();
void do_drehe_links();
void do_motor_steurung();


int main() {
  show_on_lcd("Tee Maschine v2");
  int init_result = init(); // Wichtig, nicht ändern
  if(init_result == 0) { 

    tm_lcd.clear();

    mqtt_thread.start(check_mqtt_message); // MQTT Verbindung in eigner Thread
    LOG(MessageType::INFO, "Init Erfolgreich");
    show_on_lcd("Init Erfolgreich");
    motor_steurung_ticker.attach(&do_motor_steurung, MOTOR_SPEED);
  
  } else { // init war nicht erfolgreicht

      LOG(MessageType::ERROR, "Init fehlgeschlagen");
      LOG(MessageType::ERROR, "Mikrocontroller wird in 5 Sekunden neugestartet...");
      show_on_lcd("Init fehlschlag!Neustart in 5S");
      ThisThread::sleep_for(5s); // warte 5 sekunden
      restart_controller(); // MC neustarten
  
  } // if(init_result == 0)

  while(true) {

      LOG(MessageType::DEBUG, "%d, %d", position_motor1, position_motor2);
      ThisThread::sleep_for(100ms);

  }

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
  // für jede gewünschte befehl, eine neue if/else if block hinzufügen
  // in der if/else if block, eure Function aufrufen und dann return true machen
  if(befehl == aktueller_befehl){
      return true;
  } else if(befehl == "0"){

      LOG(MessageType::INFO, "Teebeutel geht unten");
      show_on_lcd("Teebeutel unten");
      fahr_teebeautel_unten();
      return true;

  } else if(befehl == "1"){
      
      LOG(MessageType::INFO, "Teebeutel geht home");
      show_on_lcd("Teebeutel home");
      fahr_teebeautel_home();
      return true;

  } else if(befehl == "2"){
      
      LOG(MessageType::INFO, "Stand geht zur Wegwerf Position");
      show_on_lcd("Stand > Wegwerf");
      fahr_stand_zu_wegwerf_pos();
      return true;

  } else if(befehl == "3"){
      
      LOG(MessageType::INFO, "Stand geht Home");
      show_on_lcd("Stand > Home");
      fahr_stand_home();
      return true;

  } else if(befehl == "4"){
      
      LOG(MessageType::INFO, "shake it baby...");
      show_on_lcd("Jiggle ..");
      jiggle_teebeautel();
      return true;

  } else if(befehl == "5"){
      
      LOG(MessageType::INFO, "Alles home");
      show_on_lcd("All Home");
      fahr_stand_home();
      fahr_teebeautel_home();
      return true;

  } else if(befehl == "6"){
      
      LOG(MessageType::INFO, "Befehl: %s ist noch unbesetzt", befehl.c_str());
      return true;

  }

  return false;
}

void show_on_lcd(const char* msg) {

    if(!lcd_aktiv){ return; }

    tm_lcd.clear();
    if(strlen(msg) <= 16) {
        tm_lcd.cursorpos(0);
        tm_lcd.printf("%s", msg);
    } else {
        char print_msg[16];
        // die erste 16 zeichen an lcd zeile 1 ausgeben
        tm_lcd.cursorpos(0);
        memcpy(print_msg, &msg[0], 16);
        tm_lcd.printf("%s", msg);
        // die nächste 16 zeichen an lcd zeile 1 ausgeben
        tm_lcd.cursorpos(64);
        memcpy(print_msg, &msg[16], 16);
        tm_lcd.printf("%s", msg);
    }
}

void einschalte_led(int which_led) {
    if(!leds_aktiv){return;};

    if(which_led <= 4 && which_led >= 0) {
        for(int i = 0; i < 4; i++ ){
            leds[i]  = LED_AUS;
        }
        if(which_led >= 1){
            leds[which_led - 1] = LED_AN;
        }
    }
}

void do_set_zustand(Zustand zustand) {
    aktueller_zustand = zustand;
    einschalte_led(zustand);
}

void do_stop() {/* nichts zu tun */ }

void do_fahr_vor() {    position_motor1++;}

void do_fahr_zuruek() {    position_motor1--;}

void do_drehe_rechts() {    position_motor2++;}

void do_drehe_links() {    position_motor2--;}

void do_motor_steurung() {
    motor1 = motorlauf[position_motor1%8]<<motor1_verschiebung;
    motor2 = motorlauf[position_motor2%8]<<motor2_verschiebung;

    switch(aktueller_zustand) {
        case STOP: do_stop(); break;
        case FaHR_VOR: do_fahr_vor(); break;
        case FAHR_ZURUEK: do_fahr_zuruek(); break;
        case DREHE_RECHTS: do_drehe_rechts(); break;
        case DREHE_LINKS: do_drehe_links(); break;
    }
}

void fahr_teebeautel_unten() {
    while(position_motor1 < teebeutel_unten) {
        position_motor1++;
        ThisThread::sleep_for(MOTOR_SPEED);
        motor1 = motorlauf[position_motor1%8]<<motor1_verschiebung;
    }
}

void fahr_teebeautel_home() {
    while(position_motor1 > teebeutel_home) {
        position_motor1--;
        ThisThread::sleep_for(MOTOR_SPEED);
        motor1 = motorlauf[position_motor1%8]<<motor1_verschiebung;
    }
}

void jiggle_teebeautel() {
    while(position_motor1 > teebeutel_jiggle) {
        position_motor1--;
        ThisThread::sleep_for(MOTOR_SPEED);
        motor1 = motorlauf[position_motor1%8]<<motor1_verschiebung;
    }

    while(position_motor1 < teebeutel_unten) {
        position_motor1++;
        ThisThread::sleep_for(MOTOR_SPEED);
        motor1 = motorlauf[position_motor1%8]<<motor1_verschiebung;
    }
}

void fahr_stand_zu_wegwerf_pos() {
    while(position_motor2 < stand_pos_wegwerf) {
        position_motor2++;
        ThisThread::sleep_for(MOTOR_SPEED);
        motor2 = motorlauf[position_motor2%8]<<motor2_verschiebung;
    }
}

void fahr_stand_home() {
    while(position_motor2 < stand_pos_home) {
        position_motor2--;
        ThisThread::sleep_for(MOTOR_SPEED);
        motor2 = motorlauf[position_motor2%8]<<motor2_verschiebung;
    }
}
