
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>
#include <Stepper.h>

const int stepsPerRevolution = 2048;
const int maxSpeed = 15;

Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11);

// 16x2 I2C Liquid Crystal Display address: 0x27.
LiquidCrystal_I2C lcd(0x27,16,2); 

enum coins
{
  penny = 0,
  nickel,
  dime,
  quarter,
  num_coin_types
};

typedef struct {
  int debounceConstant; 
  int debounceDelay;
  int threshhold;
  int waitForDrop;
  int count;
}  SensorState;

// Global Variables
SensorState sensors[num_coin_types];

bool valueChanged = false;
unsigned long nextUpdate = 0;
char textBuffer[50];

const int resetButton = 2;  // Digital pin 2 connected to a momentary push-button

void setup() {
  pinMode(resetButton, INPUT_PULLUP);

  sensors[penny].threshhold = 700;
  sensors[nickel].threshhold = 700;
  sensors[dime].threshhold = 600;
  sensors[quarter].threshhold = 900;
  
  sensors[penny].debounceConstant = 40;
  sensors[nickel].debounceConstant = 50;
  sensors[dime].debounceConstant = 30;
  sensors[quarter].debounceConstant = 55;

  // Splash Screen
  lcd.init();
  lcd.backlight();
  lcd.print("Cont IPAC 2022");
  lcd.setCursor(0,1);
  lcd.print("Defaz - Herrera");
  delay(2000);

  // Setup the sensor interval.  Sensors will be polled at 1 kHz.  
  Timer1.initialize(1000);
  Timer1.attachInterrupt(pollSensors); 

  reset();

  myStepper.setSpeed(1);
  Serial.begin(9600);
  for (int s = 1; s <= 15 ; s++) {
    myStepper.setSpeed(s);
    myStepper.step(-s*2);
  }
  myStepper.setSpeed(maxSpeed);
}

void loop() {
myStepper.step(stepsPerRevolution);
  // Reset sensor states if the button is pressed.
  if( !digitalRead(resetButton)) {
    reset();
  }

  unsigned long currentTime = millis();
  
  if(currentTime > nextUpdate) {
    nextUpdate = currentTime + 200; // Update LCD at 5hz (every 200ms)

    // Only update the LCD if a coin has been detected.
    if(valueChanged) {
      updateLcd(); 
    }
  }
}

void pollSensors() {    // This gets called once per millisecond

  // Loop through each sensor
  for (int coinIndex = penny; coinIndex < num_coin_types; ++coinIndex)
  {
    // Get a reference to the sensor we're working with 
    SensorState &ss(sensors[coinIndex]);
    
    // If a coin has been detected recently, we go through a debounce waiting-period before we
    // check the sensor again.
    if (ss.debounceDelay > 0) {
      ss.debounceDelay--;

      // Once we reach zero, we set the waitForDrop delay
      // wait for 5 readings of LOW before the sensor is active again.
      if(ss.debounceDelay == 0) {
        ss.waitForDrop = 5;
      }
    } else {
      int sensorValue = getCoinSensorValue(coinIndex);
      
      // After the initial delay, we must have at least 5ms of sensor-low readings before
      // we start looking for coins again.
      
      if(sensorValue < ss.threshhold && ss.waitForDrop>0) {
        ss.waitForDrop--;
      }
      if (!ss.waitForDrop && sensorValue > ss.threshhold) {
        
        // New coin sensed
        valueChanged = true;
        ss.count++;
        ss.debounceDelay = ss.debounceConstant;
      }
    }
  }  
}

int getCoinSensorValue(int coinType)
{
  switch (coinType)  {
    case penny:   return analogRead(A0);
    case nickel:  return analogRead(A1);
    case dime:    return analogRead(A3);
    case quarter: return analogRead(A2);
  }
  return 0;
};

void reset() {
  for (int coinIndex = 0; coinIndex < num_coin_types; ++coinIndex)
  {
    SensorState &ss(sensors[coinIndex]);
    ss.debounceDelay = 0;
    ss.count = 0;
    ss.waitForDrop = 0;
  }
  updateLcd();
}

void updateLcd()
{
  int total =
      ( 1 * sensors[penny].count) +
      ( 5 * sensors[nickel].count) +
      (10 * sensors[dime].count) +
      (25 * sensors[quarter].count);

  int dollars = total / 100;
  int cents = total % 100;

  
  // Count the digits, if it more than 9, we have to use the alternate screen layout
  int digits = 4; // There's at least 1 digit required per sensor
  for (int coinIndex = penny; coinIndex < num_coin_types; ++coinIndex) {
    SensorState &ss(sensors[coinIndex]);
    if(ss.count >=10  ) digits++;
    if(ss.count >=100 ) digits++;
    if(ss.count >=1000) digits++;
  }

  if(digits<=9) {

    // This is the normal screen layout
    // ******************
    // *Total: $dd.cc   *
    // *Pxx Nxx Dxx Qxx *
    // ******************
    sprintf(textBuffer,"Total: $%2d.%02d      ", dollars, cents);
    lcd.setCursor(0,0);
    textBuffer[16] = 0;  // Null terminate to prevent too much data sent to LCD.
    lcd.print(textBuffer);

    sprintf(textBuffer,"P%d N%d D%d Q%d     ", 
       sensors[penny].count,
       sensors[nickel].count,
       sensors[dime].count,
       sensors[quarter].count);
    
    lcd.setCursor(0,1);
    textBuffer[16] = 0;  // Null terminate to prevent too much data sent to LCD.
    lcd.print(textBuffer); 
      
  } else { // Using alternate screen-layout to fit more digits. 
           // Up to 999 coins in each denomination.
    // ******************
    // *T: $ddd.cc Pxxx *
    // *Nxxx Dxxx Qxxx  *
    // ******************
    sprintf(textBuffer,"T: $2%d.%02d  P%d      ", dollars, cents, sensors[penny].count);
    lcd.setCursor(0,0);
    textBuffer[16] = 0;  // Null terminate to prevent too much data sent to LCD.
    lcd.print(textBuffer);

    sprintf(textBuffer,"N%d D%d Q%d      ", 
     sensors[nickel].count,
     sensors[dime].count,
     sensors[quarter].count);
    lcd.setCursor(0,1);
    textBuffer[16] = 0;  // Null terminate to prevent too much data sent to LCD.
    lcd.print(textBuffer); 
  }

  valueChanged = false;
};
