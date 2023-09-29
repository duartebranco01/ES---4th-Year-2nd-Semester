#include <WiFi.h>
#include <NTPClient.h>
#include <string>
#include <vector>
#include <tinyxml2.h>
using namespace tinyxml2;

#define BUTTON_PIN 16
#define SWITCH_PIN 28

#define WIFI_CONNECTION_TIMEOUT 3000
#define WIFI_RESTART_AP_BUTTON_TIME 6000
#define PAIRING_BUTTON_TIME 3000
#define QUERY_NTP_TIME 10000

class Schedule {
private:
    int scheduleID;
    String startTime;                  // Start time of the schedule (formatted string)
    String endTime;                    // End time of the schedule (formatted string)
    int dayOfWeek;                     // Day of the week where the schedule is active (0 - 6, Sunday - Saturday)
    int toggle;                        // 0 - Off, 1 - On

public:
    Schedule(int scheduleID, String startTime, String endTime, int dayOfWeek, int status) {
        this->scheduleID = scheduleID;
        this->startTime = startTime;
        this->endTime = endTime;
        this->dayOfWeek = dayOfWeek;
        this->toggle = status;
    }

    // bool isScheduleActive(String currentTime, int currentDay) const {
    //     if (currentDay == dayOfWeek) {
    //         int currentSeconds = timeStringToSeconds(currentTime);
    //         int startSeconds = timeStringToSeconds(startTime);
    //         int endSeconds = timeStringToSeconds(endTime);

    //         return (currentSeconds >= startSeconds) && (currentSeconds <= endSeconds);
    //     }
    //     return false;
    // }

    int timeStringToSeconds(String timeString) const {
        int hours = timeString.substring(0, 2).toInt();
        int minutes = timeString.substring(3, 5).toInt();
        int seconds = timeString.substring(6, 8).toInt();
        return (hours * 3600) + (minutes * 60) + seconds;
    }

    int getScheduleID() const {
      return scheduleID;
    }
    
    String getStartTime() const {
        return startTime;
    }

    String getEndTime() const {
        return endTime;
    }

    int getDayOfWeek() const {
        return dayOfWeek;
    }

    int getToggle() const {
        return toggle;
    }

    String toString() const {
        String str = "Schedule: \n"; 
        str += "ScheduleID: " + String(scheduleID) + "\n";
        str += "Start Time: " + startTime + "\n";
        str += "End Time: " + endTime + "\n";
        str += "Day of Week: " + String(dayOfWeek) + "\n";
        str += "Toggle: " + String(toggle) + "\n";

        return str;
    }
};

class ScheduleManager {
private:
    std::vector<Schedule> scheduleList;

public:
      ScheduleManager() {
        scheduleList = std::vector<Schedule>();
    }

    bool isScheduleConflict(const Schedule& newSchedule) const {
        for (const Schedule& existingSchedule : scheduleList) {
            if (newSchedule.getDayOfWeek() == existingSchedule.getDayOfWeek()) {
                int newStart = newSchedule.timeStringToSeconds(newSchedule.getStartTime());
                int newEnd = newSchedule.timeStringToSeconds(newSchedule.getEndTime());
                int existingStart = newSchedule.timeStringToSeconds(existingSchedule.getStartTime());
                int existingEnd = newSchedule.timeStringToSeconds(existingSchedule.getEndTime());

                if ((newStart <= existingEnd) && (newEnd >= existingStart)) {
                    return true;
                }
            }
        }
        return false;
    }

    int isActiveSchedule(int currentTime, int dayOfWeek) {
        
        for (const Schedule& schedule : scheduleList) {

            int scheduleStart = schedule.timeStringToSeconds(schedule.getStartTime());
            int scheduleEnd = schedule.timeStringToSeconds(schedule.getEndTime());

            if(schedule.getDayOfWeek() == dayOfWeek && currentTime >= scheduleStart && currentTime <= scheduleEnd) {
                if(schedule.getToggle() == 0) return 0;
                else if(schedule.getToggle() == 1) return 1;                
            }
        
        }

        return -1;
   
    }

    int addSchedule(const Schedule& newSchedule) {
        if (!isScheduleConflict(newSchedule)) {
            scheduleList.push_back(newSchedule);
            return 0;
        }
        return -1;
    }

