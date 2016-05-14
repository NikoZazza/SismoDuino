

int PIN_BNT1 = A0; //buttone on/off letto con la porta analogica
int PIN_BTN2 = 16; //buttone on/off allarme
int PIN_BUZZ = 0; //buttone buzzer 
int volume = 0; //volume dell'allarme
bool direction_vol = true; //direzione volume, se è true incrementa

bool stats = true; //se il sistema è acceso(true)
bool allarm = true; //se l'allarme è accesso(true)
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
  pinMode(PIN_BTN2, INPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  analogWrite(PIN_BUZZ, 0);
}
void loop() {
  if(analogRead(PIN_BNT1) == 1024){
    stats = !stats;
    in_allarm = true;
    ESP.restart();
    delay(100);
    return;
  }
  if(digitalRead(PIN_BTN2) == HIGH)
      in_allarm = !in_allarm;
  if(in_allarm){
    if(digitalRead(PIN_BTN2) == HIGH)
      in_allarm = false;
    inAllarm();  
  }
  if(stats){
    
    
  }
  Serial.println(analogRead(PIN_BNT1));
  
  delay(100);
}
