/*
  Maya's bedside lamp by Jasminl - 2016

  A LED mood lamp wth various effects and settings made by a dad for his daughter
  https://github.com/jasminl303/Lampe
  
  Uses: 
        60 x WS2812 LEDS
        60 x APA102 WLEDS
        4 x Pots (Mode, Color, Speed and Brightness)
        1 x Arduino Nano & breakout
        1 x IKEA Fado lampshade
        1 x Custom 3d printed case

  Based on:

  DemoReel100 and other FastLed examples
    https://github.com/FastLED/FastLED
  MoodOrb by Mario Keller
    https://github.com/mkeller0815/MoodOrb
  NeoPixelPlasma by John Ericksen
    https://github.com/johncarl81/neopixelplasma
  Various color palettes available via cpt-city
    http://soliton.vm.bytemark.co.uk/pub/cpt-city

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses/.

*/
#include "FastLED.h"
#include "Palettes.h"

FASTLED_USING_NAMESPACE

//////////////////////////////////////////////////////////////////////////////////////////////////
// HEADER
//

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define LED_PIN     2
#define LED_DATAPIN    4
#define LED_CLOCKPIN   3
#define COLOR_ORDER GRB
#define CHIPSET     WS2812B
#define WCHIPSET     APA102
#define MAX_BRIGHTNESS  255
#define SERIALDEBUG 0
#define FRAMES_PER_SECOND  60
#define MODE_PIN     A0
#define COLOR_PIN    A1
#define SPEED_PIN    A2
#define BRIGHT_PIN   A3
const uint8_t kMatrixWidth  = 10;
const uint8_t kMatrixHeight = 6;
const bool    kMatrixSerpentineLayout = true;

// Functions prototypes
void setMode();
void setColor();
void setSpeed();
void setBright();
void setPalette();
void setAllPots();
void showStrip();
void debugPots();
// Effects
void whiteonly();
void wconfetti();
void solidcolor();
void rainbow();
void confetti();
void sinelon();
void juggle();
void Fire2012();
void Plasma();
void fillnoise8();
void mapNoiseToLEDsUsingPalette();
uint16_t XY( uint8_t x, uint8_t y);
void Noise();
// end prototypes

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)

// For Fire2012
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
uint8_t COOLING = 55;
// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
uint8_t SPARKING = 60;

// For Plasma
float phase = 0.0;
float phaseIncrement = 0.01;  // Controls the speed of the moving points. Higher == faster. I like 0.08 ..03 change to .02
float colorStretch = 0.11;    // Higher numbers will produce tighter color bands. I like 0.11 . ok try .11 instead of .03

// Convenient 2D point structure
struct Point {
  float x;
  float y;
};

// For NoisePalette
static uint16_t x;
static uint16_t y;
static uint16_t z;
// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward.  Try
// 1 for a very slow moving effect, or 60 for something that ends up looking like
// water.
uint16_t speed = 20; // speed is set dynamically once we've started up
// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise iwll be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
uint16_t scale = 30; // scale is set dynamically once we've started up
// This is the array that we keep our computed noise values in
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];
uint8_t colorLoop = 0;


// WORK VARIABLES
CRGB leds[kMatrixWidth * kMatrixHeight];
CRGB wleds[kMatrixWidth * kMatrixHeight];
int a0, a1, a2, a3 = 0;
int g_mode, g_color, g_speed, g_brightness = 0;
int old_g_mode, old_g_color, old_g_speed, old_g_brightness = -1;
uint8_t g_hue = 0; // rotating "base color" used by many of the patterns
bool wled_strip_toggled = false;  // if the wled strip is toggled, APA10x are off by default
CRGBPalette16 currentPalette( Coral_reef_gp );

//////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP
//

