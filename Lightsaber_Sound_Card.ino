//Ethan Rawlins Custom Neopixel Lightsaber Driver

//sound/accelerometer logic credit goes to Brandon Sidle
//I2C library credit goes to Jeff Rowberg
//@ https://github.com/jrowberg/i2cdevlib

// FastLed is required for use of neopixel led strip.
#include <FastLED.h>
#include <I2Cdev.h>

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#include "Wire.h"

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
#include "MPU6050.h"

// SD Card reader
#include "SD.h"
#include "TMRpcm.h"
#include "SPI.h"

TMRpcm audio;

#define SD_CARD_SELECT_PIN 2 // arduino pin wired to sd card
#define LED_DATA_PIN 3 // arduino pin wired to led strip data
#define SPEAKER_PIN 4 // arduino pin wired to speaker
#define BUTTON_PIN  5 // arduino pin wired to push button

#define NUM_LEDS 124

CRGB leds[NUM_LEDS];

#define TURN_ON_SPEED 40 // speed (microseconds) for led crawl up the blade when turned on
#define TURN_OFF_SPEED 40 // speed (microseconds) for led crawl down the blade when turned off

MPU6050 accelgyro; // Initializing accelerometer library?
int16_t ax, ay, az; // setting axes for accelerometer
int16_t gx, gy, gz; // setting axes for accelerometer

int lastReading = LOW;
int hold = 0;
int single = 0;

boolean on = false;

long onTime = 0;
long lastSwitchTime = 0;

int bladeFillType = 0;

int solidColorIndex = 0; // Index (counter/placeholder) for RGB solid color array

int paletteIndex = 0; // Index (counter/placeholder) for RGB palette color
int gradientIndex = 0; // Index for starting gradient color on blade
#define GRADIENT_MOVE_SPEED 10 // speed (milliseconds) for how fast the color gradient moves on the blade when bladeFillType = 2

// gradient colors (bladeFillType = 1 or 2)
#define NUM_GRADIENT_PALETTES 2 // Number of gradient palettes
DEFINE_GRADIENT_PALETTE (gradient_1) {
  0, 0, 0, 255, // blue
  255, 200, 0, 200 // purple
};
DEFINE_GRADIENT_PALETTE (gradient_2) {
  0, 0, 0, 0, // black
  128, 255, 0, 0, // red
  200, 255, 255, 0, // yellow
  255, 255, 255, 255 // white
};
DEFINE_GRADIENT_PALETTE (rainbow_gradient) {
  0, 255, 0, 0, // red
  43, 255, 30, 0, // orange
  86, 255, 200, 0, // yellow
  129, 0, 255, 0, // green
  172, 0, 0, 255, // blue
  215, 200, 0, 200, // purple
  255, 255, 0, 0 // red again
};
  

void setup() {
// join I2C bus (I2Cdev library doesn't do this automatically)
  Wire.begin();
  
  audio.speakerPin = SPEAKER_PIN;
  Serial.begin(9600);
  if (!SD.begin(SD_CARD_SELECT_PIN)) {
  Serial.println("SD fail");
  return;
  }
  
  audio.setVolume(5);
  audio.play("Voyage .wav");

  pinMode(BUTTON_PIN, INPUT);
// initialize serial communication
  Serial.begin(38400);

// initialize device
  Serial.println("Initializing I2C devices...");
//Begin Accelerometer Code
  accelgyro.initialize();

// verify connection
  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
 
// configure fastLED
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  FastLED.clear();
  FastLED.show();
}


void loop() {
  
// read raw accel/gyro measurements from device
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
// these methods (and a few others) are also available
//accelgyro.getAcceleration(&ax, &ay, &az);
//accelgyro.getRotation(&gx, &gy, &gz);

// Solid colors (bladeFillType = 0)
  int numSolidColors = 8; // Number of solid colors
  CRGB blue = CRGB(0, 0, 255); // RGB code for blue
  CRGB red = CRGB(255, 0, 0); // RGB code for red
  CRGB pink = CRGB(255, 0, 70); // RGB code for pink
  CRGB purple = CRGB(200, 0, 200); // RGB code for purple
  CRGB green = CRGB(0, 255, 0); // RGB code for green
  CRGB cyan = CRGB(0, 255, 255); // RGB code for teal
  CRGB orange = CRGB(255, 30, 0); // RGB code for orange
  CRGB yellow = CRGB(255, 200, 0); // RGB code for yellow
  CRGB solidColors[numSolidColors] = {blue, red, pink, purple, green, cyan, orange, yellow}; // Array for blade colors

  int holdTime = 250; // How long you have to hold button down for long press
  int doubleTime = 250; // Time between double presses
  int bounceTime = 50; // debounce time for button press
  int reading = digitalRead(BUTTON_PIN);

// Button first pressed
  if (reading == HIGH && lastReading == LOW) {
    onTime = millis();
  }

// Button held
  if (reading == HIGH && lastReading == HIGH) {
    if ((millis() - onTime) > holdTime) { // Button held down
      if(hold != 1) {
        if (on == false) {
          turnOn(solidColors);
        }
        else {
          turnOff();
        }
      }
      hold = 1;
    }
  }

// Button released
  if (reading == LOW && lastReading == HIGH) {
    if (((millis() - onTime) > bounceTime) && hold != 1) {
      onRelease(doubleTime);
    }
    if (hold == 1) {
      hold = 0;
    }   
  }
  lastReading = reading;
// Button single press
  if (single == 1 && (millis() - lastSwitchTime) > doubleTime) {
    singlePress(numSolidColors);
  }
  if (on == true) {
    fillBlade(bladeFillType, solidColors);
    FastLED.show();
    if (bladeFillType == 3) {
      rainbowHum();
    }
    else {
      hum();
    }
// Drive clash sensor
    if(abs(ax) > 32600 || abs(ay) > 32600 || 
    abs(az) > 32600){
      clash(bladeFillType, solidColors);
    }

// Drive swing sensor
    if((abs(ax) > 26000 && abs(ax) < 26000) || 
    (abs(ay) > 26000 && abs(ay) < 32600) || 
    (abs(az) > 26000 && abs(az) < 32600)){
      swing();
    }
  }
}

