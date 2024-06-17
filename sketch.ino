#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// screen properties
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// pins
#define BUZZER 15
#define LED_1 5
#define PB_CANCEL 2 // 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 4 // 35
#define DHTPIN 12
#define TEMP_PIN 18
#define HUM_PIN 19
#define LEFT_LDR_PIN 34
#define RIGHT_LDR_PIN 35
#define MOTOR_PMW_PIN 14

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
Servo motorServor;

// time server and UTC offset
#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

int utc_offset = UTC_OFFSET;

// to store the time
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

// to keep track of the time
unsigned long timeNow = 0;
unsigned timeLast = 0;

// alarm related constants and variables
bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {15, 1, 2};
int alarm_minutes[] = {15, 10, 20};
bool alarm_triggered[] = {false, false, false};

// buzzer tones frequencies
int C = 267;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;

int notes[] = {C, D, E, F, G, A, B, C_H};
int n = 8;

// menu options
int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set UTC Offset", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3", "5 - Disable Alarms"};

// temperaure and humidity levels
#define MAX_TEMP 32
#define MIN_TEMP 26
#define MIN_HUM 60
#define MAX_HUM 80

// global variables to store servo motor parameters
float min_angle = 30; // minimum angle of the servo motor
float control_factor = 0.75; // controlling factor of rotating angle
float LDR_D = 1; // if RIGHT LDR > LEFT_LDR ? 0.5 : 1.5
float max_intensity; // maximum light intensity

// wifi and mqtt clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup() {
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(TEMP_PIN, OUTPUT);
  pinMode(HUM_PIN, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(LEFT_LDR_PIN, INPUT);
  pinMode(RIGHT_LDR_PIN, INPUT);
  pinMode(MOTOR_PMW_PIN, OUTPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  motorServor.attach(MOTOR_PMW_PIN);

  Serial.begin(115200);
  // initialized the OLED display
  if (! display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Display is initialized!!!");
  }
  Serial.println("Display initialized...");

  display.display();
  delay(2000);

  setupWiFi();   // setup the wifi
  setupMQTT();   // setup the MQTT

  display.clearDisplay();
  print_line("Welcome to MediBox", 2, 0, 0);
  display.clearDisplay();

  configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER);

}

void setupWiFi() {
  // connect to the WiFi
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WiFi...", 0, 0, 1);
  }

  display.clearDisplay();
  print_line("WiFi Connected", 0, 0, 1);
}

void setupMQTT() {
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void loop() {
  if(!mqttClient.connected()){
    connectToMQTTBroker();
  }

  mqttClient.loop();

  // slide the motor servo
  slideMotorServo();
  // send the intensities
  sendIntensities();

  update_time_with_alarms();

  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }

  check_temp();
}

void print_line(String text, int row, int column, int size) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(row, column);
  display.println(text);
  display.display();
}

void print_date() {
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
  display.clearDisplay();
}

void update_time() {

  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%D", &timeinfo);
  days = atoi(timeDay);
}

void ring_alarm() {
  display.clearDisplay();
  print_line("Time to medicine!", 0, 0, 2);

  bool break_happened = false;

  // light up the LED_1
  digitalWrite(LED_1, HIGH);

  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
    // ring the buzzer
    for (int i = 0; i < n; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  // clear the LED
  digitalWrite(LED_1, LOW);
  display.clearDisplay(); // clear the display again to show the time

}

void update_time_with_alarms() {
  update_time();
  print_date();

  if (alarm_enabled) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

int wait_until_button_press() {
  while (true) {
    if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }

    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }

    if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }

    if (digitalRead(PB_CANCEL) == HIGH) {
      delay(200);
      return PB_CANCEL;
    }
  }
}