void setup() {
  delay(100); // 1 second delay for recovery
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WCHIPSET, LED_DATAPIN, LED_CLOCKPIN, RGB, DATA_RATE_MHZ(12)>(wleds, NUM_LEDS);
  // Disable temporal dithering.. breaks Plasma & Noise
  FastLED.setDither(0);
  if ( SERIALDEBUG > 0) {
    Serial.begin(9600);
  }
  // Initialize our coordinates to some random values
  x = random16();
  y = random16();
  z = random16();
  // Call our pot routines a bunch of time so they stabilise before loop()
  for ( int TRASH_ADC_READS = 0;  TRASH_ADC_READS < 5; TRASH_ADC_READS++) {
    a0 = analogRead(MODE_PIN);
    a1 = analogRead(COLOR_PIN);
    a2 = analogRead(SPEED_PIN);
    a3 = analogRead(BRIGHT_PIN);
  }
  setAllPots();
}

//
// MAIN
//
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*ModeList[])();
ModeList Modes = { whiteonly, wconfetti, solidcolor, rainbow, confetti, sinelon, juggle, Fire2012, Plasma, Noise};

uint8_t MaxModes = ARRAY_SIZE (Modes);

void loop() {
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( random());
  // Call the current pattern function once, updating the 'leds' array
  Modes[g_mode]();
  // send the 'leds' array out to the actual LED strip

  EVERY_N_MILLISECONDS( 20 ) {
    g_hue++;
  }
  EVERY_N_MILLISECONDS( 30 ) {
    setPalette();
  }
  EVERY_N_MILLISECONDS( 20 ) {
    setMode();
  }
  EVERY_N_MILLISECONDS( 40 ) {
    setColor();
  }
  EVERY_N_MILLISECONDS( 40 ) {
    setSpeed();
  }
  EVERY_N_MILLISECONDS( 20 ) {
    setBright();
  }
  EVERY_N_SECONDS( 1 ) {
    if ( SERIALDEBUG > 0) {
      debugPots();
    }
  }
  FastLED.setBrightness(g_brightness);
  showStrip();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// UTILITY FUNCTIONS
//

int readAnalog(int prevvalue, int pin) {
  prevvalue = ( prevvalue * 7) + analogRead(pin);
  prevvalue = prevvalue / 8;
  return prevvalue;
}

void setMode() {
  a0 = readAnalog(a0, MODE_PIN);
  // full integer filter, maxmods is too low for abs filter.
  int value = a0;
  // Filtering
  if (abs(old_g_mode - value) > 1 ) {
    old_g_mode = value;
    g_mode = map(a0, 0, 1023, 0, MaxModes);
  }
}

void setColor() {
  a1 = readAnalog(a1, COLOR_PIN);
  int value =  map(a1, 0, 1023, 0, 255);
  // Filtering
  if (abs(old_g_color - value) > 1 ) {
    old_g_color = value;
    g_color = value;
  }
}

void setSpeed() {
  a2 = readAnalog(a2, SPEED_PIN);
  int value =  map(a2, 0, 1023, 0, 255);
  // Filtering
  if (abs(old_g_speed - value) > 1 ) {
    old_g_speed = value;
    g_speed = value;
  }
}

void setBright() {
  a3 = readAnalog(a3, BRIGHT_PIN);
  int value =  map(a3, 0, 1023, 1, 255);
  // Filtering
  if (abs(old_g_brightness - value) > 1 ) {
    old_g_brightness = value;
    g_brightness = value;
    //FastLED.setBrightness(g_brightness);
  }
}

void setPalette() {
  currentPalette = gGradientPalettes[map (g_color, 0, 255, 0, gGradientPaletteCount)];
}

void setAllPots() {
  setMode();
  setColor();
  setSpeed();
  setBright();
  setPalette();
}

void debugPots() {
  // TODO: https://gist.github.com/philippbosch/5395696
  //sprintf (stringbuffer, "%");
  Serial.print(g_mode);
  Serial.print("\t");
  Serial.print(a0);
  Serial.print("\t");
  Serial.print(g_color);
  Serial.print("\t");
  Serial.print(a1);
  Serial.print("\t");
  Serial.print(g_speed);
  Serial.print("\t");
  Serial.print(a2);
  Serial.print("\t");
  Serial.print(g_brightness);
  Serial.print("\t");
  Serial.print(a3);
  Serial.print("\n");
}

void showStrip() {
#ifdef ADAFRUIT_NEOPIXEL_H
  // NeoPixel
  strip.show();
#endif
#ifndef ADAFRUIT_NEOPIXEL_H
  // FastLED
  FastLED.show();
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MODE FUNCTIONS
//

void whiteonly ()
{
  // Fill the disabled LED strip only once
  if (wled_strip_toggled = false)
  {
    fill_solid (leds , NUM_LEDS, CRGB::Black);
    wled_strip_toggled = true;
  }
  fill_solid (wleds , NUM_LEDS, CRGB::White);
}

void wconfetti()
{
  // Fill the disabled LED strip only once
  if (wled_strip_toggled = true)
  {
    fill_solid (leds , NUM_LEDS, CRGB::Black);
    wled_strip_toggled = true;
  }
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( wleds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  wleds[pos] += CHSV( g_hue + random8(64), 200, 255);
}

void solidcolor()
{
  // Fill the disabled LED strip only once
  if (wled_strip_toggled = true)
  {
    fill_solid (wleds , NUM_LEDS, CRGB::Black);
    wled_strip_toggled = false;
  }
  fill_solid (leds, NUM_LEDS, CHSV( g_color, 200, 255) );
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, g_hue, g_color / 8);
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( g_hue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS);
  leds[pos] += CHSV( g_hue, 255, 192);
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, g_speed / 8);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void Fire2012()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];
  //COOLING = g_speed / 2;
  SPARKING = g_speed / 2;

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    byte colorindex = scale8( heat[j], 240);
    CRGB color = ColorFromPalette( currentPalette, colorindex);
    int pixelnumber;
    pixelnumber = j;
    leds[pixelnumber] = color;
  }
}

