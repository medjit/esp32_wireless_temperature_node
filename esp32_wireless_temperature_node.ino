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

/* Define sleep times */
#define LOW_BATTERY_THRESHOLD_VOLTAGE     3.3
#define NORMAL_SLEEP_TIME_MINUTES         10
#define LOW_BATTERY_SLEEP_TIME_MINUTES    70

/* includes */
#include <Arduino.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

/* Global Variables */
struct DataPacket_t {
  byte  deviceID;
  unsigned long bootCounter;
  bool isCharging;
  bool lowBatteryFlag;
  float internalTempC;    /* Store measured temperature */
  float batteryVoltage;   /* Store measured battery voltage */
  float powerVoltage;     /* Store measured power supply voltage */
} dataPacket;

// Broadcast MAC address
uint8_t broadcastAddress[] = {
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF
};


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


/**
 * @brief Prints the full contents of the global DataPacket_t struct.
 *
 * This function outputs all stored system information to the Serial monitor,
 * including device identity, boot counter, charging state, battery status,
 * and measured voltages/temperature.
 *
 * It is intended for debugging and runtime monitoring on the ESP32-C6 device.
 *
 * Output format:
 * - Human-readable multi-line diagnostic log
 *
 * @note This function uses Serial.printf(), so Serial must be initialized
 *       before calling this function.
 *
 * @warning Intended for debugging only; not optimized for production logging.
 */
void printDataPacket()
{
    Serial.println("=== Data Packet ===");

    Serial.printf("Device ID: %d\n", dataPacket.deviceID);
    Serial.printf("Boot Counter: %lu\n", dataPacket.bootCounter);

    Serial.printf("Is Charging: %s\n", dataPacket.isCharging ? "true" : "false");
    Serial.printf("Low Battery: %s\n", dataPacket.lowBatteryFlag ? "true" : "false");

    Serial.printf("Internal Temp: %.2f °C\n", dataPacket.internalTempC);
    Serial.printf("Battery Voltage: %.2f V\n", dataPacket.batteryVoltage);
    Serial.printf("Power Voltage: %.2f V\n", dataPacket.powerVoltage);

    Serial.println("===================\n");
}



// ===== Send callback =====
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    Serial.print("Send status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ?
                   "SUCCESS" : "FAIL");
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
  dataPacket.isCharging     = (dataPacket.powerVoltage > (dataPacket.batteryVoltage + 0.1f)); /* set charging status */
  dataPacket.lowBatteryFlag = (dataPacket.batteryVoltage <= LOW_BATTERY_THRESHOLD_VOLTAGE);   /* set lowBattery status */

  delay(1000); /* Wait for serial connection from pc to be established. REMOVE FOR PRODUCTION TO SAVE BATTERY */

  /* Init serial port */
  Serial.begin(115200);


  /* start and config radio */
  WiFi.mode(WIFI_STA);

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
      Serial.println("ESP-NOW init failed");
  }
    
  esp_now_register_send_cb(onDataSent);

  // Add broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);

  peerInfo.channel = 1;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
  }

  /* send dataPacket via esp now */
  esp_err_t result = esp_now_send(
      broadcastAddress,
      (uint8_t *)&dataPacket,
      sizeof(dataPacket)
  );

  if (result == ESP_OK) {
      Serial.println("Packet queued");
  } else {
      Serial.println("Send failed");
  }

  // Give radio time to transmit
  delay(100);

  /* stop radio */
  esp_now_deinit();

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  // Extra power saving
  esp_wifi_stop();

  /*print dataPacket before sleep */
  printDataPacket();

  /* enter in deep sleep if not charging */
  if(false == dataPacket.isCharging){
    unsigned long sleepTime = 0;

    if(false == dataPacket.lowBatteryFlag){
      sleepTime = NORMAL_SLEEP_TIME_MINUTES * 60 * 1000 * 1000;
    }else{
      sleepTime = LOW_BATTERY_SLEEP_TIME_MINUTES * 60 * 1000 * 1000;
    }

    esp_sleep_enable_timer_wakeup(sleepTime);

    Serial.printf("Going to sleep for %d minutes!\n\n", (sleepTime / 60000000));

    delay(30); // let serial finish printing

    // Enter deep sleep (CPU stops here)
    esp_deep_sleep_start();
  }
  
    // if device is charging continue to init wifi, elegant ota and config web page.
}

void loop(){
  //nothing to do for now.
}