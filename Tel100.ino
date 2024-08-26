#include <SPI.h>         // Inkluderer SPI-biblioteket for kommunikasjon med RFID-leseren
#include <MFRC522.h>     // Inkluderer MFRC522-biblioteket for å bruke RFID-leseren
#include <Servo.h>       // Inkluderer Servo-biblioteket for å kontrollere servomotoren

#define SS_PIN 10        // Angir Slave Select (SS) pin for RFID-leseren
#define RST_PIN 9        // Angir Reset pin for RFID-leseren
#define LED_G 5          // Definerer pinnen for den grønne LED-lampen
#define LED_R 4          // Definerer pinnen for den røde LED-lampen
#define BUZZER 2         // Definerer pinnen for buzzer

MFRC522 mfrc522(SS_PIN, RST_PIN); // Oppretter en instans av MFRC522-klassen for RFID-leseren
Servo myServo;          // Oppretter et servobjekt for å kontrollere servomotoren

unsigned long lastAccessTime = 0; // Lagrer tiden for siste vellykkede tilgang
unsigned long feedWindowStartTime = 0; // Lagrer starten på 20-minuttersvinduet for fôring
const unsigned long accessDelay = 30000; // 30 sekunders forsinkelse mellom hver tilgang
const unsigned long feedWindowDuration = 1200000; // 20 minutter (i millisekunder)
int feedCount = 0; // Teller hvor mange vellykkede fôringer som har skjedd i det nåværende vinduet
int totalFeedCount = 0; // Totalt antall fôringer siden siste påfylling
const int maxFeedsPerWindow = 4; // Maksimalt antall fôringer tillatt i 20-minuttersvinduet
const int maxFeedsBeforeRefill = 8; // Maksimalt antall fôringer før man trenger å fylle på fôr
bool isBlinking = false; // Sporer om blinkingen har startet

void setup() {
  Serial.begin(9600);   // Starter seriel kommunikasjon for debugging
  SPI.begin();          // Starter SPI-bussen for kommunikasjon med RFID-leseren
  mfrc522.PCD_Init();   // Initialiserer RFID-leseren
  myServo.attach(3);    // Angir hvilken pinne servomotoren er koblet til
  myServo.write(0);     // Setter servomotoren til startposisjon (0 grader)
  pinMode(LED_G, OUTPUT); // Setter grønne LED til utgangsmodus
  pinMode(LED_R, OUTPUT); // Setter røde LED til utgangsmodus
  pinMode(BUZZER, OUTPUT); // Setter buzzer til utgangsmodus
  noTone(BUZZER);       // Slår av buzzer hvis den var på

  Serial.println("Put your card to the reader..."); // Informerer brukeren om å plassere kortet ved leseren
  feedWindowStartTime = millis(); // Starter 20-minuttersvinduet
}

void loop() {
  // Sjekker om LED-lysene skal blinke fordi maks fôringer er nådd
  if (isBlinking) {
    blinkLEDs(); // Kaller blinkefunksjonen hvis blinking er aktiv
    return;
  }

  // Sjekker om det er et nytt kort til stede
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Velger kortet som er oppdaget
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Henter gjeldende tid
  unsigned long currentTime = millis();

  // Sjekker om et nytt 20-minuttersvindu skal starte
  if (currentTime - feedWindowStartTime >= feedWindowDuration) {
    feedWindowStartTime = currentTime; // Nullstiller starttidspunktet for 20-minuttersvinduet
    feedCount = 0; // Nullstiller fôringsantallet for det nye vinduet
    isBlinking = false; // Stopper blinking hvis et nytt vindu starter
  }

  // Sjekker om maks antall fôringer i det nåværende vinduet er nådd
  if (feedCount >= maxFeedsPerWindow) {
    Serial.println("Access denied - Max feeds reached for this window"); // Informerer om at maks fôringer er nådd
    digitalWrite(LED_R, HIGH); // Tenner rød LED
    tone(BUZZER, 300); // Aktiverer buzzer med en tone
    delay(1000); // Forsinker 1 sekund
    digitalWrite(LED_R, LOW); // Slår av rød LED
    noTone(BUZZER); // Slår av buzzer
    
    isBlinking = true; // Starter blinkeprosessen
    return;
  }

  // Sjekker om det er første skanning eller om ventetiden mellom skanninger er over
  if (lastAccessTime != 0 && currentTime - lastAccessTime < accessDelay) {
    unsigned long timeLeft = (accessDelay - (currentTime - lastAccessTime)) / 1000; // Beregner resterende ventetid
    Serial.println("Access denied - Wait for 30 seconds between scans"); // Informerer om ventetiden
    digitalWrite(LED_R, HIGH); // Tenner rød LED
    tone(BUZZER, 300); // Aktiverer buzzer med en tone
    delay(1000); // Forsinker 1 sekund
    digitalWrite(LED_R, LOW); // Slår av rød LED
    noTone(BUZZER); // Slår av buzzer
    return;
  }

  // Viser UID på det oppdagede RFID-kortet i serial monitor
  Serial.print("UID tag: ");
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message: Hei"); // Viser en melding i serial monitor
  content.toUpperCase();

  // Sjekker om det skannede kortet er autorisert
  if (content.substring(1) == "D3 E2 E3 F6") { // Endre her for å samsvare med ditt korts UID
    Serial.println("Spis i vei"); // Informerer om at fôring er tillatt
    Serial.println();
    digitalWrite(LED_G, HIGH); // Tenner grønn LED
    tone(BUZZER, 500); // Aktiverer buzzer med en annen tone
    delay(300); // Forsinker 0,3 sekunder
    noTone(BUZZER); // Slår av buzzer
    myServo.write(180); // Roterer servomotoren for å åpne materen
    delay(5000); // Holder materen åpen i 5 sekunder
    myServo.write(0); // Lukker materen
    digitalWrite(LED_G, LOW); // Slår av grønn LED

    // Oppdaterer fôringsantallet og tidspunktet for siste tilgang
    lastAccessTime = millis();
    feedCount++;
    totalFeedCount++;

    // Sjekker om det er behov for påfylling av fôr
    if (totalFeedCount >= maxFeedsBeforeRefill) {
      Serial.println("Please refill the food dispenser!"); // Informerer om behov for påfylling
      totalFeedCount = 0; // Nullstiller totalt fôringsantall etter påfylling
    }

  } else {
    // Avslår tilgang hvis kortet ikke er autorisert
    Serial.println("Mat Pause"); // Informerer om at fôring er avslått
    digitalWrite(LED_R, HIGH); // Tenner rød LED
    tone(BUZZER, 300); // Aktiverer buzzer med en tone
    delay(1000); // Forsinker 1 sekund
    digitalWrite(LED_R, LOW); // Slår av rød LED
    noTone(BUZZER); // Slår av buzzer
  }
}

// Funksjon for å blinke begge LED-lysene
void blinkLEDs() {
  static unsigned long lastBlinkTime = 0; // Lagrer tidspunktet for siste blinking
  static bool ledState = false; // Sporer LED-tilstanden (på/av)
  unsigned long currentTime = millis();

  if (currentTime - lastBlinkTime >= 500) { // Blinker hvert 500ms
    ledState = !ledState;
    digitalWrite(LED_G, ledState);
    digitalWrite(LED_R, ledState);
    lastBlinkTime = currentTime;
  }
}
