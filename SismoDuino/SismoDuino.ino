/**
 *  __ _                       ___       _             
 * / _(_)___ _ __ ___   ___   /   \_   _(_)_ __   ___  
 * \ \| / __| '_ ` _ \ / _ \ / /\ / | | | | '_ \ / _ \ 
 * _\ \ \__ \ | | | | | (_) / /_//| |_| | | | | | (_) |
 * \__/_|___/_| |_| |_|\___/___,'  \__,_|_|_| |_|\___/ 
 * 
 * 
 * SismoDuino Copyright (C) 2016 xionbig
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * @author xionbig
 * @link https://github.com/xionbig/SismoDuino
 */
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "RTClib.h"
#include <ADXL345.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>

ADXL345 accelerometro;
RTC_DS1307 rtc;

int PIN_BTN1 = A0; //buttone on/off letto con la porta analogica
int PIN_BTN2 = 16; //buttone on/off allarme
int PIN_BUZZ = 0; // buzzer 
int PIN_LED = 2; //pin led dello stato
int volume = 0; //volume dell'allarme
bool direction_vol = true; //direzione volume, se è true incrementa

DateTime ora; //ora corrente del sensore RTC
int refresh_config = 0; //quando(data) è stata caricata la configurazione

bool stats = true; //se il sistema è acceso(true)
bool allarm = true; //se l'allarme è accesso(true)
bool in_allarm = false; //se l'allarme sta suonando
bool in_error = false; //se è presente qualche errore nel sistema allora il led di accensione lampeggia
int time_allarm = 0; //la data in cui è stato attivato l'allarme 
//config
int my_id = ESP.getChipId();
int wifi_mode = 2;
String ssid = "SismoDuino-WiFi";
String password = "sismoduino";
bool dhcp = true;
int ip[] = {192, 168, 1, 4};
int gateway[] = {192, 168, 1, 1};
int subnet_mask[] = {255, 255, 255, 0};
String server_query = "sismoduino.altervista.org";
String user = "guest";
String pwd = "guest";
double lat = 0.0;
double lon = 0.0;

//accelerometro
double x = 0.00; //valori prelevati in tempo reale
double y = 0.00;
double z = 0.00;
int count_acc = 0; //contatore accelerometro, se è 3 allora fa la media e controlla con i valori calibrati
double last_x = 0.00;  //valori su cui si verifica se c'è un'accelerazione
double last_y = 0.00;
double last_z = 0.00;
double g_x = 0.00; //forze g calcolate tramite l'accelerazione
double g_y = 0.00;
double g_z = 0.00;
double g_tot = 0.00;

//WiFi
ESP8266WebServer server(80);
bool wifi_started1 = false;
bool wifi_started2 = false;

