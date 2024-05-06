//Tyler Carr ; Hardware&Software ; ILD
/*The circle and spiral are complex formulas that use the position of the LED and the distance from the origin and angle to produce their animation.
The formulas for these mathmatical equations were found in places like stackoverflow, but in order for it to work for my own setup I had to account for my physical layout and make it look how I wanted it to.
  https://stackoverflow.com/questions/283406/what-is-the-difference-between-atan-and-atan2-in-c
  https://stackoverflow.com/questions/61671642/how-to-draw-circle-with-sin-and-cos-in-c
  https://www.cuemath.com/geometry/180-degrees-to-radians/
  https://forum.arduino.cc/t/spiral-ws2812b-fastled-matrix-effect/576434
  https://stackoverflow.com/questions/1211212/how-to-calculate-an-angle-from-three-points
  https://stackoverflow.com/questions/2320986/easy-way-to-keeping-angles-between-179-and-180-degrees
  atan2 = gives angle value between -180 and 180

https://fastled.io/docs/struct_c_r_g_b.html
https://fastled.io/docs/group___trig.html#gaec8c5c0ed079727baa3b40184354ce41
https://fastled.io/docs/group___color_blends.html
https://github.com/FastLED/FastLED/wiki/High-performance-math#scaling
https://github.com/FastLED/FastLED/wiki/FastLED-Wave-Functions
https://github.com/FastLED/FastLED/wiki/Pixel-reference#crgb
https://www.waveshare.com/wiki/TF-Luna_LiDAR_Range_Sensor#Hardware_Connection_5
https://forum.arduino.cc/t/clearing-serial-buffer-solved/227853 This helped with the LiDAR crashing //might not need after optimizing
https://www.smartdraw.com/flowchart/flowchart-programming.htm
Within the FastLED library there is an example for syncing multiple pins and mapping the XY position and direction on a zigzag. https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples
*/

#include <FastLED.h>
#include <HardwareSerial.h>
HardwareSerial lidarSerial(2);
#define RXD2 13
#define TXD2 32
#define LED_PIN 0
#define LED_PIN1 1
#define LED_PIN2 2
#define LED_PIN3 3
#define WIDTH 39
#define HEIGHT 60 //height for four matrices stacked vertically. it's actually 39x15
#define NUM_LEDS (WIDTH * HEIGHT) //tot LEDs, but in matrix form
CRGB leds[NUM_LEDS]; 

enum State { ACTIVE, IDLE };
State currentState = ACTIVE;

unsigned long lastChange = 0;
const unsigned long idleTime = 18000000; //5 hours to enter IDLE
const unsigned long animationInterval = 15000; //15 seconds for each animation phase
double lastDistance = 0.0;
const double changeThreshold = 0.5; //threshold in feet, so this is 6 inches
//unsigned long measureTime = 0; //tracks the last time a distance was measured
//const unsigned long timeThreshold = 1000; //time threshold in milliseconds for color registering 

int position(int x, int y) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return -1; //this is to make sure that the animation is within the matrix. prior to using this fireworks used to brick the program as I was building from scratch, probably useful for others. Going to continue to check bounds,
  }
  int i;
  int quadrantHeight = 15;
  int quadrant = y / quadrantHeight; //logic; using global y to get the quadrant {0 59}: {0 14} / 15 = 0; {15 29} / 15 = 1; {30 44} / 15 = 2; {45 59} / 15 = 3;
  y = y % quadrantHeight; //logic; so, quadrant 0 {0 14}; 1 {15 29}; 2 {30 44}; 3 {45 59}. the purpose of this is to determine the position within the quadrant taking the modulous of each number within that range properly maps it out 0 to 14 for each quadrant. 
  switch (quadrant) {
    case 0: //first matrix  
    case 2: //third matrix
      if (y & 1) { //odd rows
        i = (y * WIDTH) + (WIDTH - 1 - x); //reversed
      } else { //even rows
        i = (y * WIDTH) + x; //forward
      }
      break;
    case 1: //second matrix
    case 3: //fourth matrix
      y = quadrantHeight - 1 - y; //adjust y for reversed start
      if (y & 1) { //odd rows
        i = (y * WIDTH) + x; //forward
      } else { //even rows
        i = (y * WIDTH) + (WIDTH - 1 - x); //reversed
      }
      break;
  }
  return i + (quadrant * quadrantHeight * WIDTH); //this is the final index calculated by adding the starting index of the quadrant to i.
}

