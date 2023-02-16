#include <Arduino.h>
#include <ICP20100.h>

ICP20100 icp20100 = ICP20100();

void setup()
{
  delay(2000); // let everything fully boot and settle down
  Serial.begin(115200);
  Wire.begin();

  if (!icp20100.begin())
  {
    Serial.println("ICP20100 not detected or failed to initialize!");
    while (1) { ;; }
  }

  // optional: configure non-default I2C address:
  //  icp20100.begin(0x64)
  // optional: set measurement configuration / output data rate of MODE1, 2, 3 (default), or 4:
  //  icp20100.setMeasurementConfiguration(icp20100.Mode3);
  // optional: set forced measurements to standby (default) or trigger (only in MODE4):
  //  icp20100.setForcedMeasurementTrigger(icp20100.Standby);
  // optional: set standby/trigger or continuous (default) measurement mode
  //  icp20100.setMeasurementMode(icp20100.Continuous);
  // optional: set power mode normal or active (default)
  //  icp20100.setPowerMode(icp20100.Active);

  // when changing to MODE1, 2, or 3, wait for the FIR filter to produce valid measurement values
  //   icp20100.waitForFIRFilterWarmUp()
}

void loop()
{
  delay(1000); // let the measurement samples accumulate in the sensor

  uint8_t count;
  float pressure_values[16];    // there are up to 16 samples to be read
  float temperature_values[16]; // there are up to 16 samples to be read
  if (!icp20100.read(&count, pressure_values, temperature_values))
  {
    Serial.println("ICP20100: no samples available or read failure!");
    delay(1000);
    return;
  }
  if (count > 0)
  {
    Serial.printf("Received %d samples - most recent Pressure: %0.2f kPa, Temperature: %0.2f Celsius\n", count, pressure_values[count-1], temperature_values[count-1]);
  }
}