void go_to_menu() {
  while (digitalRead(PB_CANCEL) == LOW) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_until_button_press();

    if (pressed == PB_UP) {
      delay(200);
      current_mode++;
      current_mode %= max_modes;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode--;
      if (current_mode < 0) {
        current_mode = max_modes - 1;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      Serial.println(modes[current_mode]);
      run_mode(current_mode);
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

void run_mode(int current_mode) {
  if (current_mode == 0) {
    set_utc();
  }
  else if (current_mode == 1 || current_mode == 2 || current_mode == 3) {
    set_alarm(current_mode - 1);
  }
  else if (current_mode == 4) {
    alarm_enabled = false;
  }
}

void set_utc() {
  // get the utc dst
  int tmp_utc = utc_offset;

  while (true) {
    display.clearDisplay();
    print_line("Set UTC Offset: " + String(tmp_utc), 0, 0, 2);

    int pressed = wait_until_button_press();

    if (pressed == PB_UP) {
      delay(200);
      tmp_utc++;
      tmp_utc %= 14;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      tmp_utc--;
      if (tmp_utc < -12) {
        tmp_utc = 14;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      utc_offset = tmp_utc;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Time Zone Set!", 0, 0, 1);
  delay(1000);
  // reload the time config
  configTime(utc_offset * 3600, UTC_OFFSET_DST * 3600, NTP_SERVER);
}

void set_time() {
  // update the hours
  int tmp_hours = hours;
  while (true) {
    display.clearDisplay();
    print_line("Set Hours: " + String(tmp_hours), 0, 0, 2);

    int pressed = wait_until_button_press();

    if (pressed == PB_UP) {
      delay(200);
      tmp_hours++;
      tmp_hours %= 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      tmp_hours--;
      if (tmp_hours < 0) {
        tmp_hours = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      hours = tmp_hours;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  // update the minutes
  int tmp_mins = minutes;
  while (true) {
    display.clearDisplay();
    print_line("Set Minutes: " + String(tmp_mins), 0, 0, 2);

    int pressed = wait_until_button_press();

    if (pressed == PB_UP) {
      delay(200);
      tmp_mins++;
      tmp_mins %= 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      tmp_mins--;
      if (tmp_mins < 0) {
        tmp_mins = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      minutes = tmp_mins;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }


  display.clearDisplay();
  print_line("Time is Set!", 0, 0, 2);
}

// set the alarm time // alarm 1 and alarm 2
void set_alarm(int alarm) {
  // update the alarm hour
  int tmp_hours = alarm_hours[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Set Alarm Hour: " + String(tmp_hours), 0, 0, 2);

    int pressed = wait_until_button_press();

    if (pressed == PB_UP) {
      delay(200);
      tmp_hours++;
      tmp_hours %= 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      tmp_hours--;
      if (tmp_hours < 0) {
        tmp_hours = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = tmp_hours;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  // update the alarm minutes
  int tmp_mins = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Set Alarm Minutes: " + String(tmp_mins), 0, 0, 2);

    int pressed = wait_until_button_press();

    if (pressed == PB_UP) {
      delay(200);
      tmp_mins++;
      tmp_mins %= 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      tmp_mins--;
      if (tmp_mins < 0) {
        tmp_mins = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = tmp_mins;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }


  display.clearDisplay();
  print_line("Alarm " + String(alarm) + " is Set!", 0, 0, 2);
}


// check temperature and humidity
void check_temp() {
  static int strValueLength = 10; 
  TempAndHumidity data = dhtSensor.getTempAndHumidity();

  if (data.temperature > MAX_TEMP) {
    display.clearDisplay();
    print_line("TEMP HIGH", 0, 40, 1);
    digitalWrite(TEMP_PIN, HIGH);
  }
  else if (data.temperature < MIN_TEMP) {
    display.clearDisplay();
    print_line("TEMP LOW", 0, 40, 1);
    digitalWrite(TEMP_PIN, HIGH);
  }
  else {
    digitalWrite(TEMP_PIN, LOW);
  }

  if (data.humidity > MAX_HUM) {
    display.clearDisplay();
    print_line("HUMIDITY HIGH", 0, 40, 1);
    digitalWrite(HUM_PIN, HIGH);
  }
  else if (data.humidity < MIN_HUM) {
    display.clearDisplay();
    print_line("HUMIDITY LOW", 0, 40, 1);
    digitalWrite(HUM_PIN, HIGH);
  }
  else {
    digitalWrite(HUM_PIN, LOW);
  }

  // publish those temerature and humidity values
  char strTemp[strValueLength];
  char strHumidity[strValueLength];

  String(data.temperature).toCharArray(strTemp, strValueLength);
  String(data.humidity).toCharArray(strHumidity, strValueLength);

  mqttClient.publish("SHAVIN-TEMP", strTemp);
  mqttClient.publish("SHAVIN-HUMIDITY", strHumidity);
}

// connect tp mqtt broker
void connectToMQTTBroker(){
  // loop until client is connected to broker
  while(!mqttClient.connected()){
    Serial.print("Attempting MQTT connection...");
    if(mqttClient.connect("ESP32-SHAVIN")){
      Serial.println("Connected");
      mqttClient.subscribe("SHAVIN-MIN-ANGLE");
      mqttClient.subscribe("SHAVIN-CONTROL-FACTOR");
      mqttClient.subscribe("SHAVIN-LDR-D");
      mqttClient.subscribe("SHAVIN-MAX-INTENSITY");
    }
    else{
      Serial.println("failed connect to broker");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}


// mqtt broker callback
void receiveCallback(char* topic, byte* payload, int length) {
  Serial.print("Topic published [");
  Serial.print(topic);
  Serial.println("]");

  // generate the string payload
  char strPayload[length + 1];
  generatePayload(payload, strPayload, length);
  float value = atof(strPayload);

  Serial.print("payload: ");
  Serial.println(strPayload);


  if (strcmp(topic, "SHAVIN-MIN-ANGLE") == 0) {
    min_angle = value;
  } else if (strcmp(topic, "SHAVIN-MAX-INTENSITY") == 0) {
    max_intensity = value;
  } else if (strcmp(topic, "SHAVIN-CONTRO-FACTOR") == 0) {
    control_factor = value;
  } else if (strcmp(topic, "SHAVIN-LDR-D") == 0) {
    LDR_D = value;
  }
}

void generatePayload(byte* payload, char* text, int length) {
  for (int i = 0; i < length; i++) {
    text[i] = (char) payload[i];
  }

  // insert the null character at the end of the char array
  text[length] = '\0';
}

// function for send the LDR values to the broker
void sendIntensities() {
  static int strLDRLength = 10;

  // read the analog input of the LDR pins
  int leftLDR = analogRead(LEFT_LDR_PIN);
  int rightLDR = analogRead(RIGHT_LDR_PIN);

  // convert int values into char arrays
  char strLeftLDR[strLDRLength];
  char strRightLDR[strLDRLength];
  String(leftLDR).toCharArray(strLeftLDR, strLDRLength);
  String(rightLDR).toCharArray(strRightLDR, strLDRLength);

  // Serial.print("LEFT LDR: ");
  // Serial.println(strLeftLDR);
  // Serial.print("RIGHT LDR: ");
  // Serial.println(strRightLDR);

  // publish those values into the broker
  mqttClient.publish("SHAVIN-LEFT-LDR", strLeftLDR);
  mqttClient.publish("SHAVIN-RIGHT-LDR", strRightLDR);

  delay(1000);

}

void slideMotorServo() {

  // calculate the motor angle by using below equation
  float motor_angle = min_angle * LDR_D + (180 - min_angle) * max_intensity * control_factor;
  if (motor_angle > 180) {
    motor_angle = 180;
  }


  motorServor.write((int) motor_angle);
  Serial.print("Servo rotate by angle: ");
  Serial.println(motor_angle);
  delay(500);

}