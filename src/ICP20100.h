#pragma once

#include "Arduino.h"

#define ICP20100_DEFAULT_I2C_ADDR 0x63 // or 0x64

class ICP20100
{
public:
    enum OperationMode : uint8_t
    {
        Mode0 = 0x00, // Bandwidth: 6.25 Hz, ODR: 25 Hz, Pressure noise: 0.5 Pa RMS, Current Consumption: 211 µA
        Mode1 = 0x01, // Bandwidth: 30 Hz, ODR: 120 Hz, Pressure noise: 1 Pa RMS, Current Consumption: 222 µA
        Mode2 = 0x02, // Bandwidth: 10 Hz, ODR: 40 Hz, Pressure noise: 2.5 Pa RMS, Current Consumption: 49 µA
        Mode3 = 0x03, // Bandwidth: 0.5 Hz, ODR: 2 Hz, Pressure noise: 0.5 Pa RMS, Current Consumption: 23 µA
        Mode4 = 0x04, // Bandwidth: 12.5 Hz, ODR: 25 Hz, Pressure noise: 0.3 Pa RMS, Current Consumption: 250 µA
    };

    enum ForcedMeasurementTrigger : uint8_t
    {
        Standby = 0x0, // Stay in Standby mode
        Trigger = 0x1, // Trigger for forced measurement (only supported for Mode4)
    };

    enum MeasurementMode : uint8_t
    {
        StandbyTrigger = 0x0, // Standby or trigger forced measurement based on the field FORCED_MEAS_TRIGGER
        Continuous = 0x1,     // Continuous Measurements (duty cycled): Measurements are started based on the selected mode ODR_REG
    };

    enum PowerMode : uint8_t
    {
        Normal = 0x0, // Normal Mode: Device is in standby and goes to active mode during the execution of a measurement
        Active = 0x1, // Active Mode: Power on DVDD and enable the high frequency clock
    };

    ICP20100(void);
    bool begin(uint8_t addr = ICP20100_DEFAULT_I2C_ADDR);
    bool read(uint8_t *count, float *pressure_values, float *temperature_values);

    bool flushFIFO();
    bool checkFIFOempty(bool *empty);
    bool checkFIFOfull(bool *full);
    bool getFIFOlevel(uint8_t *level);

    bool setMeasurementConfiguration(OperationMode mode);
    bool setForcedMeasurementTrigger(ForcedMeasurementTrigger mode);
    bool setMeasurementMode(MeasurementMode mode);
    bool setPowerMode(PowerMode mode);

    bool waitForFIRFilterWarmUp();

    uint8_t readRegister(uint8_t reg);
    bool readRegister(uint8_t reg, uint8_t length, uint8_t *buffer);
    bool writeRegister(uint8_t reg, uint8_t value);

private:
    uint8_t address;

    bool fir_warm_up = false;

    bool initVersionA();
    bool setMode(uint8_t mode);
};
