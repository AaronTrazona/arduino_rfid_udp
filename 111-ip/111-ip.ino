#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <MFRC522.h>
#include <arduino-timer.h>
#include <ArduinoJson.h>

#define STORAGE_SIZE 10
#define SS_PIN 8
#define RST_PIN 7

MFRC522 mfrc522(SS_PIN, RST_PIN);

// create a timer that holds 16 tasks, with millisecond resolution,
// and a custom handler type of String
Timer<16, millis, String> timer;

struct TaskInfo {
  Timer<>::Task id;  // id to use to cancel
  String uidTag;
};

TaskInfo tasks[STORAGE_SIZE];

char packetBuffer[255]; //buffer to hold incoming packet

// device ip and port
byte mac[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
IPAddress ip(111, 111, 111, 111);
unsigned int localPort = 11111;

// remote ip and port
char remoteIP[] = "111.111.111.222";
int remotePort = 22222;

EthernetUDP Udp;

void commandSerialMonitor() {
  if (Serial.available() > 0) {
    String msgReceived = Serial.readString();
    Serial.println(msgReceived);

    if (msgReceived == "Exit\n") {
      digitalWrite(3, HIGH);
      digitalWrite(2, LOW);
    }
  }
}

bool findOne(String uidTag, Timer<>::Task id) {
  for (int i =0; i< STORAGE_SIZE; i++) {
    if(uidTag == tasks[i].uidTag) {
      Serial.print("Found UIDTag: ");
      Serial.println(uidTag);
      timer.cancel(tasks[i].id);
      tasks[i].id = id;
      return true;
    }
  }
  return false;
}

void insertUIDTag(String uidTag, Timer<>::Task id) {
  for (int i =0; i< STORAGE_SIZE; i++) {
    if(tasks[i].uidTag.length() == 0) {
      tasks[i].uidTag = uidTag;
      tasks[i].id = id;
      Serial.print("Insert UIDTag to Struct: ");
      Serial.println(uidTag);
      digitalWrite(3, LOW);
      digitalWrite(2, HIGH);
      return;
    }
  }
}

void removeUIDTag(String uidTag) {
  for (int i =0; i< STORAGE_SIZE; i++) {
    if(uidTag == tasks[i].uidTag) {
      Serial.print("Remove UIDTag: ");
      Serial.println(uidTag);
      tasks[i].uidTag = "";
      timer.cancel(tasks[i].id);
    }
  }
}

bool checkStructHasNoTask() {
  for (int i =0; i< STORAGE_SIZE; i++) {
    if(tasks[i].uidTag.length() != 0) {
      return false;
    }
  }

  return true;
}

bool releaseUidTag(String uidTag) {
  Serial.print("Release UIDTag: ");
  Serial.println(uidTag);
  removeUIDTag(uidTag);

  if (checkStructHasNoTask()) {
    digitalWrite(3, HIGH);
    digitalWrite(2, LOW);
  }

  return false;
}

void beep() {
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
}

void sendMessage(String uidTag) {
  Udp.beginPacket(remoteIP, remotePort);
  Udp.write(uidTag.c_str());
  Udp.endPacket();
}

void setup() {
  Serial.begin(9600);
  // Ethernet setup
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  // MFRC522 setup
  SPI.begin();
  mfrc522.PCD_Init();
  // if (mfrc522.PCD_PerformSelfTest()) Serial.println("Passed Self-Test");
  Serial.println("Approximate your card to the reader...");
  
  // Initialize PIN OUTPUT
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  digitalWrite(3, HIGH);
}


void loop() {
  timer.tick();
  delay(500);

  // look for transmit message through udp
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    Serial.println(Udp.remotePort());

    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }

    Serial.println("Contents:");
    Serial.println(packetBuffer);
    String uidTagRemote = String(packetBuffer);
    // call the releaseUidTag function in 20 seconds
    auto task = timer.in(20000, releaseUidTag, uidTagRemote);

    if (!findOne(uidTagRemote, task)) {
      insertUIDTag(uidTagRemote, task);
    }
  }

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  {
    return;
  }
  //Show UID on serial monitor
  Serial.print("UID tag :");
  String uidTag= "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
     uidTag.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     uidTag.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  mfrc522.PICC_HaltA();
  uidTag.toUpperCase();
  Serial.println(uidTag);
  beep();
  if (checkStructHasNoTask()) {
    sendMessage(uidTag);
  } else {
    releaseUidTag(uidTag);
  }
}