// PLASMA
void Plasma() {
  phase += phaseIncrement;
  byte row, col;
  // The two points move along Lissajious curves, see: http://en.wikipedia.org/wiki/Lissajous_curve
  // We want values that fit the LED grid: x values between 0..13, y values between 0..8 .
  // The sin() function returns values in the range of -1.0..1.0, so scale these to our desired ranges.
  // The phase value is multiplied by various constants; I chose these semi-randomly, to produce a nice motion.
  Point p1 = { (sin(phase * 1.000) + 1.0) * 4.5, (sin(phase * 1.310) + 1.0) * 4.0 };
  Point p2 = { (sin(phase * 1.770) + 1.0) * 4.5, (sin(phase * 2.865) + 1.0) * 4.0 };
  Point p3 = { (sin(phase * 0.250) + 1.0) * 4.5, (sin(phase * 0.750) + 1.0) * 4.0 };

  // For each row...
  for ( row = 0; row < kMatrixHeight; row++ ) {
    float row_f = float(row);  // Optimization: Keep a floating point value of the row number, instead of recasting it repeatedly.

    // For each column...
    for ( col = 0; col < kMatrixWidth; col++ ) {
      float col_f = float(col);  // Optimization.

      // Calculate the distance between this LED, and p1.
      Point dist1 = { col_f - p1.x, row_f - p1.y };  // The vector from p1 to this LED.
      float distance1 = sqrt( dist1.x * dist1.x + dist1.y * dist1.y );

      // Calculate the distance between this LED, and p2.
      Point dist2 = { col_f - p2.x, row_f - p2.y };  // The vector from p2 to this LED.
      float distance2 = sqrt( dist2.x * dist2.x + dist2.y * dist2.y );

      // Calculate the distance between this LED, and p3.
      Point dist3 = { col_f - p3.x, row_f - p3.y };  // The vector from p3 to this LED.
      float distance3 = sqrt( dist3.x * dist3.x + dist3.y * dist3.y );

      // Warp the distance with a sin() function. As the distance value increases, the LEDs will get light,dark,light,dark,etc...
      // You can use a cos() for slightly different shading, or experiment with other functions. Go crazy!
      float color_1 = distance1;  // range: 0.0...1.0
      float color_2 = distance2;
      float color_3 = distance3;
      float color_4 = (sin( distance1 * distance2 * colorStretch )) + 2.0 * 0.5;

      // Square the color_f value to weight it towards 0. The image will be darker and have higher contrast.
      color_1 *= color_1 * color_4;
      color_2 *= color_2 * color_4;
      color_3 *= color_3 * color_4;
      color_4 *= color_4;

      // Scale the color up to 0..7 . Max brightness is 7.
      //strip.setPixelColor(col + (8 * row), strip.Color(color_4, 0, 0) );
      // For Neopixel.h
      //strip.setPixelColor(y + (maxcol * x), strip.Color(color_1, color_2, color_3));
      leds[col + (kMatrixWidth * row)] = CRGB (color_1, color_2, color_3);
      if ( SERIALDEBUG > 0) 
      {
        Serial.print(row_f);
        Serial.print("\t");
        Serial.print(col_f);
        Serial.print("\t");
        Serial.print(col + (kMatrixWidth * row));
        Serial.print("\t");
        Serial.print(col * row);
        Serial.print("\n");
      }
    }
  }
}