    void removeScheduleFromScheduleList(const Schedule& schedule) {
      for (auto it = scheduleList.begin(); it != scheduleList.end(); ++it) {
          const Schedule& currentSchedule = *it;
          if (currentSchedule.getScheduleID() == schedule.getScheduleID() &&
              currentSchedule.getStartTime() == schedule.getStartTime() &&
              currentSchedule.getEndTime() == schedule.getEndTime() &&
              currentSchedule.getDayOfWeek() == schedule.getDayOfWeek() &&
              currentSchedule.getToggle() == schedule.getToggle()) {
              scheduleList.erase(it);
              break;
          }
      }
    }

    
    const std::vector<Schedule>& getScheduleList() const {
        return scheduleList;
    }

    String toString() const { // Change return type to String
        String str = "Schedule Manager: \n";

        for (const auto& schedule : scheduleList) {
            // Assuming Schedule class has a toString() method
            str += schedule.toString();
            str += "\n";
        }

        return str;
    }
};

typedef struct 
{
  int state, newState;

  // tes - time entering state
  // tis - time in state
  unsigned long tes, tis;
} fsm_t;

fsm_t fsmMode, fsmBlink, fsmButton, fsmSwitch, fsmSchedule;

void setState(fsm_t& fsm, int newState)
{
  if (fsm.state != newState) {  // if the state chnanged tis is reset
    fsm.state = newState;
    fsm.tes = millis(); //time entering state
    fsm.tis = 0; //time in state
  }
}

String wifiSSID = "", newWifiSSID = "";
String wifiPass = "", newWifiPass = "";

unsigned long currTime = 0, prevUpdateTimeTime = 0, blinkTimmer = 500, buttonTimeRE = 0, buttonTimeFE = 0, prevUpdateNTPTime = 0;
long buttonTimePressed = 0;

bool ledBuiltInStatus = 0, buttonInput = 0, prevButtonInput = 0, switchPinStatus = 0;
int toggleSwitch = 0, isActiveSchedule = -1;

const char* picoSSID = "Pico W1";
const char* picoPassword = "password";
IPAddress local_IP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const char* ntpServer = "pool.ntp.org";
const int32_t gmtOffset_sec = 3600; // adjust this to your time zone
const int32_t daylightOffset_sec = 0; // adjust this if your location observes DST

WiFiUDP wifiNTPClientUDP;
NTPClient timeClient(wifiNTPClientUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);
WiFiUDP receiveBroadcastUDP;
int udpPort = 8888;

WiFiServer server(80);

ScheduleManager scheduleManager;

int handledBroadcast = 0;

String timeOfDay = "00:00:00";
int dayOfWeek = 0; 

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, OUTPUT);
  
  startAP();

  server.begin();

  setState(fsmMode, 0);
  setState(fsmBlink, 0);
  setState(fsmButton, 0);
  setState(fsmSwitch, 0);
  setState(fsmSchedule, 0);

  // Schedule schedule("16:24:30", "17:25:00", 1, 1);
  // if(scheduleManager.addSchedule(schedule) == 0){
  //   Serial.println("Added schedule successfully");
  // }
  // if(scheduleManager.addSchedule(schedule) == 0){
  //   Serial.println("Added schedule successfully");
  // }
  // Serial.println(scheduleManager.toString());
}

