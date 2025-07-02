

// Smart Door Lock with SRF05, Keypad, LCD, RFID, Servo
// Cập nhật: SRF05 là công tắc, nhập đúng pass user để mở cửa, nhập pass admin trong thời gian mở cửa để đổi mật khẩu

#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#define BLYNK_TEMPLATE_ID "TMPL6614VtJsF"
#define BLYNK_TEMPLATE_NAME "Smart Lock"
#define BLYNK_AUTH_TOKEN "j7KvY5ZpqLaVYbqie9wg2yPPzaNEEsWe"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
//------------------Blynk---------------
char ssid[] = "FPTU_Library";
char pass[] = "12345678";
bool remoteControl = false;
// ---------------- LCD ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- Keypad ----------------
#define ROW_NUM 4
#define COLUMN_NUM 4
char keys[ROW_NUM][COLUMN_NUM] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte pin_rows[ROW_NUM] = {13, 12, 14, 27};
byte pin_column[COLUMN_NUM] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// ---------------- Servo ----------------
Servo doorServo;
#define SERVO_PIN 15

// ---------------- RFID RC522 ----------------
#define SS_PIN 5
#define RST_PIN 4
MFRC522 rfid(SS_PIN, RST_PIN);

// ---------------- SRF05 ----------------
#define TRIG_PIN 2
#define ECHO_PIN 34

// ---------------- Data ----------------
String userPass = "1234";
String adminPass = "4321";
String inputPIN = "";
bool systemActive = false;
bool doorOpened = false;
unsigned long doorOpenTime = 0;
unsigned long activeUntil = 0;
#define DOOR_OPEN_DURATION 8000 // Thời gian cửa mở (8s)
#define ACTIVE_DURATION 15000   // Mạch giữ trạng thái hoạt động 15s sau khi phát hiện người

// ---------------- Functions ----------------
float getDistanceCM()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return duration * 0.034 / 2;
}

String generateStars(int count)
{
  String s = "";
  for (int i = 0; i < count; i++)
    s += '*';
  return s;
}

String readPassword(bool show = true)
{
  String input = "";
  lcd.setCursor(0, 1);
  while (true)
  {
    char key = keypad.getKey();
    if (key)
    {
      if (key == '#')
        break;
      if (key == '*')
      {
        input = "";
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        continue;
      }
      input += key;
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(generateStars(input.length()));
    }
  }
  return input;
}

void closeDoor()
{
  doorServo.write(0);

  doorOpened = false;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhap ma hoac quet");
  lcd.setCursor(0, 1);
  lcd.print("the RFID");
  remoteControl = false;
}

void openDoor()
{
  doorServo.write(90);
  doorOpened = true;
  doorOpenTime = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cua da mo");

  lcd.setCursor(0, 1);
  lcd.print("Nhap admin...");

  unsigned long start = millis();
  String adminTry = "";

  while (millis() - start < DOOR_OPEN_DURATION)
  {
    char key = keypad.getKey();
    if (key)
    {
      if (key == '#')
      {
        if (adminTry == adminPass)
        {
          while (true)
          {
            lcd.clear();
            lcd.print("1:Admin 2:User");
            lcd.setCursor(0, 1);
            lcd.print("0:Thoat");
            char choice = 0;
            while (!choice)
              choice = keypad.getKey();

            if (choice == '0')
              break;

            lcd.clear();
            lcd.print("Pass moi:");
            String newPass = readPassword(true);

            if (choice == '1')
              adminPass = newPass;
            else if (choice == '2')
              userPass = newPass;

            lcd.clear();
            lcd.print("Doi thanh cong!");
            delay(1500);
          }
          break;
        }
        else
        {
          lcd.clear();
          lcd.print("Sai admin!");
          delay(1500);
          break;
        }
      }
      else if (key == '*')
      {
        adminTry = "";
        lcd.setCursor(0, 1);
        lcd.print("                ");
      }
      else
      {
        adminTry += key;
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(generateStars(adminTry.length()));
      }
    }
  }
}

void checkRFID()
{
  if (!systemActive)
    return;
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  Serial.print("UID: ");
  for (byte i = 0; i < rfid.uid.size; i++)
  {
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  bool valid = true;
  byte validUID[4] = {0xCC, 0x6B, 0x37, 0x2};
  for (byte i = 0; i < 4; i++)
  {
    if (rfid.uid.uidByte[i] != validUID[i])
    {
      valid = false;
      break;
    }
  }

  if (valid)
    openDoor();
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("The sai!");
    delay(2000);
    closeDoor();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void checkKeypad()
{
  if (!systemActive)
    return;
  char key = keypad.getKey();
  if (key)
  {
    if (key == '#')
    {
      if (inputPIN == userPass)
      {
        openDoor();
      }
      else
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sai pass!");
        delay(1500);
        closeDoor();
      }
      inputPIN = "";
    }
    else if (key == '*')
    {
      inputPIN = "";
      lcd.setCursor(0, 1);
      lcd.print("                ");
    }
    else
    {
      inputPIN += key;
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(generateStars(inputPIN.length()));
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();
  lcd.backlight();
  doorServo.attach(SERVO_PIN);
  closeDoor();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop()
{
  Blynk.run();
  float d = getDistanceCM();
  if (d > 0 && d < 15)
  {
    activeUntil = millis() + ACTIVE_DURATION;
    if (!systemActive)
    {
      lcd.backlight();
      lcd.clear();
      lcd.print("Nhap ma hoac quet");
      lcd.setCursor(0, 1);
      lcd.print("the RFID");
    }
    systemActive = true;
  }

  if (systemActive && millis() > activeUntil && !doorOpened && !remoteControl)
  {
    systemActive = false;
    lcd.noBacklight();
    inputPIN = "";
    closeDoor();
  }

  checkRFID();
  checkKeypad();

  if (doorOpened && millis() - doorOpenTime >= DOOR_OPEN_DURATION)
  {
    closeDoor();
  }

  delay(100);
}

// BLYNK_WRITE(V0)
// {
//   int pinValue = param.asInt(); // 1 = UNLOCK, 0 = LOCK
//   Serial.print("Blynk gửi giá trị V0: ");
//   Serial.println(pinValue);

//   if (pinValue == 1)
//   {
//     Serial.println("→ Yêu cầu mở cửa từ Blynk");
//     remoteControl = true;
//     systemActive = true;
//     openDoor();
//   }
//   else
//   {
//     Serial.println("→ Yêu cầu đóng cửa từ Blynk");
//     closeDoor();
//     remoteControl = false;
//   }
// }

BLYNK_WRITE(V0)
{
  int pinValue = param.asInt();
  Serial.println("===== BLYNK_WRITE V0 GỌI =====");
  Serial.print("Giá trị nhận từ app: ");
  Serial.println(pinValue);

  if (pinValue == 1)
  {
    Serial.println("→ Yêu cầu mở cửa từ Blynk");
    remoteControl = true;
    systemActive = true;
    openDoor();
  }
  else
  {
    Serial.println("→ Yêu cầu đóng cửa từ Blynk");
    closeDoor(); 
  }
}