// NOISE

// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  // If we're runing at a low "speed", some 8-bit artifacts become visible
  // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
  // The amount of data smoothing we're doing depends on "speed".
  uint8_t dataSmoothing = 0;
  if ( speed < 50) {
    dataSmoothing = 200 - (speed * 4);
  }

  for (int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    for (int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = scale * j;

      uint8_t data = inoise8(x + ioffset, y + joffset, z);

      // The range of the inoise8 function is roughly 16-238.
      // These two operations expand those values out to roughly 0..255
      // You can comment them out if you want the raw noise data.
      data = qsub8(data, 16);
      data = qadd8(data, scale8(data, 39));

      if ( dataSmoothing ) {
        uint8_t olddata = noise[i][j];
        uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
        data = newdata;
      }

      noise[i][j] = data;
    }
  }

  z += speed;

  // apply slow drift to X and Y, just for visual variation.
  x += speed / 8;
  y -= speed / 16;
}

void mapNoiseToLEDsUsingPalette()
{
  static uint8_t ihue = 0;

  for (int i = 0; i < kMatrixWidth; i++) {
    for (int j = 0; j < kMatrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.

      uint8_t index = noise[j][i];
      uint8_t bri =   noise[i][j];

      // if this palette is a 'loop', add a slowly-changing base value
      if ( colorLoop) {
        index += ihue;
      }

      // brighten up, as the color palette itself often contains the
      // light/dark dynamic range desired
      //      if ( bri > 127 ) {
      //        bri = 255;
      //      } else {
      //        bri = dim8_raw( bri * 2);
      //      }

      //CRGB color = ColorFromPalette( currentPalette, index, bri);
      CRGB color = ColorFromPalette( currentPalette, index, bri);
      leds[XY(i, j)] = color;
    }
  }

  ihue += 1;
}

//
// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
//
uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  if ( kMatrixSerpentineLayout == false) {
    i = (y * kMatrixWidth) + x;
  }
  if ( kMatrixSerpentineLayout == true) {
    if ( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (kMatrixWidth - 1) - x;
      i = (y * kMatrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * kMatrixWidth) + x;
    }
  }
  return i;
}

void Noise()
{
  speed = g_speed / 12;
  scale = 90;
  // Override of colorLoop
  // colorLoop = 0;
  // generate noise data
  fillnoise8();
  // convert the noise data to colors in the LED array
  // using the current palette
  mapNoiseToLEDsUsingPalette();
  // Will show in main loop
}