void loop() {

  currTime = millis();

  //para dar update ao tempo tenho que ter ligaçao wifi que so tenho no mode.state>=2
  updateTimeOfDayAndDayOfWeek();
  isActiveSchedule = scheduleManager.isActiveSchedule(timeStringToSeconds(timeOfDay), dayOfWeek);


  // // Get the schedule list from the ScheduleManager
  // const std::vector<Schedule>& scheduleList = scheduleManager.getScheduleList();

  // // Print each schedule in the list
  // for (const Schedule& schedule : scheduleList) {
  //   printSchedule(schedule);
  // }

  // if(fsmMode.state>=2) {
  //     Serial.print("CurrTime:");
  //     //Serial.print(getCurrentTime());
  //     Serial.print(timeClient.getFormattedTime());
  //     Serial.print(" isActiveSchedule: "); 
  //     Serial.println(isActiveSchedule);

  // }


  prevButtonInput = buttonInput;
  buttonInput = !digitalRead(BUTTON_PIN);

  //Serial.println(buttonInput);
  //Serial.println(long(buttonTimeFE - buttonTimeRE));

  // currTime = millis(); 
  fsmMode.tis = currTime - fsmMode.tes;
  fsmBlink.tis = currTime - fsmBlink.tes;
  fsmButton.tis = currTime - fsmButton.tes;


  //fsmButton --------------------------------------------------------------------------------------------

  if(fsmButton.state == 0 && buttonInput == 1 && prevButtonInput == 0){
    buttonTimeRE = currTime;
    fsmButton.newState = 1;
    //Serial.println("fsmButton: 0->1");
  }
  else if(fsmButton.state == 1 && buttonInput == 0 && prevButtonInput == 1){
    buttonTimeFE = currTime;
    buttonTimePressed = long(buttonTimeFE - buttonTimeRE); // typcast cuz underflow
    fsmButton.newState = 0;
    //Serial.println("fsmButton: 1->0");
  }


  //fsmMode --------------------------------------------------------------------------------------------

  if(fsmMode.state == 0 && (wifiSSID != newWifiSSID || wifiPass != newWifiPass)){
    WiFi.softAPdisconnect(true);
    wifiSSID = newWifiSSID;
    wifiPass = newWifiPass;
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    blinkTimmer = 250;
    fsmMode.newState = 1;
    Serial.println("fsmMode: 0->1");
  }
  else if(fsmMode.state == 1 && WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Server port: ");
    Serial.println(server.port());
    timeClient.begin();
    receiveBroadcastUDP.begin(udpPort); 
    //timeClient.update();
    fsmMode.newState = 2;
    Serial.println("fsmMode: 1->2");
  }
  else if(fsmMode.state == 1 && fsmMode.tis >= WIFI_CONNECTION_TIMEOUT){
    blinkTimmer = 500;
    startAP();
    fsmMode.newState = 0;
    Serial.println("fsmMode: 1->0");
  }
  else if(fsmMode.state == 2 && WiFi.status() != WL_CONNECTED){
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    blinkTimmer = 250;
    fsmMode.newState = 1;
    Serial.println("fsmMode: 2->1");
  }
  else if(fsmMode.state == 2 && buttonTimePressed >= WIFI_RESTART_AP_BUTTON_TIME){ //typecast long cuz underflow quando tenho so o time do RE
    blinkTimmer = 500;
    startAP();
    fsmMode.newState = 0;
    Serial.println("fsmMode: 2->0");
  }
  else if(fsmMode.state == 2 && buttonTimePressed >= PAIRING_BUTTON_TIME){ //typecast long cuz underflow quando tenho so o time do RE
    fsmMode.newState = 3;
    Serial.println("fsmMode: 2->3");
  }
  else if(fsmMode.state == 3 && handledBroadcast == 1/*buttonTimePressed >= PAIRING_BUTTON_TIME*/){ //typecast long cuz underflow quando tenho so o time do RE
    fsmMode.newState = 2;
    handledBroadcast = 0;
    Serial.println("fsmMode: 3->2");
  }

  //fsmBlink --------------------------------------------------------------------------------------------

  if(fsmBlink.state == 0 && fsmBlink.tis >= blinkTimmer){
    fsmBlink.newState = 1;
    //Serial.println("fsmBlink: 0->1");
  }
  else if(fsmBlink.state == 0 && fsmMode.state == 2){
    fsmBlink.newState = 2;
    //Serial.println("fsmBlink: 0->2");    
  }
  else if(fsmBlink.state == 1 && fsmBlink.tis >= blinkTimmer){
    fsmBlink.newState = 0;
    //Serial.println("fsmBlink: 1->0");
  }
  else if(fsmBlink.state == 1 && fsmMode.state == 2){
    fsmBlink.newState = 2;
    //Serial.println("fsmBlink: 1->2");
  } 
  else if(fsmBlink.state == 2 && fsmMode.state != 2){
    fsmBlink.newState = 0;
    //Serial.println("fsmBlink: 2->0");
  } 

  //fsmSwitch --------------------------------------------------------------------------------------------

  if(fsmSwitch.state == 0 && (toggleSwitch == 1 || (buttonTimePressed > 0 && buttonTimePressed < PAIRING_BUTTON_TIME) || fsmSchedule.state == 1)){
    toggleSwitch = 0;
    fsmSwitch.newState = 1;
    Serial.println("fsmSwitch: 0->1");
  }
  else if(fsmSwitch.state == 1 && (toggleSwitch == 1 || (buttonTimePressed > 0 && buttonTimePressed < PAIRING_BUTTON_TIME) || fsmSchedule.state == 2 || fsmSchedule.state == 10)){ // so tenho que desligar se o schedule era obrigar a ligar
    toggleSwitch = 0;
    fsmSwitch.newState = 0;
    Serial.println("fsmSwitch: 1->0");
  }

  //fsmSchedule --------------------------------------------------------------------------------------------

    //fsm schedule so pode fazer transições que requerem ver o servidor date time apenas se existir ligação wifi, logo fsmMode.state>=2

  if(fsmSchedule.state == 0 && fsmMode.state>=2 && isActiveSchedule == 1) {
    fsmSchedule.newState = 1;
    Serial.println("fsmSchedule: 0->1");
  }
  else if(fsmSchedule.state == 1 && fsmMode.state>=2 && isActiveSchedule == -1) {
    fsmSchedule.newState = 10;
    Serial.println("fsmSchedule: 1->10");
  }
  else if(fsmSchedule.state == 1 && (toggleSwitch == 1 || (buttonTimePressed > 0 && buttonTimePressed < PAIRING_BUTTON_TIME))) {
    fsmSchedule.newState = 3;
    Serial.println("fsmSchedule: 1->3");
  }
  else if(fsmSchedule.state == 0 && fsmMode.state>=2 && isActiveSchedule == 0) {
    fsmSchedule.newState = 2;
    Serial.println("fsmSchedule: 0->2");
  }
  else if(fsmSchedule.state == 2 && fsmMode.state>=2 && isActiveSchedule == -1) {
    fsmSchedule.newState = 20;
    Serial.println("fsmSchedule: 2->20");
  }
  else if(fsmSchedule.state == 2 && (toggleSwitch == 1 || (buttonTimePressed > 0 && buttonTimePressed < PAIRING_BUTTON_TIME))) {
    fsmSchedule.newState = 3;
    Serial.println("fsmSchedule: 2->3");
  }
  else if(fsmSchedule.state == 3 && fsmMode.state>=2 && isActiveSchedule == -1) {
    fsmSchedule.newState = 0;
    Serial.println("fsmSchedule: 3->0");
  }
  else if(fsmSchedule.state == 10) {
    fsmSchedule.newState = 0;
    Serial.println("fsmSchedule: 10->0");
  }
  else if(fsmSchedule.state == 20) {
    fsmSchedule.newState = 0;
    Serial.println("fsmSchedule: 20->0");
  }










  setState(fsmButton, fsmButton.newState);
  setState(fsmMode, fsmMode.newState);
  setState(fsmBlink, fsmBlink.newState);
  setState(fsmSwitch, fsmSwitch.newState);
  setState(fsmSchedule, fsmSchedule.newState);
  //Serial.println(fsmButton.state);

  //PENSAR NISTO MELHOR: para que se perder a conecção e for do 2->0, ao inserir os dados new == old logo nunca 0->1, assim ja da 
  wifiSSID = "";
  newWifiSSID = "";
  wifiPass = ""; 
  newWifiPass = "";


  if(fsmButton.state == 0) buttonTimePressed = 0;

  if(fsmMode.state == 0) hostAPMode();
  else if(fsmMode.state == 2){ 
    
    //timeClient.update();
    /*time_t now = timeClient.getEpochTime();
    struct tm * timeInfo = localtime(&now);
    Serial.print("Current date: ");
    Serial.print(timeInfo->tm_mday);
    Serial.print("/");
    Serial.print(timeInfo->tm_mon + 1);
    Serial.print("/");
    Serial.print(timeInfo->tm_year + 1900);
    Serial.print(", ");
    Serial.print("Current time: ");
    Serial.println(timeClient.getFormattedTime());*/
    
    //handleUDPBroadcast();
    hostAppServerMode();
  }
  else if(fsmMode.state == 3){
    handleUDPBroadcast();
  }
  
  if(fsmBlink.state == 0) ledBuiltInStatus = 0;
  else if(fsmBlink.state == 1 || fsmBlink.state == 2) ledBuiltInStatus = 1;

  if(fsmSwitch.state == 0) switchPinStatus = 0;
  else if(fsmSwitch.state == 1) switchPinStatus = 1;


  digitalWrite(LED_BUILTIN, ledBuiltInStatus);
  digitalWrite(SWITCH_PIN, !switchPinStatus);

}