void onRelease(int doubleTime) {

  if ((millis() - lastSwitchTime) >= doubleTime) {
    single = 1;
    lastSwitchTime = millis();
    return;
  }  

// Button double press
  if ((millis() - lastSwitchTime) < doubleTime) {
    doublePress();
  }
}

void fillBlade(int fillType, CRGB solidColors) {
  CRGBPalette16 bluePurplePal = gradient_1;
  CRGBPalette16 heatmapPal = gradient_2;
  CRGBPalette16 rainbowGradient = rainbow_gradient;
  CRGBPalette16 gradientColors[NUM_GRADIENT_PALETTES] = {bluePurplePal, heatmapPal};
  
  if (fillType == 0) {
// Solid color blade fill
    fill_solid(leds, NUM_LEDS, solidColors[solidColorIndex]);
  }
  else if (fillType == 1) {
// gradient color blade fill
    gradientIndex = 0;
    fill_palette(leds, NUM_LEDS, gradientIndex, 255 / NUM_LEDS, gradientColors[paletteIndex], 255, LINEARBLEND);
  }
  else if (fillType == 2) {
// moving gradient color blade fill
    fill_palette(leds, NUM_LEDS, gradientIndex, 255 / NUM_LEDS, gradientColors[paletteIndex], 255, LINEARBLEND);
    EVERY_N_MILLISECONDS(GRADIENT_MOVE_SPEED) {
      gradientIndex--;
    }
  }
  else if (fillType == 3) {
    fill_palette(leds, NUM_LEDS, gradientIndex, 255 / NUM_LEDS, rainbowGradient, 255, LINEARBLEND);
    EVERY_N_MILLISECONDS(1) {
      gradientIndex = gradientIndex - 10;
    }
  }
  FastLED.show();  
}

void turnOn(CRGB solidColors) {
  on = true;
  bladeFillType = 0;
  audio.play("turn_on.wav"); //turn on sound
  delay(25);
  for (int i = 0; i < NUM_LEDS - 1; i++) {
    leds[i] = solidColors[solidColorIndex];
    FastLED.show();
    delayMicroseconds(TURN_ON_SPEED);
  }
  audio.pause(); // turn off turn on sound
}

void hum() {
  audio.pause();
  audio.play("hum.wav");
}

void rainbowHum() {
  audio.pause();
  audio.play("rainbowHum.wav");
}

void turnOff() {
  on = false;
  audio.pause(); // turn off hum sound
  audio.play("turnOff.wav"); // turn off sound
  delay(25);
  audio.pause(); // turn off turn off sound
  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    leds[i] = CRGB(0, 0, 0);
    FastLED.show();
    delayMicroseconds(TURN_OFF_SPEED);
  }
}

void clash(int fillType, CRGB solidColors) {
  audio.pause();
  audio.play("clash.wav");
  // Add fastLED code here for flash on clash     *******************************************    CLASH LEDS HERE ***********************************
  fill_solid(leds, NUM_LEDS, CRGB(255, 255, 200));
  FastLED.show();

  delay(500);
  audio.pause();
  fillBlade(fillType, solidColors);
  FastLED.show();
}

void swing() {
  audio.pause();
  audio.play("swing.wav");
  delay(25);
  audio.pause();
}

void singlePress(int numSolidColors) {
  if (bladeFillType == 0) {
    if (solidColorIndex < numSolidColors - 1) {
      solidColorIndex++;
    }
    else {
      solidColorIndex = 0;
    }
  }
  else {
    if (paletteIndex < NUM_GRADIENT_PALETTES - 1) {
      paletteIndex++;
    }
    else {
      paletteIndex = 0;
    }
  }
  single = 0;
}

void doublePress() {
  if (bladeFillType == 0) {
    bladeFillType = 1;
  }
  else if (bladeFillType == 1) {
    bladeFillType = 2;
  }
  else if (bladeFillType == 2) {
    bladeFillType = 3;
  }
  else {
    bladeFillType = 0;
  }
  single = 0;
  lastSwitchTime = millis();
}
