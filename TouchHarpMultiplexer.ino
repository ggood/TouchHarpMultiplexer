/**

The Touch Harp is an instrument played like a harp, but uses
touch sensitive input on a Teensy 3 microcontroller.

The Teensy3 has 9 pins that can do touch sensing input. Since
a traditional harp has many more strings, we need some way
to allow more than 9 "strings" to be sensed.

An analog multiplexer chip like to CD4067 (1 x 16) allows one
touch input pin on the Teensy to handle 16 separate touch
inputs if you periodically select each input and record
the current value. To select which one of the 16 inputs on
the 4067 is active, you need to write values to the four
select pins on the chip, e.g.

S3 S2 S1 S0
 0  0  0  0 - input 0 is selected
 0  0  0  1 - input 1 is selected
 0  0  1  0 - input 2 is selected

and so on, up to

 1  1  1  1 - input 15 is selected
 
This skectch assumes the following connections between the
Teensy 3.0 pins and the input select pins on the analog
multiplexer:

Teensy  4067
======  ====
     2    10
     3    11
     4    14
     5    13

A datasheet for the 4067 is available online at:
https://www.sparkfun.com/datasheets/IC/CD74HC4067.pdf

You can think of the 4067 like an old school rotary
switch that connects an output pin to one of 16 input
pins. So connect the output pin of the 4067 to a
touch-input-capable pin (pin 23, aka A9 is used in this
sketch). Then connect your touch sensors to the inputs
on the 4067, labeled i0 through i15.

In this sketch, only the first 13 inputs on the 4067
are used, since that's how many "harp strings" my
instrument has.

The value that the Teensy TouchRead() method returns
can vary a lot depending on how much stray capacitance
is present in the system, so the treshold above which
we declare an input to be touched can be set via a
potentiometer which is read on analog input A0.

*/




// Touch input pin
#define TOUCH_IN A9

// LED output pins
#define LED0 13

// Multiplexer select pins
#define SEL0 2
#define SEL1 3
#define SEL2 4
#define SEL3 5

// Sensitivity adjustment input pin
#define SENS_IN A0

// Sample period - sample input no more often than this (in milliseconds)
#define SAMPLE_PERIOD 1
// How many samples to average
#define SAMPLE_BUFFER_SIZE 10
// Default reading from a touch sensor to consider a touch event
#define DEFAULT_TOUCH_THRESHOLD 5000

// Formatted print support, just for debug messages
#include <stdarg.h>
void p(char *fmt, ... ){
  char tmp[128]; // resulting string limited to 128 chars
  va_list args;
  va_start (args, fmt );
  vsnprintf(tmp, 128, fmt, args);
  va_end (args);
  Serial.print(tmp);
}

class TouchPin {
  /**
   * This class handles input on a single Teensy touch input pin, and does
   * de-glitching by keeping a ring buffer of input values. The value
   * returned by the value() method is a running average of the last
   * SAMPLE_BUFFER_SIZE samples, sampled every _sample_period milliseconds.
   * The caller is reponsible for calling the update() method at least once
   * every _sample_period milliseconds.
   */
  int _pin;  // Touch input pin on Teensy
  unsigned long _sample_period;  // Sample input every _sample_period millisconds
  unsigned long _last_sample_time;
  unsigned int _samples[SAMPLE_BUFFER_SIZE];  // Ring buffer of touchRead values
  unsigned int _index;  // Current index into ring buffer
  unsigned long _sum;  // Current sum
  int _select_line;  // If > 0, select this input pin on the multiplexer
  unsigned int _touch_threshold;  // Threshold for declaring a touched event

public:
  TouchPin(int, unsigned long);
  TouchPin(int, unsigned long, unsigned int);
  virtual void update();
  unsigned int value();
  boolean touching();
  void set_touch_threshold(unsigned int touch_threshold);
};

TouchPin::TouchPin(int pin, unsigned long sample_period) {
  _pin = pin;
  _sample_period = sample_period;
  _last_sample_time = 0UL;
  // _samples = {0};
  _index = 0;
  _sum = 0UL;
  _select_line = -1;
  _touch_threshold = DEFAULT_TOUCH_THRESHOLD;
}

TouchPin::TouchPin(int pin, unsigned long sample_period, unsigned int select_line) {
  _pin = pin;
  _sample_period = sample_period;
  _last_sample_time = 0UL;
  _index= 0;
  _sum = 0UL;
  _select_line = select_line;
  _touch_threshold = DEFAULT_TOUCH_THRESHOLD;
}

void TouchPin::update() {
  if (millis() > _last_sample_time + _sample_period) {
    if (_select_line >= 0) {
      // Select the appropriate input on the multiplexer
      digitalWrite(SEL0, _select_line & 0x01);
      digitalWrite(SEL1, (_select_line >> 1)  & 0x01);
      digitalWrite(SEL2, (_select_line >> 2)  & 0x01);
      digitalWrite(SEL3, (_select_line >> 3)  & 0x01);
    }
    // Remember the time we read this value
    _last_sample_time = millis();
    // Read the value
    unsigned int val = touchRead(_pin);
    // Update the running average
    _sum -= _samples[_index];
    _samples[_index] = val;
    _sum += val;
    _index = (_index + 1 ) % SAMPLE_BUFFER_SIZE;
  }
}