void startAP(){

  const char* picoSSID = "Pico W";
  const char* picoPassword = "password";
  IPAddress local_IP(192, 168, 42, 1);
  IPAddress gateway(192, 168, 42, 1);
  IPAddress subnet(255, 255, 255, 0);

  //start AP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(picoSSID, picoPassword);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

}

void hostAppServerMode(){
  //Serial.println("------------");

  // int packetUDPSize = receiveBroadcastUDP.parsePacket();
  // if(packetUDPSize > 0) Serial.println("---------------Received UDP");  

  // char packetData[255];
  // int len = receiveBroadcastUDP.read(packetData, 255);

  // if (len > 0) {
  //   packetData[len] = '\0';
  //   Serial.print("Received message: ");
  //   Serial.println(packetData);
  // }


  WiFiClient client = server.available();  
  String xml = "";
 
  if (client) {                             
    // currentTime = millis();
    // previousTime = currentTime;
    Serial.println("-------------------------------New Client.");
    
    while (client.connected() /*&& currentTime - previousTime <= 10000000*/) {
      //currentTime = millis();
      if (client.available()) {
        char c = client.read();
        xml += c;
        //Serial.print(c);
        //client.print(c);  
        //xml = client.readString();
        Serial.print(c);
      }
      
      if(xml.indexOf("</request_root>") != -1){
        // Serial.print("total xml: ");
        // Serial.println(xml);
        XMLDocument xmlDocument;

        if(xmlDocument.Parse(xml.c_str()) == XML_SUCCESS ){
        Serial.println("\nXML_SUCCESS");
        Serial.print("total xml: ");
        Serial.println(xml);
      
        XMLNode * rootNode = xmlDocument.RootElement();
        XMLElement * requestElement = rootNode->FirstChildElement("request");

        while(requestElement){
            String clientId = getXMLString(requestElement, "client_id");
            String reqType = getXMLString(requestElement, "req_type");
            String actionType = getXMLString(requestElement, "action_type");
            String scheduleID = "";
            String startTime = "";
            String endTime = "";
            String dayOfWeek = "";

            Serial.println("----------------------");
            Serial.println(clientId.c_str());
            Serial.println(reqType.c_str());
            Serial.println(actionType.c_str());


            XMLElement * scheduleElement = requestElement->FirstChildElement("schedule");
            if (scheduleElement != nullptr) {
              scheduleID = getXMLString(scheduleElement, "scheduleID");
              startTime = getXMLString(scheduleElement, "startTime");
              endTime = getXMLString(scheduleElement, "endTime");
              dayOfWeek = getXMLString(scheduleElement, "dayOfWeek");

              Serial.println(startTime.c_str());
              Serial.println(endTime.c_str());
              Serial.println(dayOfWeek.c_str());
            }
          


            //toggleSwitch
            if(reqType.toInt() == 1){
              Serial.println("reqToInt == 1");
              toggleSwitch = 1; // PARA MUDAR A MAQUINA DE ESTADOS DO SWITCH

              String reqResp;
              
              // if state == 0 it is going to change to 1, where its ON, so reqRes = ON, same for state == 1
              if(fsmSwitch.state == 0) reqResp = "ON";
              else if(fsmSwitch.state == 1) reqResp = "OFF";

              String xmlResponse;
              
              xmlResponse = " <?xml version=\"1.0\" encoding=\"UTF-8\"?>";
              xmlResponse += "<pico_response>";
              xmlResponse += "<client_id>" + clientId + "</client_id>";
              xmlResponse += "<req_status>SUCCESSFUL</req_status>";
              xmlResponse += "<req_response>" + reqResp + "</req_response>";
              xmlResponse += "</pico_response>";

              client.print(xmlResponse);  
            }
            //probe Switch Status
            else if(reqType.toInt() == 2) {

              String reqResp;
              //similiar to reqType == 1, but state is not going to change so if state == 0 its OFF, same for when state == 1
              if(fsmSwitch.state == 0) reqResp = "OFF";
              else if(fsmSwitch.state == 1) reqResp = "ON";

              String xmlResponse;
              
              xmlResponse = " <?xml version=\"1.0\" encoding=\"UTF-8\"?>";
              xmlResponse += "<pico_response>";
              xmlResponse += "<client_id>" + clientId + "</client_id>";
              xmlResponse += "<req_status>SUCCESSFUL</req_status>";
              xmlResponse += "<req_response>" + reqResp + "</req_response>";
              xmlResponse += "</pico_response>";

              client.print(xmlResponse);  
            }
            //add schedule
            else if(reqType.toInt() == 3) {

              String xmlResponse;

              Schedule schedule(scheduleID.toInt(), startTime, endTime, dayOfWeek.toInt(), actionType.toInt());
              Serial.println("Trying to add schedule: ");
              Serial.print(schedule.toString());

              if(scheduleManager.addSchedule(schedule) == 0){
                Serial.println("Added schedule sucessfully");

                xmlResponse = " <?xml version=\"1.0\" encoding=\"UTF-8\"?>";
                xmlResponse += "<pico_response>";
                xmlResponse += "<client_id>" + clientId + "</client_id>";
                xmlResponse += "<req_status>SUCCESSFUL</req_status>";
                xmlResponse += "<req_response>" + actionType + "</req_response>";
                xmlResponse += "</pico_response>";
                client.print(xmlResponse); 
              }
              else{
                Serial.println("Failed to add schedule"); 

                xmlResponse = " <?xml version=\"1.0\" encoding=\"UTF-8\"?>";
                xmlResponse += "<pico_response>";
                xmlResponse += "<client_id>" + clientId + "</client_id>";
                xmlResponse += "<req_status>FAILED</req_status>";
                xmlResponse += "<req_response>" + actionType + "</req_response>";
                xmlResponse += "</pico_response>";
                client.print(xmlResponse); 
              }
            }
            //removeSchedule
            else if(reqType.toInt() == 4) {
              String xmlResponse;

              Schedule schedule(scheduleID.toInt(), startTime, endTime, dayOfWeek.toInt(), actionType.toInt());
              Serial.println("Trying to remove schedule: ");
              Serial.print(schedule.toString());

              scheduleManager.removeScheduleFromScheduleList(schedule);

                xmlResponse = " <?xml version=\"1.0\" encoding=\"UTF-8\"?>";
                xmlResponse += "<pico_response>";
                xmlResponse += "<client_id>" + clientId + "</client_id>";
                xmlResponse += "<req_status>SUCCESSFUL</req_status>";
                xmlResponse += "<req_response>" + actionType + "</req_response>";
                xmlResponse += "</pico_response>";
                client.print(xmlResponse);

            }
            
            requestElement = requestElement->NextSiblingElement();

          }
          break; // break after sending response message, disconnects client after breaking
        }
      }


    }

    Serial.println();
    client.stop();
    Serial.println("-------------------------------Client disconnected.");
    //timeClient.update();
    //printDateTime();
  }
}