void setup() { //addleds with multiple pins is within one of the FastLED examples, just had to alter for my setup
  FastLED.addLeds<WS2811, LED_PIN, RGB>(leds, NUM_LEDS / 4); //the first matrix 60tot/4 = 15 which is the actual size.
  FastLED.addLeds<WS2811, LED_PIN1, RGB>(leds + (NUM_LEDS / 4), NUM_LEDS / 4);  //this is the second matrix and this is second matrix where is includes the first with leds + (NUM_LEDS / 4), then accounts for itself with (NUM_LEDS / 4).
  FastLED.addLeds<WS2811, LED_PIN2, RGB>(leds + (NUM_LEDS * 2 / 4), NUM_LEDS / 4); //third matrix
  FastLED.addLeds<WS2811, LED_PIN3, RGB>(leds + (NUM_LEDS * 3 / 4), NUM_LEDS / 4); //fourth matrix
  FastLED.setBrightness(128);
  Serial.begin(115200);
  lidarSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
}

void loop() {
  static uint8_t buf[9]; //code to read the LiDAR data provided by the vendor at waveshare
  static double distance_ft = 0;  //for now i'm testing using inches, but will move to feet for final
  static bool sigChange = false; //checks for significant change in distance for smoother transitions on the active animation
  if (lidarSerial.available() >= 9) {
    lidarSerial.readBytes(buf, 9);
    if (buf[0] == 0x59 && buf[1] == 0x59) {
      uint16_t distance_cm = buf[2] + buf[3] * 256;
      distance_ft = distance_cm * 0.0328084;  //simple conversion to feet
      Serial.print("Distance: ");
      Serial.println(distance_ft);
    } //else {
    //no longer applies, but just incase I'll leave it here.  //while (lidarSerial.read() >= 0); //this basically dumps any remaining bytes in the buffer and forcefully reads new data. I was having an issue where the LiDAR would crash.
    //}
    if (fabs(distance_ft - lastDistance) > changeThreshold) {// && (currentTime - measureTime > timeThreshold)) {  //if the floating point absolute value of the (distance(in) - lastDistance register) is greater than the changeThreshold, then continue. This is used because the LiDAR will stay "Active" with the smallest change in centimeters and sometimes it will get a phatom reading never turning "Idle".
      lastDistance = distance_ft; //update lastDistance to the current measurement.
      currentState = ACTIVE; //when the current measurement updates the currentState is active.
      lastChange = millis(); //now lastChange will be used as a timestamp counting from the time the program started which will be used for animation timings and state timings.
      //measureTime = currentTime;
      sigChange = true;
    } else {
      lastDistance = distance_ft; //update lastDistance to the current measurement.
      currentState = ACTIVE; //when the current measurement updates the currentState is active.
      lastChange = millis(); //now lastChange will be used as a timestamp counting from the time the program started which will be used for animation timings and state timings.
      sigChange = false;
    }
  }

  unsigned long timeStamp = millis();
  switch (currentState) {
    case IDLE:
      if (timeStamp - lastChange < animationInterval) {
        FastLED.clear();
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        loading();
        //FastLED.clear();
      } else if (timeStamp - lastChange < animationInterval * 2) {
        dots();
        //FastLED.clear();
      } else if (timeStamp - lastChange < animationInterval * 3) {
        twinkleStar();
        //FastLED.clear();
      } else if (timeStamp - lastChange < animationInterval * 4) {
        fireworks();
        FastLED.clear();
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        //FastLED.clear();
      } else if (timeStamp - lastChange < animationInterval * 5) {
        raindrops();
      } else if (timeStamp - lastChange < animationInterval * 6) {
        spring();
        //FastLED.clear();
      } else if (timeStamp - lastChange < animationInterval * 7) {
        circle();
      } else if (timeStamp - lastChange < animationInterval * 8) {
        spiral();
      }
      else {
        lastChange = timeStamp;
      }
      break;
    case ACTIVE:
      if (timeStamp - lastChange > idleTime) { //this checks if the current time - the last timestamp is greater than 2 1/2 minutes which will then go back into the idle state.
        currentState = IDLE;
        lastChange = timeStamp;  //updating timestamp
      } else {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 50; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static CRGB color = CRGB::Black; //starting blank
  static float beat = 0; //starting blank
  static float waveLength = 0; //strating blank
  if (sigChange) { //if 6 inch threshold has been exceeded
  if (timeStamp - lastUpdate >= interval) {
  lastUpdate = timeStamp; //updates the last update time
  if (distance_ft <= 4 ) {
    color = CRGB(155, 0, 255);
    beat = 30;
    waveLength = 13;
  } else if (distance_ft <= 6) {
    color = CRGB::Blue;
    beat = 25;
    waveLength = 11;
  } else if (distance_ft <= 8) {
    color = CRGB::Aqua;
    beat = 20;
    waveLength = 9;
  } else if (distance_ft <= 10) {
    color = CRGB::Green;
    beat = 15;
    waveLength = 7;
  } else if (distance_ft <= 12) {
    color = CRGB(255, 125, 0);
    beat = 13;
    waveLength = 5;
  } else if (distance_ft <= 14) {
    color = CRGB(255, 50, 0);
    beat = 11;
    waveLength = 4;
  } else if (distance_ft <= 16) {
    color = CRGB::Red;
    beat = 9;
    waveLength = 3;
  } else {
    //else no input, keep last registered.
    }
  }
}
  sineWave(color, beat, waveLength);
      }
  }
}

