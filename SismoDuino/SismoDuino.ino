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

ADXL345 accelerometro;
RTC_DS1307 rtc;

int PIN_BTN1 = A0; //buttone on/off letto con la porta analogica
int PIN_BTN2 = 16; //buttone on/off allarme
int PIN_BUZZ = 0; // buzzer 
int PIN_LED = 2; //pin led dello stato
int volume = 0; //volume dell'allarme
bool direction_vol = true; //direzione volume, se è true incrementa

DateTime ora; //ora corrente del sensore RTC
int refresh_config = 0; //quando è stata caricata la configurazione

bool stats = true; //se il sistema è acceso(true)
bool allarm = true; //se l'allarme è accesso(true)
bool in_allarm = false; //se l'allarme sta suonando
bool in_error = false; //se è presente qualche errore nel sistema allora il led di accensione lampeggia

//config
int my_id = 0;
bool debug = false;
int wifi_mode = 2;
const char *ssid = "SismoDuino-WiFi";
const char *password = "sismoduino";
bool dhcp = true;
int ip[] = {192, 168, 1, 4};
int gateway[] = {192, 168, 1, 1};
int subnet_mask[] = {255, 255, 255, 0};
const char *server_query = "http://sismoduino.altervista.org/";
const char *user = "guest";
const char *pwd = "guest";
double lat = 0.0;
double lon = 0.0;
bool bluetooth = false;
int precision = 3; //precisione dell'accelerometro(1- precisione alta, 10- precisione bassa)

//accelerometro
double x = 0.00;
double y = 0.00;
double z = 0.00;
int count_acc = 0; //contatore accelerometro, se è 3 allora fa la media e controlla con i valori calibrati
bool calibrato = false;
double last_x = 0.00; 
double last_y = 0.00;
double last_z = 0.00;