void handleUDPBroadcast() {
  int packetSize = receiveBroadcastUDP.parsePacket();

  if (packetSize) {
    Serial.print("Received udp packet of size: ");
    Serial.println(packetSize);

    char packetData[255];
    receiveBroadcastUDP.read(packetData, packetSize);

    IPAddress senderIP = receiveBroadcastUDP.remoteIP();  // Get the sender's IP address
    uint16_t senderPort = receiveBroadcastUDP.remotePort();  // Get the sender's port

    Serial.print("UDP senderIP: ");
    Serial.print(senderIP);
    Serial.print(" senderPort:");
    Serial.println(senderPort);

    // Prepare the response packet
    String response = "Microcontroller IP: " + WiFi.localIP().toString();

    // Send the response packet
    receiveBroadcastUDP.beginPacket(senderIP, senderPort);
    receiveBroadcastUDP.print(response);
    receiveBroadcastUDP.endPacket();

    Serial.print("Response sent: ");
    Serial.println(response);
    handledBroadcast = 1;
  }
}

String getXMLString(XMLElement * rootElement, const char * nodeName){

  XMLElement * nodeElement = rootElement->FirstChildElement(nodeName);
  if(nodeElement == NULL) return String("");

  String valueString(nodeElement->GetText());
   
  return valueString;
}

