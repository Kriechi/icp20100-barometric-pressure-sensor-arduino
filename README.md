# ICP-20100: Arduino library for barometric pressure sensor over I2C

Device: [ICP-20100 by TDK-InvenSense](https://invensense.tdk.com/download-pdf/icp-20100-datasheet/)

Interface: I2C

Library: C++ for Arduino

## Usage

```c++
#include <Arduino.h>
#include <ICP20100.h>

ICP20100 icp20100 = ICP20100();

void setup()
{
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
  uint8_t count;
  float pressure_values[16];    // there are up to 16 samples to be read
  float temperature_values[16]; // there are up to 16 samples to be read
  if (!icp20100.read(&count, pressure_values, temperature_values))
  {
    Serial.println("ICP20100: failed to read measurement values!");
    delay(500);
    return;
  }
  if (count > 0)
  {
    Serial.printf("Most recent Pressure: %d, Temperature: %0.2f\n", pressure_values[count], temperature_values[count]);
  }

  delay(500);
}
```

## License

This project is made available under the MIT License. For more details, see the LICENSE file in the repository.

## Author

This project was created by Thomas Kriechbaumer.
