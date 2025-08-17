// Include required libraries 
#include <SPI.h>               // For SPI communication
#include <MFRC522.h>           // For RC522 RFID reader

// Define RFID module connections
#define SS_PIN 10              // Slave Select (SDA) pin on RC522
#define RST_PIN 9              // Reset pin on RC522
#define BUZZER_PIN 7           // Digital pin for buzzer output

//Define button connections
bool breakActive = false;       // true while operator is on break
unsigned long breakStart = 0;   // timestamp when break began
const unsigned long BREAK_MS = 15000;

const int buttonPin       = 4;
const unsigned long debounceDelay = 50;
int  lastButtonState     = HIGH;   // raw pin value last time  
int  buttonState         = HIGH;   // debounced, “real” state  
unsigned long lastDebounceTime = 0;

// Define Solenoid Lock connections
#define SOLENOID_PIN 5

MFRC522 rfid(SS_PIN, RST_PIN); // Create RFID object with specified pins

bool firstRun = true;          // Helps initialize system state during first loop
unsigned long tagRemovedTime = 0; // Timestamp when tag is removed
bool buzzerActivated = true;   // Tracks whether the buzzer is currently active

// Define the authorized tag UID (replace with your own tag if needed)
byte authorizedUID[4] = {0x73, 0xEA, 0x66, 0x14};

// Function to handle tag removal logic
void handleTagRemoval() {
  buzzerActivated = false;         // Reset buzzer flag
  tagRemovedTime = millis();       // Record time of tag removal
  Serial.println("Tag removed - Waiting to turn buzzer ON.");
}


// Function to handle tag removal logic
void breakTime() {
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW && !breakActive) {
        // Start break
        breakActive = true;
        breakStart  = millis();

        Serial.println("Break ON → silencing for 15s");
        // Immediately mute
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActivated = false;
      }
    }
  }
  lastButtonState = reading;
}

void setup() {
  Serial.begin(9600);              // Start serial communication
  while (!Serial);                 // Wait for Serial Monitor to open
  SPI.begin();                     // Initialize SPI bus
  rfid.PCD_Init();                 // Initialize RFID module
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);     // Set buzzer pin as output
  digitalWrite(BUZZER_PIN, LOW);   // Start with buzzer off
  Serial.println("RFID system initialized.");
  digitalWrite(SOLENOID_PIN, LOW);
  delay(10000);
}

void loop() {
  breakTime();
  // If on break, skip everything else until 15 s elapses
if (breakActive) {
  if (millis() - breakStart >= BREAK_MS) {
    breakActive = false;
    Serial.println("Break OFF → resuming normal operation");
    // reset removal timer so you get your normal delay
    tagRemovedTime = millis();
  }
  delay(50);  // small idle so we don’t hammer the CPU
  return;     // skip all belt/lock code while on break
}
  bool currentTagPresent = false;   // Temporary variable to check tag state in this iteration

  // Try to detect a new card/tag
  if (rfid.PICC_IsNewCardPresent()) {
    if (rfid.PICC_ReadCardSerial()) {
      // Compare scanned UID with authorized UID
      bool isAuthorized = true;
      for (byte i = 0; i < 4; i++) {
        if (rfid.uid.uidByte[i] != authorizedUID[i]) {
          isAuthorized = false;
          break;                    // Exit loop early if mismatch found
        }
      }
      if (isAuthorized) {
        currentTagPresent = true;   // Mark tag as authorized and present
      }
    }
  }

  // If tag just came into range
  if (currentTagPresent) {
    Serial.println("Authorized tag detected - Buzzer OFF.");
     // Turn off buzzer if belt is worn
    digitalWrite(BUZZER_PIN, LOW);
  }
  else {
    if (!currentTagPresent) {
      handleTagRemoval();         // Call tag removal handler
    }

    // Check if 400 milli-seconds passed since tag removal, and buzzer isn't yet activated
    if (!currentTagPresent && !buzzerActivated && millis() - tagRemovedTime >= 400) {
      digitalWrite(BUZZER_PIN, HIGH);
      // Turn on buzzer after delay
      Serial.println("Buzzer ON.");
      buzzerActivated = true;
    }
  }

  delay(200); // Small delay for system stability
}