void sineWave(CRGB color, float beat, float waveLength) {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 50; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static int x = 0; //tracks the current LED position in the width
  if (timeStamp - lastUpdate >= interval) {
  lastUpdate = timeStamp; //updates the last update time
  for (int i = 0; i < NUM_LEDS; i++) {
    float timeStamp = millis();
    float pulse = sin8(timeStamp * 0.3); //play around with. alters the speed and how long it last.
    float adjustedPulse = map(pulse, 0, 255, 50, 255); //min brightness, max brightness, my brightnesses (x, y)
    leds[i].nscale8(adjustedPulse);
  }
  for (int x = 0; x < WIDTH; x++) {
  float y = beatsin8(beat, 0, HEIGHT - 1, 0, x * waveLength); //x * waveLength to create a phase shift based on x
  leds[position(x, y)] = blend(leds[position(x, y)], color, 30); //play around with. these both control the wave output, so both can't be 0. Although, this controls a wave with the thickness of 1. 
    for (int Y = -3; Y <= 3; Y++) { //adjust for thickness. [0,0] is a single line. [-1, 1] is a line with a thickness of 3. [-2, 2] is a line with a thickness of 5...
      int newY = y + Y; //extends the height of the wave
      if (newY >= 0 && newY < HEIGHT) { //avoid going out of bounds
        leds[position(x, newY)] = blend(leds[position(x, newY)], color, 255); //play around with, the int is the brightness. these both control the wave output, so both can't be 0. Although, this controls a wave with the thickness of 3. How they work together is that I can have either one brighter and have a cool overlapping effect.
      }
    }
  }
  nscale8(leds, NUM_LEDS, 210); //play around with. this controls the distorted effect. it can never be removed, or set to zero since it controls the output of LEDs. However, it can be set very low (I would say 10 is the lowest) to basically produce a steady visual without distortion. The higher the number the more aubsurd trail. This also controls the brightness...?
  int blurAmount = 5; //play around with
  blur1d(leds, NUM_LEDS, blurAmount);
  FastLED.show();
  }
}

