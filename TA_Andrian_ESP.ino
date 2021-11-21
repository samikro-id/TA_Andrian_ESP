#include <ESP8266WiFi.h>                  // Install esp8266 by ESP8266 Community version 2.6.3
#include <PubSubClient.h>                 // Install Library by Nick O'Leary version 2.7.0

WiFiClient espClient;
PubSubClient client(espClient);

uint32_t current_time;
uint32_t previous_time;
uint32_t chart_time;
uint32_t led_time;

bool led_state = false;

#define PAYLOAD_LEN  100
char payload_get[PAYLOAD_LEN];
bool mqtt = false;

#define SERIAL_LEN   1000
char text[SERIAL_LEN];

char ssid[] = "Paijo";
char pass[] = "isisembarang";

typedef struct{
    float voltage = 0.0;
    float current = 0.0;
    float power = 0.0;
    float energy = 0.0;
    float frequency = 0.0;
    float pf = 0.0;
}PzemData_Struct;

typedef struct{
    float suhu;
    PzemData_Struct input;
    PzemData_Struct output;
    bool source_pln;
    bool proteksi_on;
    bool fan_on;
    bool suhu_on;
    bool voltage_low;
}SensorData_Struct;

#define CHART_DELAY     300000      // 300000 = 5 menit

#define MQTT_ID         "ddfdf6cd-f3eb-4936-a8cb-440ff3518b99"

#define MQTT_BROKER     "broker.emqx.io"            //
#define MQTT_PORT       1883                        //
#define MQTT_USERNAME   ""                          // Change to your Username from Broker
#define MQTT_PASSWORD   ""                          // Change to your password from Broker
#define MQTT_TIMEOUT    10
#define MQTT_QOS        0
#define MQTT_RETAIN     false

#define MQTT2_BROKER    "mqtt.thingspeak.com"      //
#define MQTT2_PORT      1883                       //
#define MQTT2_USERNAME  "mwa0000023509738"         // Change to your Username from thingspeak
#define MQTT2_PASSWORD  "GWHQ75HUKGBKYXDI"         // Change to your User API Key from thingspeak
#define MQTT2_TIMEOUT   10
#define MQTT2_QOS       0
#define MQTT2_RETAIN    false
#define PUBLISH_SUBTOPIC  "NLA5486CXZ5GGICQ"       // Change to your Write API Key from thingspeak
#define CHANNEL_ID      1476079

void callback(char* topic, byte* payload, unsigned int length) { //A new message has been received
  memcpy(payload_get, payload, length);
  mqtt = true;
}

/**
 *  enum rst_info->reason {
 *    0 : REASON_DEFAULT_RST, normal startup by power on
 *    1 : REASON_WDT_RST, hardware watch dog reset 
 *    2 : REASON_EXCEPTION_RST, exception reset, GPIO status won't change 
 *    3 : REASON_SOFT_WDT_RST, software watch dog reset, GPIO status won't change 
 *    4 : REASON_SOFT_RESTART, software restart ,system_restart , GPIO status won't change 
 *    5 : REASON_DEEP_SLEEP_AWAKE, wake up from deep-sleep 
 *    6 : REASON_EXT_SYS_RST, external system reset 
 *  };
 */
rst_info *myResetInfo;

void setup(){
  delay(300);
  Serial.begin(9600);

  initLed();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);                            delay(10);
  ESP.wdtFeed();                                  yield();

  led_time = millis();

  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    yield();

    if((millis()-led_time) > 1000){          // toggle led setiap detik
        led_time = millis();
        
      toggleLed();
    }

    if(WiFi.status() == WL_CONNECTED){
      break;
    }
  }
  
  if(WiFi.status() == WL_CONNECTED){
    connectMqtt();
  }

  chart_time = millis() + CHART_DELAY;
  led_time = millis();
}

void loop(){
  if(WiFi.status() == WL_CONNECTED){

    if((millis() - led_time) > 200){
      toggleLed();

      led_time = millis();
    }

    if((millis()-chart_time) >= CHART_DELAY){
      chart_time = millis();

      publishChart();
    }

    if(mqtt){
      mqtt = false;
      
      uint8_t n=0;
      for(n=0; n<3; n++){
        Serial.println(payload_get);
        if(waitSerialApp()){
          break;
        }
      }
      
      clearDataMqtt();
    }
  }

  client.loop();
}

void publishChart(){
  bool chartIsConnected = false;
  uint8_t n = 0;

  client.disconnect();
  client.setServer(MQTT2_BROKER, MQTT2_PORT);

  for(n=0; n<MQTT2_TIMEOUT; n++){

    if(client.connect(MQTT_ID, MQTT2_USERNAME, MQTT2_PASSWORD)){
      chartIsConnected = true;

      break;
    }
  }

  if(chartIsConnected){
    for(n=0; n<3; n++){
      Serial.println("GET|DATA");
      if(waitSerialChart()){
        break;
      }
    }
  }

  connectMqtt();
}

