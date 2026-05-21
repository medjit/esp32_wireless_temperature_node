/* Enable esp32 native USB peripheral */
#define ARDUINO_USB_MODE 1
#define ARDUINO_USB_CDC_ON_BOOT 1

/* Config Temperature reading */
#define TEMPERATURE_REDING_DEBOUNCE 7

/* includes */
#include <Arduino.h>

/* Global Variables */
volatile float internalTempC = 0; /* Store measured temperature */

void setup(){
  /* Read internal temperature immediately after startup to prevent self heating during operation.*/
  for(byte count = 0; count < TEMPERATURE_REDING_DEBOUNCE; count++){
    internalTempC += temperatureRead(); 
  }
  internalTempC = internalTempC / TEMPERATURE_REDING_DEBOUNCE;

  delay(3000); /* Wait for serial connection from pc to be established. REMOVE FOR PRODUCTION TO SAVE BATTERY */

  /* Init serial port and send measured temperature */
  Serial.begin(115200);
  Serial.printf("Temperature: %.2f °C\n", internalTempC);
}

void loop(){
  //nothing to do for now.
}