void dots()  {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 50; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static int x = 0; //tracks the current LED position in the width
  static int direction = 1; //current direction
  if (timeStamp - lastUpdate >= interval) {
    lastUpdate = timeStamp; //updates the last update time
    for (int y = 0; y < HEIGHT; y++) {
      leds[position(x, y)] = CRGB::White;
    }
    FastLED.show();
    FastLED.clear();
    x += direction; //updating position
    if (x == WIDTH || x == -1) { //checks if it needs to bounce
      FastLED.clear();
      direction *= -1; //bounces back switching direction
      x += direction; //move back in bounds
    }
  }
}

void loading() {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 500; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static int x = 0; //tracks the current LED position in the width
  if (timeStamp - lastUpdate >= interval) {
    lastUpdate = timeStamp; //updates the last update time
    for (int y = 0; y < HEIGHT; y++) {
      leds[position(x, y)] = CRGB::White;
    }
    FastLED.show();
    x++; //increments to move to the next position
    if (x >= WIDTH) {
      FastLED.clear(); //clears the trail of white LEDs
      x = 0; //reset to start once it reaches the end
    }
  }
}

void twinkleStar() {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 5; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static int brightness[NUM_LEDS] = {0};
    static bool increasing[NUM_LEDS] = {false};
  if (timeStamp - lastUpdate >= interval) {
    lastUpdate = timeStamp; //updates the last update time
    for (int i = 0; i < NUM_LEDS; ++i) { //fade all LEDs by a small amount
      if (brightness[i] > 0 && !increasing[i]) {
        brightness[i] -= 1; //decreases brightness if not increasing
      }
      leds[i] = CRGB(brightness[i], brightness[i], brightness[i]);
    }
    if (random(0, 1) == 0) { //randomly chooses LEDs to start twinkling 0 to 1
      int index = random(NUM_LEDS);
      increasing[index] = true; // sets an LED as increasing in brightness
      brightness[index] = 1; //increasing from a low value for a smooth transition
    }

    for (int i = 0; i < NUM_LEDS; ++i) { //increase brightness of LEDs set for twinkling
      if (increasing[i]) {
        if (brightness[i] < 255) {
          brightness[i] += 1; //ncirease for a visible brightening effect
        } else {
          increasing[i] = false; //at max brightness start fading
        }
      }
    }
    FastLED.show();
  }
}

void fireworks() {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 2000; //this is the replacement for delay()
  const unsigned long reset = 2500; //reset fireworks after the last explosion
  unsigned long timeStamp = millis();
  static const int fireworks = 9;
  static int X[fireworks], Y[fireworks];
  static CRGB colors[fireworks];
  static int radius = 0;
  if (timeStamp - lastUpdate > (radius == 10 ? reset : interval)) { //if radius reaches 10 (peak) hold at reset after 2.5 seconds, else proceed at the 2 second interval
    lastUpdate = timeStamp; //updates the last update time
    if (radius == 0) { //new fireworks
    FastLED.clear();
    for (int i = 0; i < fireworks; ++i) {
    X[i] = random(WIDTH);
    Y[i] = random(HEIGHT);
    colors[i] = CHSV(random8(), 255, 255);
  }
  for (int radius = 0; radius < 10; radius++) { //radius
    for (int f = 0; f < fireworks; ++f) { //each firework
      for (int angle = 0; angle < 360; angle += 8) { //denser explosions
        int x = X[f] + cos((angle * PI) / 180) * radius;
        int y = Y[f] + sin((angle * PI) / 180) * radius;
        int index = position(x, y);
        if (index >= 0 && index < NUM_LEDS) { //within bounds
          leds[index] = colors[f];
        }
      }
    }
    FastLED.show();
    fadeToBlackBy(leds, NUM_LEDS, 100); //fade effect
      }
    }
  }
}