bool connectMqtt(){
  client.disconnect();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
  mqtt = false;

  uint8_t i;
  for(i = 0; i < MQTT_TIMEOUT; i++) {
      
    if( client.connect(MQTT_ID, MQTT_USERNAME, MQTT_PASSWORD) ){
      delay(500);
      if(client.subscribe("samikro/cmd/project/3", MQTT_QOS)){
        return true;
        break;
      }
    }

    delay(500);                                 
  }
  return false;
}

void clearDataMqtt(){
  uint8_t n;
  for(n=0; n<PAYLOAD_LEN; n++){
    payload_get[n] = 0;
  }
}

void clearDataSerial(){
  uint16_t n;
  for(n=0; n<SERIAL_LEN; n++){
    text[n] = 0;
  }
}

/***** Serial ******/
bool waitSerialChart(){
  float field1=0;
  float field2=0;
  float field3=0;
  float field4=0;
  float field5=0;
  float field6=0;
  float field7=0;
  SensorData_Struct sensor;

  if(waitSerial()){
    parseData(&sensor);
    
    field2 = sensor.output.voltage;
    
    field4 = sensor.output.current;
    field5 = sensor.suhu;

    if(sensor.source_pln){
      field1 = sensor.input.voltage;
      field3 = sensor.input.current;
      
      field6 = 0;
      field7 = 0;
    }
    else{
      field6 = sensor.input.voltage;
      field7 = sensor.input.current;
      
      field1 = 0;
      field3 = 0;
    }

    clearDataSerial();
    sprintf(text,"field1=%.1f&field2=%.1f&field3=%.2f&field4=%.2f&field5=%.1f&field6=%.2f&field7=%.1f", 
            field1, field2, field3, field4, field5, field6, field7);

    char topic[50];
    memset(&topic, 0, 50);
    sprintf(topic,"channels/%d/publish/%s", CHANNEL_ID, PUBLISH_SUBTOPIC);
    client.publish(topic,text,false);

    clearDataSerial();
    return true;
  }
  return false;
}

void parseData(SensorData_Struct *sensor){
    int index, index2;

    index = findChar(text, '|', 0);

    // Serial.println(charToString(text, 0, index-1));

    if(charToString(text, 0, index-1) == "DATA"){
      String val = "";

      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get suhu
      sensor->suhu = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get input voltage
      sensor->input.voltage = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get input current
      sensor->input.current = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get input power
      sensor->input.power = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get input frequency
      sensor->input.frequency = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get input pf
      sensor->input.pf = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get input energy
      sensor->input.energy = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get output voltage
      sensor->output.voltage = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get output current
      sensor->output.current = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get output power
      sensor->output.power = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get output frequency
      sensor->output.frequency = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get output pf
      sensor->output.pf = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get output energy
      sensor->output.energy = val.toFloat();

      index = index2;
      index++;
      index2 = findChar(text, '|', index);
      val = charToString(text, index, index2 - 1);    // get source flag
      sensor->source_pln = (val.toInt() == 1)? true : false;
    }
}

int findChar(char * data, char character, int start_index){
  int n;
  for(n=start_index; n < strlen(data); n++){
    if(data[n] == character){
      break;
    }
  }

  return n;
}

String charToString(char * data, int start, int end){
  String buff = "";
  for(int n=start; n<=end; n++){
    buff += data[n];
  }

  return buff;
}

bool waitSerialApp(){
  if(waitSerial()){
    client.publish("samikro/data/project/3", text,false);
    clearDataSerial();
    return true;
  }
  return false;
}

bool waitSerial(){
  uint16_t n = 0;
  bool hasData = false;

  clearDataSerial();

  previous_time = millis();
  do{
    if(Serial.available() > 0){
      delay(10);
      break;
    }
  }while((millis() - previous_time) <= 1000);

  while(Serial.available() > 0){
    byte d = (char) Serial.read();
    if(d == '\n'){  hasData = true; }
    else if(d == '\r'){ }
    else{ 
        text[n] = d;
        n++;  
    }
    
    delay(1);
  }

  return hasData;
}

/***** LED Setting *****/

void initLed(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);      // default mati
  led_state = false;
}

void toggleLed(){
  if(led_state == false){
    digitalWrite(LED_BUILTIN, LOW);     // nyalakan LED
    led_state = true;
  }else{
    digitalWrite(LED_BUILTIN, HIGH);    // matikan LED
    led_state = false;
  }
}

void setLed(bool state){
  if(state){    digitalWrite(LED_BUILTIN, LOW);     }// nyalakan LED
  else{    digitalWrite(LED_BUILTIN, HIGH);    }// matikan LED

  led_state = state;
}