#include <Arduino.h>
#include <analogWrite.h>
#include <LiquidCrystal.h>
#include <BigNumbers.h>
#include <DHT.h>


#define LCD_RS_PIN 12
#define LCD_EN_PIN 14
#define LCD_D4_PIN 26
#define LCD_D5_PIN 25
#define LCD_D6_PIN 33
#define LCD_D7_PIN 32
#define LCD_CONTRAST_PIN 27
#define ENCODER_CLK_PIN 16
#define ENCODER_DT_PIN 17
#define ENCODER_SW_PIN 18
#define DHT_PIN 15


struct
{
  struct
  {
    volatile int prev = 0;
    volatile long value = 0;
    volatile unsigned int lastread = 0;
    volatile bool debounce = false;
  } encoder;
  float temp = 0;
  float humid = 0;
  byte menu = 0; // 0:clock/backlight, 1:temp+humidity/contrast, 2:set hours, 3:set minutes
  int contrast = 10;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  unsigned long lastcheck = 0;
  unsigned long lastread = 0;
  bool refresh = true;
} data;


LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
BigNumbers bignum(&lcd);
DHT dht(DHT_PIN, DHT11);


void nextView()
{
  data.menu++;
  if (data.menu > 3)
  {
    data.menu = 0;
  }
}


void readData()
{
  if (millis() - 1000 > data.lastcheck && millis() > 1000)
  {
    data.lastcheck = millis();
    data.refresh = true;
    data.seconds++;
    if (data.seconds > 59)
    {
      data.minutes++;
      data.seconds = 0;
    }
    if (data.minutes > 59)
    {
      data.hours++;
      data.minutes = 0;
    }
  }
  if (millis() - 5000 > data.lastread && millis() > 5000)
  {
    data.lastread = millis();
    data.humid = dht.readHumidity();
    data.temp = dht.readTemperature();
  }
}


void decodeEncoder()
{
  int MSB = digitalRead(ENCODER_CLK_PIN); //MSB = most significant bit
  int LSB = digitalRead(ENCODER_DT_PIN);  //LSB = least significant bit


  int encoded = (MSB << 1) | LSB;               //converting the 2 pin value to single number
  int sum = (data.encoder.prev << 2) | encoded; //adding it to the previous encoded value


  data.encoder.prev = encoded; //store this value for next time
  if (data.encoder.lastread < millis() - 100)
  {
    data.encoder.lastread = millis();
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
      data.encoder.value++;


    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
      data.encoder.value--;
  }
}


bool encoderPush()
{
  if (!digitalRead(ENCODER_SW_PIN))
  {
    if (!data.encoder.debounce)
    {
      data.encoder.debounce = true;
      return true;
    }
  }
  else
  {
    data.encoder.debounce = false;
  }
  return false;
}


int clamp(int in, int min, int max)
{
  if (in > max)
    return max;
  else if (in < min)
    return min;
  return in;
}


int wrap(int in, int min, int max)
{
  if (in > max)
    return min;
  else if (in < min)
    return max;
  return in;
}


void setup()
{
  Serial.begin(9600);
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  // pinMode(DHT_PIN, INPUT_PULLUP);
  pinMode(LCD_CONTRAST_PIN, OUTPUT);
  analogWriteResolution(LCD_CONTRAST_PIN, 24);
  lcd.begin(16, 2);
  bignum.begin();
  dht.begin();
  lcd.clear();
  attachInterrupt(ENCODER_CLK_PIN, decodeEncoder, CHANGE);
  attachInterrupt(ENCODER_DT_PIN, decodeEncoder, CHANGE);
}


void loop()
{
  analogWrite(LCD_CONTRAST_PIN, data.contrast);
  if (encoderPush())
  {
    data.encoder.value = 0;
    nextView();
  }
  if (data.encoder.value != 0)
  {
    if (data.menu == 0)
    {
      // setting contrast
      data.contrast = clamp(data.contrast + (data.encoder.value * 2), 0, 255);
    }
    if (data.menu == 2)
    {
      // setting hours
      data.hours = wrap(data.hours + data.encoder.value, 0, 23);
    }
    if (data.menu == 3)
    {
      // setting minutes
      data.minutes = wrap(data.minutes + data.encoder.value, 0, 59);
    }
    data.refresh = true;
    data.encoder.value = 0;
  }
  readData();
  if (data.refresh)
  {
    data.refresh = false;
    Serial.print("Contrast:");
    Serial.println(data.contrast);
    lcd.clear();
    if (data.menu == 0 || data.menu == 2 || data.menu == 3)
    {
      // display clock
      bignum.displayLargeInt(data.hours, 0, 2, true);
      lcd.setCursor(6, 0);
      if (data.menu == 2)
      {
        lcd.print("<");
      }
      else
      {
        lcd.print(".");
      }
      lcd.setCursor(6, 1);
      if (data.menu == 3)
      {
        lcd.print(">");
      }
      else
      {
        lcd.print(".");
      }
      bignum.displayLargeInt(data.minutes, 7, 2, true);
      lcd.setCursor(13, 1);
      String sec = ".";
      if (data.seconds < 10)
      {
        sec = ".0";
      }
      sec.concat(data.seconds);
      lcd.print(sec);
      lcd.setCursor(14, 0);
      lcd.print(data.contrast);
    }
    else if (data.menu == 1)
    {
      lcd.clear();
      String out = String(data.temp, 0);
      out.concat(" C");
      lcd.setCursor(1, 0);
      lcd.print(out);
      out = String(data.humid, 0);
      out.concat(" %");
      lcd.setCursor(1, 1);
      lcd.print(out);
    }
  }
}