void hostAPMode (){

  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");

    String request = client.readStringUntil('\r');
    Serial.println(request);
    client.flush();

    //check new credentials
    if (request.indexOf("POST /") != -1) {
      String body = "";
      while(body = client.readStringUntil('\r')){
        Serial.println(body);
        if(body.indexOf("ssid=") != -1) break;
      }
      Serial.println(body);
      int ssidStart = body.indexOf("ssid=") + 5;
      int ssidEnd = body.indexOf("&");
      int passStart = body.indexOf("password=") + 9;
      int passEnd = body.indexOf("\r");
      newWifiSSID = urldecode(body.substring(ssidStart, ssidEnd));
      newWifiPass = urldecode(body.substring(passStart, passEnd));

      Serial.print("New SSID: ");
      Serial.println(newWifiSSID);
      Serial.print("New password: ");
      Serial.println(newWifiPass);

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("");
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<h1>Wi-Fi settings updated!</h1>");
      client.println("</html>");

      client.stop();
      Serial.println("Client disconnected.");

      // connect to wifi with new credentials
      // WiFi.softAPdisconnect(true);
      // WiFi.begin(newSSID.c_str(), newPass.c_str());
      // while (WiFi.status() != WL_CONNECTED) {
      //   delay(1000);
      //   Serial.print(".");
      // }

      // Serial.println("");
      // Serial.println("Wi-Fi connected");
      // Serial.print("IP address: ");
      // Serial.println(WiFi.localIP());
    }
    //send http response
    else {

      String allScannedNetworks = scanNetworks();

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("");
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<h1>Update Wi-Fi Settings</h1>");
      client.println("<form method='POST'>");
      client.println("<label>SSID:</label><br>");
      client.println("<input type='text' name='ssid'><br>");
      client.println("<label>Password:</label><br>");
      client.println("<input type='password' name='password'><br><br>");
      client.println("<input type='submit' value='Update'>");
      client.println("</form>");

      // Display scanned networks
      client.println("<h2>Scanned Networks (SSID, ENC, BSSID, CH, RSSI)</h2>");
      client.println("<ul>");
      // Split the allScannedNetworks string into individual network strings
      int networkCount = 0;
      int separatorIndex = allScannedNetworks.indexOf(";");
      while (separatorIndex >= 0) {
        String networkString = allScannedNetworks.substring(0, separatorIndex);
        allScannedNetworks = allScannedNetworks.substring(separatorIndex + 1);
        separatorIndex = allScannedNetworks.indexOf(";");
        networkCount++;

        // Split the individual network string into its components
        String networkComponents[5];
        int componentIndex = networkString.indexOf(",");
        int componentCount = 0;
        while (componentIndex >= 0) {
          networkComponents[componentCount] = networkString.substring(0, componentIndex);
          networkString = networkString.substring(componentIndex + 1);
          componentIndex = networkString.indexOf(",");
          componentCount++;
        }
        networkComponents[componentCount] = networkString;

        // Print the network details as a list item
        client.print("<li>");
        client.print(networkComponents[0]);  // SSID
        client.print(", ");
        client.print(networkComponents[1]);  // Encryption
        client.print(", ");
        client.print(networkComponents[3]);  // Channel
        client.print(", ");
        client.print(networkComponents[4]);  // RSSI
        client.print("</li>");
      }
      client.println("</ul>");

      client.println("</html>");

      //close connection
      client.stop();
      Serial.println("Client disconnected.");
    }
  }
}