void setup() {  
  Serial.begin(9600);
  pinMode(PIN_BTN2, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  
  analogWrite(PIN_LED, 20);
  analogWrite(PIN_BUZZ, 0);

  if(!SD.begin()){
    Serial.println("Ci sono problemi con la memoria SD!");
    in_error = true;
    return;
  }
  if(!rtc.begin()) {
    Serial.println("Il sensore RealTimeClock non è stato trovato!");
    in_error = true;
    return;
  }
  if(!rtc.isrunning()){
    Serial.println("Il sensore RealTimeClock non è in esecuzione!");
    in_error = true;
    return;
  }  
  if(!accelerometro.begin()){
    Serial.println("il sensore ADX345(accelerometro) non è stato trovato!");
    delay(500);
  }
  accelerometro.setRange(ADXL345_RANGE_16G);
  Serial.println("La memoria SD e' stata letta con successo");
  delay(1000);
  analogWrite(PIN_LED, 120);
}
//funzione che accende il buzzer e il led quando l'allarme è attivato
void inAllarm(){  
  if(direction_vol && volume <= 20)
    volume++;
  if(!direction_vol && volume > 3)
    volume--;
  if(direction_vol && volume >= 20)
    direction_vol = false;
  if(!direction_vol && volume <= 3)
    direction_vol = true;
  analogWrite(PIN_BUZZ, volume*10);
  Serial.print("Volume = ");
  Serial.println((volume*10));
}
//Imposta se il sistema è acceso o è spento
void setStatus(bool var){
  stats = var;
  if(stats){
    analogWrite(PIN_LED, 100);
    Serial.println("ON");
    ESP.restart();
  }else{
    analogWrite(PIN_LED, 0);
    Serial.println("OFF");
    in_allarm = false;
    allarm = false;     
  }  
  delay(200);   
}
//Imposta se l'allarme deve suonare oppure no
void setAllarm(bool var){
  allarm = var;
  if(!allarm){
    Serial.println("Allarm OFF");
    digitalWrite(PIN_BUZZ, HIGH);
    delay(50);
    digitalWrite(PIN_BUZZ, LOW);
    delay(50);
    digitalWrite(PIN_BUZZ, HIGH);
    delay(100);
  }else
    Serial.println("Allarm ON");
  digitalWrite(PIN_BUZZ, HIGH);
  delay(100);
  digitalWrite(PIN_BUZZ, LOW);
}
//Legge la configurazione e la memorizza nell'array
bool loadConfig(){  
  Serial.println("Carico la configurzione...");
  if (SD.exists("CONFIG.JSO")) {
      Serial.println("Lettura configurazione da scheda SD");
      File conf = SD.open("CONFIG.JSO", FILE_READ);
      DynamicJsonBuffer jsonBuffer;
      String c = "";
      while (conf.available()) {
        c +=(char) conf.read();
      }
      conf.close(); 
      JsonObject& root = jsonBuffer.parseObject(c.c_str());
      if (!root.success()){
        Serial.println("Alla configurazione mancano alcuni parametri!");
        SD.remove("CONFIG.JSO");
        return loadConfig();
      }
      if(isValidConf(root)){ //la configurazione è valida
        stats = (bool)root["status"];
        allarm = (bool)root["allarm"];
        wifi_mode = (int)root["wifi_mode"];
        ssid = root["my_ssid"].asString();
        password = root["password"].asString();
        dhcp = (bool)root["dhcp"];

        ip[0] = getValue(root["ip_v4"], '.', 0).toInt();
        ip[1] = getValue(root["ip_v4"], '.', 1).toInt();
        ip[2] = getValue(root["ip_v4"], '.', 2).toInt();
        ip[3] = getValue(root["ip_v4"], '.', 3).toInt();
        
        gateway[0] = getValue(root["gateway"], '.', 0).toInt();
        gateway[1] = getValue(root["gateway"], '.', 1).toInt();
        gateway[2] = getValue(root["gateway"], '.', 2).toInt();
        gateway[3] = getValue(root["gateway"], '.', 3).toInt();

        subnet_mask[0] = getValue(root["subnet_mask"], '.', 0).toInt();
        subnet_mask[1] = getValue(root["subnet_mask"], '.', 1).toInt();
        subnet_mask[2] = getValue(root["subnet_mask"], '.', 2).toInt();
        subnet_mask[3] = getValue(root["subnet_mask"], '.', 3).toInt();
        
        server_query = root["server_query"].asString();
        user = root["user"].asString();
        pwd = root["pwd"].asString();
        lat = double_with_n_digits(root["lat"], 8);
        lon = double_with_n_digits(root["lon"], 8);
        refresh_config = ora.unixtime();
        return true;
      }else{
        Serial.println("La configurazione attuale non è valida!");
        SD.remove("CONFIG.JSO");
        return loadConfig();
      }
  }else{
      Serial.println("Ci sono problemi con la memoria SD: file config.jso non trovato");
      setConfig();
  }
  return false;
}

String getValue(String data, const char separator, int index){
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
//verifica sel a configurazione del file è valida
bool isValidConf(JsonObject& root){
  int j = 0;
  char *my_param[] = {"status", "allarm", "wifi_mode", "my_ssid", "password", "dhcp", "ip_v4", "gateway", "subnet_mask", "server_query", "user", "pwd", "lat", "lon", "\n"};
  bool error = false;
  while(my_param[j] != "\n"){    
    if(!root.containsKey(my_param[j]))    
      error = true;
    j++; 
  }
  if(!error)
    return true;
  return false;
}

//imposta i valori di default per la configurazione
void setConfig(){
  if(SD.exists("CONFIG.JSO"))
    SD.remove("CONFIG.JSO");
  File conf = SD.open("CONFIG.JSO", FILE_WRITE);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  
  root["status"] = stats;
  root["allarm"] = allarm;
  root["wifi_mode"] = wifi_mode;
  root["my_ssid"] = ssid;
  root["password"] = password;
  root["dhcp"] = dhcp;
  root["ip_v4"] = addressToChar(ip[0], ip[1], ip[2], ip[3]);
  root["gateway"] = addressToChar(gateway[0], gateway[1], gateway[2], gateway[3]);
  root["subnet_mask"] = addressToChar(subnet_mask[0], subnet_mask[1], subnet_mask[2], subnet_mask[3]);
  root["server_query"] = server_query;
  root["user"] = user;
  root["pwd"] = pwd;
  root["lat"] = double_with_n_digits(lat, 8);
  root["lon"] = double_with_n_digits(lon, 8);
  
  root.prettyPrintTo(conf);
  conf.close();
  if (!SD.exists("CONFIG.JSO")) {
    in_error = true;  
    return;
  }  
  Serial.println("Configurazione ricaricata!");
  if(!loadConfig()){
    Serial.println("Ci sono problemi con la scrittura o lettura del file di configurazione");  
    in_error = true;
  }
}
//converte un indirizzo in una stringa
String addressToChar(int a, int b, int c, int d){
  String t = ".";
  return String(a)+t+String(b)+t+String(c)+t+String(d);
}

void execCmd(String cmd){
  if(cmd == "print" || getValue(cmd, ' ', 0) == "print"){
    printValues();  
  }  
  if(cmd == "set" ||  getValue(cmd, ' ', 0) == "set"){
    if(wifi_mode == 2)
      wifi_mode = 1;  
    else
      wifi_mode = 2;   
  }
}
//funzione che memorizza le forze g in file csv
void memorizza(){
  ora = rtc.now();
  if(!SD.exists("database/"))
    SD.mkdir("database/");
  if(!SD.exists("database/")){
    Serial.println("Scheda SD non presente!");
    in_error = true;
    return;
  }  
  String dir = "database/"+String(ora.year())+"/";
  char buf[25];
  dir.toCharArray(buf, 25);
  if(!SD.exists(buf))
    SD.mkdir(buf);  
  dir += String(ora.month())+"-"+String(ora.day())+"-"+String(ora.hour())+".csv";
  char nome[50];
  dir.toCharArray(nome, 50);
  
  bool nuovo = false;
  if(!SD.exists(nome)){
    nuovo = true;
  }
  File file = SD.open(nome, FILE_WRITE);
  if(file){
    String pvt = String(ora.unixtime())+";"+String(g_x, 3)+";"+String(g_y, 3)+";"+String(g_z, 3)+";"+String(g_tot)+";";
    pvt.replace(".", ",");
    char pvt_in[40];
    pvt.toCharArray(pvt_in, 40);    
    if(nuovo)
      file.println("Ora Unixtime;Forza G(X);Forza G(Y);Forza G(Z);Forza G Totale;");    
    file.println(pvt_in);  
    file.close(); 
  }
}

void printValues(){  
  Serial.print("stats=");  
  Serial.println(stats);  
  Serial.print("allarm=");  
  Serial.println(allarm); 
  Serial.print("wifi_mode=");  
  Serial.println(wifi_mode);
  Serial.print("my_ssid=");  
  Serial.println(ssid);  
  Serial.print("password=");  
  Serial.println(password);  
  Serial.print("dhcp=");  
  Serial.println(dhcp);  
  Serial.print("ip_v4=");  
  Serial.print(ip[0]);
  Serial.print(".");
  Serial.print(ip[1]);
  Serial.print(".");
  Serial.print(ip[2]);
  Serial.print(".");
  Serial.println(ip[3]);  
  Serial.print("gateway=");  
  Serial.print(gateway[0]);
  Serial.print(".");  
  Serial.print(gateway[1]);
  Serial.print(".");
  Serial.print(gateway[2]);
  Serial.print(".");
  Serial.println(gateway[3]);    
  Serial.print("subnet_mask=");  
  Serial.print(subnet_mask[0]);
  Serial.print(".");  
  Serial.print(subnet_mask[1]);
  Serial.print(".");  
  Serial.print(subnet_mask[2]);
  Serial.print(".");  
  Serial.println(subnet_mask[3]);
  Serial.print("server_query=");  
  Serial.println(server_query);
  Serial.print("user=");  
  Serial.println(user);
  Serial.print("pwd=");  
  Serial.println(pwd);
  Serial.print("lat=");  
  Serial.println(lat);
  Serial.print("lon=");  
  Serial.println(lon);
  Serial.print("timestamp");
  ora = rtc.now();
  Serial.println(ora.unixtime());
} 

void wifiConnect(){
  switch(wifi_mode){
    case 0: //nessun wifi
      WiFi.mode(WIFI_OFF);
      return;
    case 1: //wifi connessione al router
      if(wifi_started1){        
        if(wifi_started2){
          WiFi.mode(WIFI_STA);
          wifi_started2 = false;
          return;
        }
        server.handleClient();
      }else{
        WiFi.begin(ssid.c_str(), password.c_str());
        int i = 0;
        while (WiFi.status() != WL_CONNECTED) {
          i++;
          Serial.print(".");
          if(i >= 30){
            Serial.println("Problemi con la connessione di rete.");  
            in_error = true;
            return;
          }
          delay(500);
        }
        if(!dhcp)
            WiFi.config(IPAddress(ip[0], ip[1], ip[2], ip[3]), IPAddress(gateway[0], gateway[1], gateway[2], gateway[3]), IPAddress(subnet_mask[0], subnet_mask[1], subnet_mask[2], subnet_mask[3]));
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        server.onNotFound(handleNotFound);
        server.begin();
        wifi_started1 = true;
      }
      return;
    case 2: //wifi AP
      if(wifi_started2){        
        if(wifi_started1){
          WiFi.mode(WIFI_AP);
          wifi_started1 = false;
          return;
        }
        server.handleClient();
      }else{
        WiFi.softAP(ssid.c_str(), password.c_str());
        Serial.println(dhcp);       
        if(!dhcp)
          WiFi.softAPConfig(IPAddress(ip[0], ip[1], ip[2], ip[3]), IPAddress(gateway[0], gateway[1], gateway[2], gateway[3]), IPAddress(subnet_mask[0], subnet_mask[1], subnet_mask[2], subnet_mask[3]));
        IPAddress myIP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(myIP);
        server.onNotFound(handleNotFound);
        server.begin();
        wifi_started2 = true;
      }
      return;   
  }    
}

bool loadFromSdCard(String path){
  String dataType = "text/plain";
  String p = path;
  p.toLowerCase();
  Serial.println(p);
  if(p == "/config.jso")
    return false;
  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  server.streamFile(dataFile, dataType);
  dataFile.close();
  return true;
}

void handleNotFound(){
  if(loadFromSdCard(server.uri())) return;
  String message = "<b>Eroore 404</b>! Pagina non trovata\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    if(server.argName(i) == "getdata"){
      ora = rtc.now();
      String data1 = "{"+String(ora.unixtime())+";"+String(g_x, 8)+";"+String(g_y, 8)+";"+String(g_z, 8)+";"+String(g_tot, 8)+"}";  
      server.send(200, "text/plain", data1);
      return;
    }
    if(server.argName(i) == "getconfig"){
      if (SD.exists("CONFIG.JSO")) {
          File conf = SD.open("CONFIG.JSO", FILE_READ);
          DynamicJsonBuffer jsonBuffer;
          String c = "";
          while (conf.available()) {
            c +=(char) conf.read();
          }
          conf.close(); 
          JsonObject& root = jsonBuffer.parseObject(c.c_str());
          if (!root.success()){
            SD.remove("CONFIG.JSO");
            if(!loadConfig())
              return;
          }
          if(isValidConf(root)){ //la configurazione è valida
            root["pwd"] = "";
            String c = "";
            root.prettyPrintTo(c);
            server.send(200, "text/plain", c);
            return;
          }
      }
    }
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}

void pulisci(File dir, int anno  = 0) {
  DateTime ora_del(rtc.now()-TimeSpan((60*60*24*365)));
  int anno_del = ora_del.year();
  int mese_del = ora_del.month();
  if(!SD.exists("DATABASE/")){
    Serial.println("non esiste");
    return;
  }
  while(true){     
    File entry = dir.openNextFile();
    if(!entry){
      break;
    }
    if(entry.isDirectory()){
      if(String(entry.name()).toInt() == anno_del)
        pulisci(entry, anno_del);          
      if(String(entry.name()).toInt() < anno_del){
        pulisci(entry,String(entry.name()).toInt());   
        String pvt = String("DATABASE/"+String(entry.name()));
        char pvt_in[20];
        pvt.toCharArray(pvt_in, 20); 
        SD.rmdir(pvt_in);
      }
    }else{
      if((getValue(String(entry.name()), '-', 0).toInt() <= mese_del && anno == anno_del) || anno < anno_del){
        String pvt = String("DATABASE/"+String(anno)+"/"+String(entry.name()));
        char pvt_in[40];
        pvt.toCharArray(pvt_in, 40);    
        SD.remove(pvt_in);
      }
    }
    entry.close();     
  }
}

String request(String url){
  if(wifi_mode != 1)
    return "";
  WiFiClient client;
  if (!client.connect(server_query.c_str(), 80)) {
    Serial.println("Connessione Fallita");
    return "";
  }  
  client.print(String("GET ")+url+" HTTP/1.1\r\nHost: "+server_query+"\r\nConnection: close\r\n\r\n");
  
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return "";
    }
  }
  String ret = "";
  while(client.available())
    ret += client.readStringUntil('\r');
  return ret; 
}

