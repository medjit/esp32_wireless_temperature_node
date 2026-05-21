/* Enable esp32 native USB peripheral */
#define ARDUINO_USB_MODE          1
#define ARDUINO_USB_CDC_ON_BOOT   1

/* Define pinout */
#define BATTERY_VOLTAGE_SENS_GPIO A0
#define POWER_VOLTAGE_SENS_GPIO   A1

/* Define voltage deviders */
#define VOLTAGE_DEVIDER_BATTERY_TOP_RESISTOR      220000.0
#define VOLTAGE_DEVIDER_BATTERY_BOTTOM_RESISTOR   220000.0
#define VOLTAGE_DEVIDER_POWER_TOP_RESISTOR        220000.0
#define VOLTAGE_DEVIDER_POWER_BOTTOM_RESISTOR     220000.0

/* ADC reference voltage */
#define ADC_REFERENCE_VOLTAGE  3.3

/* ESP32-C6 ADC resolution */
#define ADC_MAX_VALUE          4095.0

/* Config Temperature reading */
#define TEMPERATURE_REDING_DEBOUNCE 7

/* includes */
#include <Arduino.h>

/* Global Variables */
struct DataPacket_t {
  float internalTempC;    /* Store measured temperature */
  float batteryVoltage;   /* Store measured battery voltage */
  float powerVoltage;     /* Store measured power supply voltage */
} dataPacket;


/**
 * @brief Read and calculate the real input voltage from a voltage divider.
 *
 * This function reads the ADC value from the specified GPIO pin,
 * converts the raw ADC reading into the measured ADC voltage,
 * then reconstructs the original input voltage using the
 * voltage divider resistor values.
 *
 * Voltage divider formula:
 *
 * Vout = Vin * (Rbottom / (Rtop + Rbottom))
 *
 * Rearranged to calculate the original voltage:
 *
 * Vin = Vout * ((Rtop + Rbottom) / Rbottom)
 *
 * @param pin
 * ADC GPIO pin connected to the voltage divider output.
 *
 * @param rTop
 * Top resistor value in ohms.
 * Connected between the measured voltage source and ADC output.
 *
 * @param rBottom
 * Bottom resistor value in ohms.
 * Connected between ADC output and GND.
 *
 * @return float
 * Calculated real input voltage in volts.
 */
float readVoltage(uint8_t pin, float rTop, float rBottom)
{
    /* Read raw ADC value */
    uint16_t adcRaw = analogRead(pin);

    /*
     * Convert raw ADC reading into actual ADC voltage.
     *
     * Example:
     * 0    -> 0.0V
     * 4095 -> 3.3V
     */
    float adcVoltage =
        ((float)adcRaw / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;

    /*
     * Recalculate original voltage before the voltage divider.
     *
     * Divider equation:
     *
     * Vin = Vout * ((Rtop + Rbottom) / Rbottom)
     */
    float realVoltage =
        adcVoltage * ((rTop + rBottom) / rBottom);

    /* Return calculated real voltage */
    return realVoltage;
}

void setup(){
  /* Read internal temperature immediately after startup to prevent self heating during operation.*/
  for(byte count = 0; count < TEMPERATURE_REDING_DEBOUNCE; count++){
    dataPacket.internalTempC += temperatureRead(); 
  }
  dataPacket.internalTempC = dataPacket.internalTempC / TEMPERATURE_REDING_DEBOUNCE;

  /*Read and store voltages */
  dataPacket.batteryVoltage = readVoltage(BATTERY_VOLTAGE_SENS_GPIO,  VOLTAGE_DEVIDER_BATTERY_TOP_RESISTOR,   VOLTAGE_DEVIDER_BATTERY_BOTTOM_RESISTOR);
  dataPacket.powerVoltage   = readVoltage(POWER_VOLTAGE_SENS_GPIO,    VOLTAGE_DEVIDER_POWER_TOP_RESISTOR,     VOLTAGE_DEVIDER_POWER_BOTTOM_RESISTOR);


  delay(3000); /* Wait for serial connection from pc to be established. REMOVE FOR PRODUCTION TO SAVE BATTERY */

  /* Init serial port and send measured temperature */
  Serial.begin(115200);
  Serial.printf("Temperature: %.2f °C\n", dataPacket.internalTempC);
  Serial.printf("BatteryV: %.2f v, Power SupplyV %.2f v\n", dataPacket.batteryVoltage, dataPacket.powerVoltage);
  
}

void loop(){
  //nothing to do for now.
}