String urldecode(const String& url) {
  String decoded = "";

  for (size_t i = 0; i < url.length(); ++i) {
    if (url[i] == '+') {
      decoded += ' ';
    }
    else if (url[i] == '%' && i + 2 < url.length()) {
      String hex = url.substring(i + 1, i + 3);
      decoded += static_cast<char>(strtol(hex.c_str(), NULL, 16));
      i += 2;
    }
    else {
      decoded += url[i];
    }
  }

  return decoded;
}

String scanNetworks(){

  String allScannedNetworks = "";

  Serial.printf("Beginning scan at %lu\n", currTime);
  int cnt = WiFi.scanNetworks();
  if (!cnt) {
    Serial.printf("No networks found\n");
  } 
  else {
    Serial.printf("Found %d networks\n\n", cnt);
    //Serial.printf("%32s %5s %17s %2s %4s\n", "SSID", "ENC", "BSSID        ", "CH", "RSSI");
    for (int i = 0; i < cnt; i++) {
      uint8_t bssid[6];
      WiFi.BSSID(i, bssid);
      //Serial.printf("%32s %5s %17s %2d %4ld\n", WiFi.SSID(i), encToString(WiFi.encryptionType(i)), macToString(bssid), WiFi.channel(i), WiFi.RSSI(i));
    
      String scannedNetwork = String(WiFi.SSID(i)) + "," + encToString(WiFi.encryptionType(i)) + "," + macToString(bssid) 
                              + "," + String(WiFi.channel(i)) + "," + String(WiFi.RSSI(i));
      allScannedNetworks += scannedNetwork + ";";

      Serial.println(scannedNetwork);
    }
  }

  return allScannedNetworks;
}

