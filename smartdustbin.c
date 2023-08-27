#include<Servo.h>
const int trigPin = D3;
const int echoPin = D4;
long duration;
int distance;
int safetyDistance;
Servo myservo; 
on most boards
int pos = 0; 
void setup() {
pinMode(trigPin, OUTPUT);
pinMode(echoPin, INPUT);
myservo.attach(D5);
Serial.begin(9600);
}
void loop() {
digitalWrite(trigPin, LOW);
delayMicroseconds(5);
digitalWrite(trigPin, HIGH);
delayMicroseconds(15);
digitalWrite(trigPin, LOW);
duration = pulseIn(echoPin, HIGH);
distance = duration * 0.034 / 2;
safetyDistance = distance;
if (safetyDistance <= 5) {
myservo.write(90); 
delay(1000); 
}
else { 
myservo.write(0);
delay(1000); }
Serial.print("Distance: ");
Serial.println(distance);
}
}
#include "ThingSpeak.h"
#include <ESP8266WiFi.h>
#include<Servo.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
char ssid[] = "name"; 
char pass[] = "password";
const int total_height = 9;
const int hold_height = 7;
int minutes = 1; 
unsigned long Channel_ID = 1816688;
const char * WriteAPIKey = "XFDR6GN8KDOZBTSL"; 
const int trigger = D8;
const int echo = D7;
long Time;
int x;
int i;
int distanceCM;
int resultCM;
int bin_lvl = 0;
int snsr_to_max = 0;
const int Field_number = 1;
WiFiClient client;
void setup()
{
lcd.init();
lcd.backlight();
lcd.setCursor(0, 0);
lcd.print("IoT Garbage lvl");
lcd.setCursor(0, 1);
lcd.print("Monitoring Sys.");
Serial.begin(115200);
pinMode(trigger, OUTPUT);
pinMode(echo, INPUT)
WiFi.mode(WIFI_STA);
ThingSpeak.begin(client);
snsr_to_max = total_height - hold_height;
//delay(2500);
}
void loop()
{
internet();
measure();
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("Garbage Level:");
lcd.setCursor(5, 1);
lcd.print(bin_lvl);
lcd.print('%');
Serial.print("Garbage Level:");
Serial.print(bin_lvl);
Serial.println("%");
upload();
for (i = 0; i < minutes; i++)
{
Serial.println("-------------------------");
Serial.println("System Standby....");
Serial.print(i);
Serial.println(" minutes elapsed.");
//delay(2000);
//delay(2000);
//de//lay(2000);
}
}
void upload()
{
internet();
x = ThingSpeak.writeField(Channel_ID, Field_number, bin_lvl, WriteAPIKey);
if (x == 200)Serial.println("Data Updated.");
if (x != 200)
{
Serial.println("Data upload failed, retrying....");
//delay(15000);
upload();
}
}
void measure()
{
delay(100);
digitalWrite(trigger, HIGH);
delayMicroseconds(10);
digitalWrite(trigger, LOW);
Time = pulseIn(echo, HIGH);
distanceCM = Time * 0.034;
resultCM = distanceCM / 2;
bin_lvl = map(resultCM, snsr_to_max, total_height, 100, 0);
if (bin_lvl > 100) bin_lvl = 100;
if (bin_lvl < 0) bin_lvl = 0;
}
void internet()
{
if (WiFi.status() != WL_CONNECTED)
{
Serial.print("Attempting to connect to SSID: ");
Serial.println(ssid);
while (WiFi.status() != WL_CONNECTED)
{
WiFi.begin(ssid, pass);
Serial.print(".");
//delay(5000);
}
Serial.println("\nConnected.");
}
}