unsigned int TouchPin::value() {
  // Return the running average
  return _sum / SAMPLE_BUFFER_SIZE;
}

boolean TouchPin::touching() {
  // If the current value is above the touch threshold, return true
  return value() > _touch_threshold;
}

void TouchPin::set_touch_threshold(unsigned int touch_threshold) {
  // Set the threshold value - if the value we read is larger than
  // touch_threshold, we report a touch event.
  _touch_threshold = touch_threshold;
}


class HarpString: public TouchPin {
  /**
  A HarpString extends the TouchPin object and adds the concept of a "pluck"
  operation. A pluck happens when the string is touched (we enter the "armed"
  state) and them is released.
  
  TODO(ggood) implement the plucked() method
  */
  boolean _armed;
  
public:
  HarpString(int pin, unsigned long sample_period) : TouchPin(pin, sample_period) {};
  HarpString(int pin, unsigned long sample_period, unsigned int select_line) : TouchPin(pin, sample_period, select_line) {};
  void update();
  boolean plucked();
};

boolean HarpString::plucked() {
  return true;
}

void HarpString::update() {
  TouchPin::update();
  // TODO: record touch event. Record release event and time. If vibration period has expired, send note off. Or
  // maybe just expect caller to send note on and off immediately on detecting a pluck.
}

// The current prototype has 13 "strings", all attached to
// analog input 9 on the Teensy
HarpString strings[13] = {
  HarpString(A9, SAMPLE_PERIOD, 0),
  HarpString(A9, SAMPLE_PERIOD, 1),
  HarpString(A9, SAMPLE_PERIOD, 2),
  HarpString(A9, SAMPLE_PERIOD, 3),
  HarpString(A9, SAMPLE_PERIOD, 4),
  HarpString(A9, SAMPLE_PERIOD, 5),
  HarpString(A9, SAMPLE_PERIOD, 6),
  HarpString(A9, SAMPLE_PERIOD, 7),
  HarpString(A9, SAMPLE_PERIOD, 8),
  HarpString(A9, SAMPLE_PERIOD, 9),
  HarpString(A9, SAMPLE_PERIOD, 10),
  HarpString(A9, SAMPLE_PERIOD, 11),
  HarpString(A9, SAMPLE_PERIOD, 12),
};


void update_leds() {
  // Turn on LED0 if any string is being touched
  for (int i = 0; i < 13; i++) {
    if (strings[i].touching()) {
      digitalWrite(LED0,  HIGH);
      return;
    }
  }
  digitalWrite(LED0,  LOW);
}

// Record the current on/off state for each string
int note_state[13];

// Define the notes that sound when each string is touched
// or plucked. For now we have pentatonic, whole tone, and
// octatonic scales, but there's no way to change them while
// playing. Need to fix that...

unsigned int pentatonic_notes[13] = {
  60,
  62,
  64,
  67,
  69,
  72,
  74,
  76,
  79,
  81,
  84,
  86,
  88
};

unsigned int whole_tone_notes[13] = {
  60,
  62,
  64,
  66,
  68,
  70,
  72,
  74,
  76,
  78,
  80,
  82,
  84
};

unsigned int octatonic_notes[13] = {
  60,
  61,
  63,
  64,
  66,
  67,
  69,
  70,
  72,
  73,
  75,
  76,
  78
};

unsigned int *notes = octatonic_notes;

void setup() {
  for (int i = 0; i < 13; i++) {
    note_state[i] = -1L;
  }

  // Set LED output pins
  pinMode(LED0, OUTPUT);

  // Select pins for multiplexer
  pinMode(SEL0, OUTPUT);
  pinMode(SEL1, OUTPUT);
  pinMode(SEL2, OUTPUT);
  pinMode(SEL3, OUTPUT);
  
  // Input pin for sensitivity adjustment
  pinMode(SENS_IN, INPUT);
  Serial.begin(9600);

}

void loop() {
  // Reset touch threshold
  unsigned int sens_raw = analogRead(SENS_IN);
  unsigned int sens = map(sens_raw, 0, 1023, 4000, 8000);
  for (int i = 0; i < 13; i++) {
    strings[i].set_touch_threshold(sens);
    strings[i].update();
    if (strings[i].touching() && note_state[i] == -1) {
      // Note sounding - turn on
      Serial.println("ON");
      usbMIDI.sendNoteOn(notes[i], 100, 1);
      note_state[i] = millis();
    } 
    else if ((!(strings[i].touching())) && note_state[i] > -1) {
      // Note sounding - turn off
      note_state[i] = -1L;
      Serial.println("OFF");
      usbMIDI.sendNoteOff(notes[i], 100, 1);
    }
  }
}
