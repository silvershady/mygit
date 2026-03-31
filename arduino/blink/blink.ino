#include <Servo.h>

// Pins
#define trigPin 10
#define echoPin 11
#define buzzer 6
#define servoPin 9

Servo myServo;

long duration;
int distance;

// Function to get distance
int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000); // timeout
  distance = duration * 0.034 / 2;

  return distance;
}

// Buzzer logic
void handleBuzzer(int d) {
  if (d == 0 || d > 50) {
    noTone(buzzer);
  }
  else if (d > 30) {
    tone(buzzer, 800);
    delay(400);
    noTone(buzzer);
    delay(400);
  }
  else if (d > 15) {
    tone(buzzer, 1000);
    delay(200);
    noTone(buzzer);
    delay(200);
  }
  else {
    tone(buzzer, 1500); // continuous sound
  }
}

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzer, OUTPUT);

  myServo.attach(servoPin);

  Serial.begin(9600);
}

void loop() {

  // Sweep 0 → 180
  for (int pos = 0; pos <= 180; pos += 2) {
    myServo.write(pos);
    delay(20);

    int d = getDistance();

    // Send to Processing
    Serial.print(pos);
    Serial.print(",");
    Serial.println(d);

    handleBuzzer(d);
  }

  // Sweep 180 → 0
  for (int pos = 180; pos >= 0; pos -= 2) {
    myServo.write(pos);
    delay(20);

    int d = getDistance();

    // Send to Processing
    Serial.print(pos);
    Serial.print(",");
    Serial.println(d);

    handleBuzzer(d);
  }
}