void refreshC(){  
  String g = request("/refresh.php");
  if(g != ""){
    String c = "{";
    c += getValue(g, '{', 1);
    c = getValue(c, '}', 0) + "}";
    if(c == "{}")
      return;
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(c.c_str());
    if(!root.success() || isValidConf(root)){
      Serial.println("non è valida");
      return;
    }
    DateTime o(DateTime(0)+TimeSpan((int)root["timestamp"]));
    rtc.adjust(o);
    lat =  double_with_n_digits(root["lat"], 8);
    lon =  double_with_n_digits(root["lon"], 8);
    setConfig();
  } 
}

void loop() { 
  ora = rtc.now();
  //Serial.println(ora.unixtime());
  if(analogRead(PIN_BTN1) > 150){
    setStatus(!stats);
  }
  if(!in_allarm){
    analogWrite(PIN_BUZZ, 0);
    volume = 0;
    time_allarm = 0;
  }
  if(in_error && stats){
    analogWrite(PIN_LED, 0);  
    delay(150);
    analogWrite(PIN_LED, 255);  
    delay(150);
    WiFi.mode(WIFI_OFF);
    in_allarm = false;
    return;
  }

  if(!stats){
    delay(100);  
    WiFi.mode(WIFI_OFF);
    return;
  }
    
  if(digitalRead(PIN_BTN2) == HIGH && !in_allarm){
    delay(100);
    if(digitalRead(PIN_BTN2) == HIGH){
      setAllarm(!allarm);
      setConfig();
    }
  }
  if(in_allarm){
    if(time_allarm == 0){
      time_allarm = ora.unixtime();  
    }else if(ora.unixtime() > time_allarm + 60){ //faccio durare l'allarme per 1 minuto
      in_allarm = false;  
    }    
    Serial.println("=====================ALLARME==========================");
    if(digitalRead(PIN_BTN2) == HIGH){
      in_allarm = false;
    }
    if(allarm && in_allarm)
      inAllarm();  
  }
     
  String received = "";
  bool getCmd = false;
  while(Serial.available() > 0){
    getCmd = true;
    received += (char) Serial.read();    
  }
  if(getCmd){
    received.trim();
    Serial.print("Comando ricevuto: ");
    Serial.println(received);
    execCmd(received);
  }
  
  if(stats && !in_error){
    if(ora.unixtime() > (refresh_config + 60*5)){ //ricarica la configurazione ogni 5 minuti
      if(!loadConfig()){
        in_error = true;
        Serial.println("Ci sono problemi con il file di configurazione");
        return;
      }
      refreshC();
      pulisci(SD.open("DATABASE/")); 
    }
    wifiConnect();      
  }  
  Vector norm = accelerometro.readNormalize();
  if(count_acc >= 3){
    x = x/count_acc;
    y = y/count_acc;
    z = z/count_acc;
    
    double x1 = (last_x - x);
    if(x1 < 0)
      x1 = -x1;
    double y1 = (last_y - y);
    if(y1 < 0)
      y1 = -y1;
    double z1 = (last_z - z);
    if(z1 < 0)
      z1 = -z1;
    g_x = sqrt(x1*x1); //forze g calcolate tramite l'accelerazione
    g_y = sqrt(y1*y1);
    g_z = sqrt(z1*z1);
    g_tot = (g_x+g_y+g_z);
    if(!(last_x == 0.00 && last_y == 0.00 && last_z == 0.00 )){
      memorizza();
      if(g_tot > 0.39){ //scala mercalli livello 5
        in_allarm = true;   
      }      
    }        
    last_x = x;
    last_y = y;
    last_z = z;  
    x = 0;
    y = 0;
    z = 0;          
    count_acc = 0;
  }
  if(norm.XAxis < 0)
    x -= -norm.XAxis;
  else
    x += norm.XAxis;
  if(norm.YAxis < 0)
    y -= -norm.YAxis;
  else
    y += norm.YAxis;
  if(norm.ZAxis < 0)
    z -= -norm.ZAxis;
  else
    z += norm.ZAxis;
  count_acc++;  
  delay(100);
}