String macToString(uint8_t mac[6]) {
  char s[20];
  sprintf(s, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(s);
}

String encToString(uint8_t enc) {
  switch (enc) {
    case ENC_TYPE_NONE: return "NONE";
    case ENC_TYPE_TKIP: return "WPA";
    case ENC_TYPE_CCMP: return "WPA2";
    case ENC_TYPE_AUTO: return "AUTO";
  }
  return "UNKN";
}

void printDateTimeNTP(){
    
  //timeClient.update();
  time_t now = timeClient.getEpochTime();
  struct tm * timeInfo = localtime(&now);
  
  Serial.print("Current date: ");
  Serial.print(timeInfo->tm_mday);
  Serial.print("/");
  Serial.print(timeInfo->tm_mon + 1);
  Serial.print("/");
  Serial.print(timeInfo->tm_year + 1900);
  Serial.print(", Day of week: ");
  Serial.print(timeInfo->tm_wday);
  Serial.print(", Current time: ");
  Serial.println(timeClient.getFormattedTime());
}

int getDayOfWeekNTP() {

  //timeClient.update();
  time_t now = timeClient.getEpochTime();
  struct tm * timeInfo = localtime(&now);
  
  int dayOfWeek = timeInfo->tm_wday;
  
  return dayOfWeek;
}

String getCurrentTimeNTP() {
  
  //timeClient.update();
  String formatedTime = timeClient.getFormattedTime();

  return formatedTime;
}

int timeStringToSeconds(String timeString) {
    int hours = timeString.substring(0, 2).toInt();
    int minutes = timeString.substring(3, 5).toInt();
    int seconds = timeString.substring(6, 8).toInt();
    return (hours * 3600) + (minutes * 60) + seconds;
}



void printSchedule(const Schedule& schedule) {
  Serial.print("Start Time: ");
  Serial.print(schedule.getStartTime());
  Serial.print(" | End Time: ");
  Serial.print(schedule.getEndTime());
  Serial.print(" | Day of Week: ");
  Serial.print(schedule.getDayOfWeek());
  Serial.print(" | Status: ");
  Serial.println(schedule.getToggle());
}

String incrementTimeBySeconds(const String& timeString, int secondsToAdd) {
    // Extract hours, minutes, and seconds from the time string
    int hours, minutes, seconds;
    sscanf(timeString.c_str(), "%d:%d:%d", &hours, &minutes, &seconds);

    // Convert the time to seconds
    int totalSeconds = hours * 3600 + minutes * 60 + seconds;

    // Increment the time by the specified seconds
    totalSeconds += secondsToAdd;

    // Adjust the time if necessary
    if (totalSeconds >= 86400) {
        totalSeconds %= 86400;
    }

    // Calculate the hours, minutes, and seconds from the total seconds
    hours = totalSeconds / 3600;
    minutes = (totalSeconds % 3600) / 60;
    seconds = totalSeconds % 60;

    // Create and return the updated time string
    char buffer[9];
    sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
    return String(buffer);
}

int updateDayOfWeek(const String& timeString) {
   
    if(timeString != "23:59:59"){
      return dayOfWeek;
    } 
    else if (dayOfWeek == 6) { //saturday->sunday
        return 0;
    } 
    else {
        return dayOfWeek + 1; 
    }

}

void updateTimeOfDayAndDayOfWeek(){


  int passedTimeInSeconds = (currTime - prevUpdateTimeTime)/1000;


  if(passedTimeInSeconds > 0){
    timeOfDay = incrementTimeBySeconds(timeOfDay, passedTimeInSeconds);
    dayOfWeek = updateDayOfWeek(timeOfDay);
    Serial.print("Time: ");
    Serial.print(timeOfDay);
    Serial.print(" Day of week: ");
    Serial.print(dayOfWeek);
    Serial.println();


    prevUpdateTimeTime = currTime;
  }

  

  if(fsmMode.state>=2 && currTime - prevUpdateNTPTime >= QUERY_NTP_TIME){

    if (timeClient.update()) {
      timeOfDay = getCurrentTimeNTP();
      dayOfWeek = getDayOfWeekNTP();
    
    Serial.print("Time updated with NTP. ");
    Serial.print("Time: ");
    Serial.print(timeOfDay);
    Serial.print(" Day of week: ");
    Serial.print(dayOfWeek);
    Serial.println();
  
    }
    prevUpdateNTPTime = currTime;
  }
  


}