void setup() {  
  Serial.begin(9600);
  pinMode(PIN_BTN2, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  
  analogWrite(PIN_LED, 100);
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
  if(!accelerometro.begin())
  {
    Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
    delay(500);
  }
  accelerometro.setRange(ADXL345_RANGE_16G);
  Serial.println("La memoria SD e' stata letta con successo");
}
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
    allarm = true;
        
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
     Serial.println("Lettura scheda SD");
      File conf = SD.open("CONFIG.JSO", FILE_READ);
      StaticJsonBuffer<1024> jsonBuffer;
      char c [1024];
      int  i = 0;
      while (conf.available()) {
        c[i] =(char) conf.read();
        i++;
      }
      conf.close(); 
      JsonObject& root = jsonBuffer.parseObject(c);

      if (!root.success()){
        Serial.println("Alla configurazione mancano alcuni parametri!");
        SD.remove("CONFIG.JSO");
        return loadConfig();
      }
      if(isValidConf(root)){ //la configurazione è valida
        stats = (bool)root["status"];
        allarm = (bool)root["allarm"];
        debug = (bool)root["debug"];
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
        
        my_id =(int) root["my_id"];
        server_query = root["server_query"].asString();
        user = root["user"].asString();
        pwd = root["pwd"].asString();
        lat = (double)root["lat"];
        lon = (double)root["lon"];
        bluetooth = (bool)root["bluetooth"];
        precision = (int)root["precision"];
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
  char *my_param[] = {"status", "allarm", "debug", "wifi_mode", "my_ssid", "password", "dhcp", "ip_v4", "gateway", "subnet_mask", "my_id", "server_query", "user", "pwd", "lat", "lon", "bluetooth", "precision","\n"};
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
  File conf = SD.open("CONFIG.JSO", FILE_WRITE);
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  
  root["status"] = stats;
  root["allarm"] = allarm;
  root["debug"] = debug;
  root["wifi_mode"] = wifi_mode;
  root["my_ssid"] = ssid;
  root["password"] = password;
  root["dhcp"] = dhcp;
  root["ip_v4"] = addressToChar(ip[0], ip[1], ip[2], ip[3]);
  root["gateway"] = addressToChar(gateway[0], gateway[1], gateway[2], gateway[3]);
  root["subnet_mask"] = addressToChar(subnet_mask[0], subnet_mask[1], subnet_mask[2], subnet_mask[3]);
  root["my_id"] = my_id;
  root["server_query"] = server_query;
  root["user"] = user;
  root["pwd"] = pwd;
  root["lat"] = double_with_n_digits(lat, 8);
  root["lon"] = double_with_n_digits(lon, 8);
  root["bluetooth"] = bluetooth;
  root["precision"] = precision;
  
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
}


void printValues(){
  Serial.print("my_key=");  
  Serial.println(my_id);  
  Serial.print("stats=");  
  Serial.println(stats);  
  Serial.print("allarm=");  
  Serial.println(allarm);  
  Serial.print("debug=");  
  Serial.println(debug);  
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
  Serial.print("bluetooth=");  
  Serial.println(bluetooth);
  Serial.print("precision=");
  Serial.println(precision);
} 

void loop() {    
  ora = rtc.now();
  Serial.println(in_allarm);
  Serial.println(ora.unixtime());
  if(analogRead(PIN_BTN1) > 150){
    setStatus(!stats);
  }
  if(digitalRead(PIN_BTN2) == HIGH && !in_allarm){
    delay(100);
    if(digitalRead(PIN_BTN2) == HIGH)
      setAllarm(!allarm);
    calibrato = false;
  }
  if(in_allarm){
    Serial.println("=====================ALLARME==========================");
    if(digitalRead(PIN_BTN2) == HIGH){
      in_allarm = false;
      calibrato = false;
    }
    if(!allarm)
      inAllarm();  
  }
  if(!in_allarm){
    analogWrite(PIN_BUZZ, 0);
    volume = 0;
  }
  if(in_error && stats){
    analogWrite(PIN_LED, 0);  
    delay(150);
    analogWrite(PIN_LED, 255);  
    delay(150);
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
    /*[ ] leggere i dati ricevuti da seriale o da bluetooth ed eseguirli
     * [X] print
     * [ ] reload config
     * [ ] restart nodemcu
     * [ ] set (tutti i  campi)
     * [ ] get (tutti i campi)
     */
  }
  if(stats && !in_error){
    if(ora.unixtime() > (refresh_config + 60*5)){ //ricarica la configurazione ogni 5 minuti
      if(!loadConfig()){
        in_error = true;
        Serial.println("Ci sono problemi con il file di configurazione");
        return;
      }
    }
    
    //[X] calibrazione accelerometro tramite il pulsante allarme
    //[X] leggere la configurazione da SD (config.json)
    //[X] refresh della configurazione ogni 5 minuti
    //[X] leggere il timestamp della RTC
    //[ ] aprire la connessione di rete (Wifi client o Wifi router)
    //[ ] basso consumo
    //[X] leggere il sensore di accelerazione (ADXl345)
    //[X] rilevare attività
    //[ ] scrivere il datalog (raccolto in Database/Anno/Mese/Giorno/Ora.txt) . Se non è presente allora fare errore con buzzer
    
    //[ ] aggiornare il timestamp della RTC via WEB
    //[ ] uploadare il client 
    //[ ] uploadare il server (via php i file raccolti per ora)
  }  
  Vector norm = accelerometro.readNormalize();
  if(!calibrato){
    last_x = norm.XAxis;
    last_y = norm.YAxis;
    last_z = norm.ZAxis;
    calibrato = true;
  }
  Serial.println(count_acc);
  if(count_acc >= 3){
    if(!in_allarm){
      x = x/count_acc;
      y = y/count_acc;
      z = z/count_acc;
      double range = 0.10 * precision;
      if(x >= last_x + range || x <= last_x - range)
        in_allarm = true;
      if(y >= last_y + range || y <= last_y - range)
        in_allarm = true;  
      if(z >= last_z + range || z <= last_z - range)
        in_allarm = true;
      x = 0;
      y = 0;
      z = 0;
      count_acc = 0;
    }
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