void raindrops() {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 100; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static int raindrops[WIDTH];
  if (timeStamp - lastUpdate >= interval) {
  lastUpdate = timeStamp; //updates the last update time
  for (int x = 0; x < WIDTH; x++) { //moves each raindrop down one position, and create new ones randomly.
      if (random(0, 15) == 0) { //sets the probability then moves down. 0, 15) == 0)
        raindrops[x] = HEIGHT - 1; //comes from the very top before it even 
      } else {
        if (raindrops[x] >= 0) {//glass effect. if there is a raindrop in this column, move it down.
          if (raindrops[x] < HEIGHT) {
            leds[position(x, raindrops[x])] = CRGB::Black; //then erases the LED at the current position.
            raindrops[x] -= 3; //move the raindrop down.speed
            if (raindrops[x] >= 0) { //turns on the LED at the new position.
              leds[position(x, raindrops[x])] = CRGB::Blue;
            }
          } else {
            raindrops[x] = 0; //reset the raindrop position to indicate there's no raindrop in this column.
          }
        }
      }
    }
    FastLED.show();
  }
}

void spring() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastColumn = 0;
  const unsigned long interval = 50; //this is the replacement for delay()
  const unsigned long columnInterval = 2000; //time for column changes
  static int column1 = random(WIDTH - 30); //making sure theres room for 15 columns + space for the other rainbow
  static int column2 = (column1 + 16 + random(WIDTH - 46)) % WIDTH; //for the second column making sure some space between and they aren't placed on top of each other
  unsigned long timeStamp = millis();
  if (timeStamp - lastColumn >= columnInterval) {
    lastColumn = timeStamp;
    column1 = random(WIDTH - 30); //new position making sure its room for 15 columns + space
    column2 = (column1 + 16 + random(WIDTH - 46)) % WIDTH; //making sure theres some space between columns
  }
  if (timeStamp - lastUpdate >= interval) {
  lastUpdate = timeStamp; //updates the last update time
  FastLED.clear();
  for (int i = 0; i < 15; i++) { //loop to cover 15  LEDs wide for each column
        for (int y = 0; y < HEIGHT; y++) {
          if (column1 + i < WIDTH) { //check bounds for column 1
            int index = position(column1 + i, y);
            if (index != -1) leds[index] = CHSV(((y * 256 / HEIGHT) + timeStamp / 5) % 256, 255, 255); //color change
          }
          if (column2 + i < WIDTH) { //check bounds for column 2
            int index = position(column2 + i, y);
            if (index != -1) leds[index] = CHSV(((y * 256 / HEIGHT) + timeStamp / 5) % 256, 255, 255); //same color pattern
          }
        }
      }
    FastLED.show();
  }
}

void circle() {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 10; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static int hueChange = 0;
  if (timeStamp - lastUpdate >= interval) {
  lastUpdate = timeStamp; //updates the last update time
  hueChange++; //increment hueChange each interval
    FastLED.clear();
    int X = WIDTH / 2;
    int Y = HEIGHT / 2; 
    for (int x = 0; x < WIDTH; x++) {
      for (int y = 0; y < HEIGHT; y++) {
        float center = sqrt(sq(X - x) + sq(Y - y));
        int colorHue = int(center * 20 + hueChange) % 255; //hue change
        leds[position(x, y)] = CHSV(colorHue, 255, 255);
      }
    }
    FastLED.show();
  }
}

void spiral() {
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 10; //this is the replacement for delay()
  unsigned long timeStamp = millis();
  static int angleChange = 0;
  if (timeStamp - lastUpdate >= interval) {
  lastUpdate = timeStamp; //updates the last update time
  float X = (WIDTH - 1) / 2.0;
  float Y = (HEIGHT - 1) / 2.0;
    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        int i = position(x, y);
        float angle = atan2(y - Y, x - X) * 180 / PI;
          angle = fmod(angle + 360 + angleChange, 360);
          int colorHue = map(angle, 0, 360, 0, 255);
          leds[i] = CHSV(colorHue, 255, 255);
        }
      }
      FastLED.show();
      angleChange = (angleChange + 1) % 360;
    }
  }