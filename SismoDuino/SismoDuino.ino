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

int PIN_BTN1 = A0; //buttone on/off letto con la porta analogica
int PIN_BTN2 = 16; //buttone on/off allarme
int PIN_BUZZ = 0; // buzzer 
int PIN_LED = 2; //pin led dello stato
int volume = 0; //volume dell'allarme
bool direction_vol = true; //direzione volume, se è true incrementa

int val = 0; //test

bool stats = true; //se il sistema è acceso(true)
bool allarm = false; //se l'allarme è accesso(true)
bool in_allarm = false; //se l'allarme sta suonando

void inAllarm(){  
  if(direction_vol && volume < 25)
    volume++;
  if(!direction_vol && volume > 3)
    volume--;
  if(direction_vol && volume >= 25)
    direction_vol = false;
  if(!direction_vol && volume <= 3)
    direction_vol = true;
  analogWrite(PIN_BUZZ, volume*10);
  Serial.print("Volume = ");
  Serial.println((volume*10));
}

void setup() {  
  Serial.begin(9600);
  pinMode(PIN_BTN2, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  
  analogWrite(PIN_LED, 125);
  analogWrite(PIN_BUZZ, 0);
}
//Imposta se il sistema è acceso o è spento
void setStatus(bool var){
  stats = var;
  if(stats){
    analogWrite(PIN_LED, 125);
    Serial.println("ON");
  }else{
    analogWrite(PIN_LED, 0);
    Serial.println("OFF");
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

void loop() {    
  if(analogRead(PIN_BTN1) > 150){
    setStatus(!stats);
  }
  if(digitalRead(PIN_BTN2) == HIGH && !in_allarm){
    setAllarm(!allarm);
  }
  if(in_allarm){
    if(digitalRead(PIN_BTN2) == HIGH)
      in_allarm = false;
    if(!allarm)
      inAllarm();  
  }
  if(!in_allarm){
    analogWrite(PIN_BUZZ, 0);
    volume = 0;
  }
  //leggere i dati ricevuti da seriale o da bluetooth ed eseguirli
  
  if(stats){
    //leggere la configurazione da SD (config.txt)
    //aprire la connessione di rete (Wifi client o Wifi router)
    //leggere il sensore di accelerazione (ADXl345)
    //scrivere il datalog (raccolto in Database/Anno/Mese/Giorno/Ora.txt) . Se non è presente allora fare errore con buzzer
    //uploadare il client 
    //uploadare il server (via php i file raccolti per ora)
  }  
  delay(100);
}
