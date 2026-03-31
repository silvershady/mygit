import processing.serial.*;

Serial myPort;

String data = "";
int angle = 0;
int distance = 0;

void setup() {
  size(800, 600);
  
  printArray(Serial.list());
  myPort = new Serial(this, Serial.list()[0], 9600);
  myPort.bufferUntil('\n');
}

void draw() {
  background(0);

  translate(width/2, height);

  drawRadar();
  drawLine();
  drawObject();
}

void serialEvent(Serial myPort) {
  data = myPort.readStringUntil('\n');

  if (data != null) {
    data = trim(data);
    String[] values = split(data, ',');

    if (values.length == 2) {
      angle = int(values[0]);
      distance = int(values[1]);
    }
  }
}

void drawRadar() {
  stroke(0, 255, 0);
  noFill();

  for (int r = 100; r <= 400; r += 100) {
    arc(0, 0, r*2, r*2, PI, TWO_PI);
  }

  for (int a = 0; a <= 180; a += 30) {
    float x = 400 * cos(radians(a));
    float y = -400 * sin(radians(a));
    line(0, 0, x, y);
  }
}

void drawLine() {
  stroke(0, 255, 0);
  float x = 400 * cos(radians(angle));
  float y = -400 * sin(radians(angle));
  line(0, 0, x, y);
}

void drawObject() {

  if (distance < 50 && distance > 0) {

    float r = map(distance, 0, 50, 0, 400);
    float x = r * cos(radians(angle));
    float y = -r * sin(radians(angle));

    // 🔴 RED DETECTION WAVE
    noFill();
    stroke(255, 0, 0);

    for (int i = 0; i < 5; i++) {
      ellipse(x, y, i*15, i*15);
    }
  }
}
