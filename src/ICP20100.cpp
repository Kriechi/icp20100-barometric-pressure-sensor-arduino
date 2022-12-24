#include "ICP20100.h"

#include <Wire.h>
#include <stdio.h>

#define REG_EMPTY 0x00
#define REG_TRIM1_MSB 0x05
#define REG_TRIM2_LSB 0x06
#define REG_TRIM2_MSB 0x07
#define REG_DEVICE_ID 0x0C
#define REG_IO_DRIVE_STRENGTH 0x03
#define REG_OTP_CONFIG1 0xAC
#define REG_OTP_MR_LSB 0xAD
#define REG_OTP_MR_MSB 0xAE
#define REG_OTP_MRA_LSB 0xAF
#define REG_OTP_MRA_MSB 0xB0
#define REG_OTP_MRB_LSB 0xB1
#define REG_OTP_MRB_MSB 0xB2
#define REG_OTP_ADDRESS 0xB5
#define REG_OTP_COMMAND 0xB6
#define REG_OTP_RDATA 0xB8
#define REG_OTP_STATUS 0xB9
#define REG_OTP_DBG2 0xBC
#define REG_OTP_STATUS2 0xBF
#define REG_MASTER_LOCK 0xBE
#define REG_MODE_SELECT 0xC0
#define REG_INTERRUPT_STATUS 0xC1
#define REG_INTERRUPT_MASK 0xC2
#define REG_FIFO_CONFIG 0xC3
#define REG_FIFO_FILL 0xC4
#define REG_SPI_MODE 0xC5
#define REG_PRESS_ABS_LSB 0xC7
#define REG_PRESS_ABS_MSB 0xC8
#define REG_PRESS_DELTA_LSB 0xC9
#define REG_PRESS_DELTA_MSB 0xCA
#define REG_DEVICE_STATUS 0xCD
#define REG_I3C_INFO 0xCE
#define REG_VERSION 0xD3
#define REG_FIFO_BASE 0xFA

ICP20100::ICP20100(void)
{
    // Constructor
}

bool ICP20100::begin(uint8_t addr)
{
    address = addr;

    writeRegister(0xEE, 0xFF); // dummy write to init the I2C interface of the ASIC

    uint8_t device_id = readRegister(REG_DEVICE_ID);
    if (device_id != 0x63)
    {
        return false;
    }

    uint8_t version = readRegister(REG_VERSION);
    if (version == 0xB2)
    {
        // version B, no further initialization is required
    }
    else if (version == 0x00)
    {
        if (!initVersionA())
        {
            return false;
        }
    }
    else
    {
        // invalid version
        return false;
    }

    setMeasurementConfiguration(Mode3);
    setForcedMeasurementTrigger(Standby);
    setMeasurementMode(Continuous);
    setPowerMode(Active);

    waitForFIRFilterWarmUp();
    return true;
}

bool ICP20100::initVersionA()
{
    uint8_t b = readRegister(REG_OTP_STATUS2);
    if (b & 0x01)
    {
        // Didn’t go through power cycle after previous boot up sequence.
        // No further initialization is required.
        return true;
    }

    writeRegister(REG_MODE_SELECT, 0x04); // ignore DEVICE_STATUS and do not use setPowerMode function
    delay(4);                             // milliseconds

    writeRegister(REG_MASTER_LOCK, 0x1f);

    b = readRegister(REG_OTP_CONFIG1) | 0x03;
    writeRegister(REG_OTP_CONFIG1, b);
    delayMicroseconds(10);

    b = readRegister(REG_OTP_DBG2);
    writeRegister(REG_OTP_DBG2, b | 0x80);
    delayMicroseconds(10);
    writeRegister(REG_OTP_DBG2, b & (~0x80));
    delayMicroseconds(10);

    writeRegister(REG_OTP_MRA_LSB, 0x04);
    writeRegister(REG_OTP_MRA_MSB, 0x04);
    writeRegister(REG_OTP_MRB_LSB, 0x21);
    writeRegister(REG_OTP_MRB_MSB, 0x20);
    writeRegister(REG_OTP_MR_LSB, 0x10);
    writeRegister(REG_OTP_MR_MSB, 0x80);

    writeRegister(REG_OTP_ADDRESS, 0xF8);
    writeRegister(REG_OTP_COMMAND, 0x10);
    do
    {
        delay(1); // milliseconds
    } while (readRegister(REG_OTP_STATUS) & 0x01);
    uint8_t offset = readRegister(REG_OTP_RDATA);

    writeRegister(REG_OTP_ADDRESS, 0xF9);
    writeRegister(REG_OTP_COMMAND, 0x10);
    do
    {
        delay(1); // milliseconds
    } while (readRegister(REG_OTP_STATUS) & 0x01);
    uint8_t gain = readRegister(REG_OTP_RDATA);

    writeRegister(REG_OTP_ADDRESS, 0xFA);
    writeRegister(REG_OTP_COMMAND, 0x10);
    do
    {
        delay(1); // milliseconds
    } while (readRegister(REG_OTP_STATUS) & 0x01);
    uint8_t hfosc = readRegister(REG_OTP_RDATA);

    b = readRegister(REG_OTP_CONFIG1);
    b &= (~0x01);
    b &= (~0x02);
    writeRegister(REG_OTP_CONFIG1, b);
    delayMicroseconds(10);

    writeRegister(REG_TRIM1_MSB, offset & 0x3F);

    b = readRegister(REG_TRIM2_MSB);
    b = (b & (~0x70)) | ((gain & 0x07) << 4);
    writeRegister(REG_TRIM2_MSB, b);

    writeRegister(REG_TRIM2_LSB, hfosc & 0x7F);

    writeRegister(REG_MASTER_LOCK, 0x00);

    writeRegister(REG_MODE_SELECT, 0x00);
    delay(5);

    b = readRegister(REG_OTP_STATUS2);
    writeRegister(REG_OTP_STATUS2, b | 0x01);
    delay(5);

    return true;
}

bool ICP20100::read(uint8_t *count, float *pressure_values, float *temperature_values)
{
    if (!getFIFOlevel(count))
    {
        *count = 0;
        return false;
    }

    if (fir_warm_up)
    {
        if (*count >= 14)
        {
            flushFIFO();
            fir_warm_up = false;
        }
        *count = 0;
        return false;
    }

    if (*count > 0)
    {
        uint8_t data[96] = {0}; // maximum size for a full FIFO
        uint8_t position = 0;
        if (!readRegister(REG_FIFO_BASE, *count * 2 * 3, data)) // read pressure + temperature with 3 bytes each
        {
            return false;
        }
        for (uint8_t i = 0; i < *count; i++)
        {
            // 20-bit pressure value
            int32_t rawPressure = (int32_t)(((data[position + 2] & 0x0F) << 16) | ((data[position + 1]) << 8) | ((data[position] & 0x0F)));
            if (rawPressure & 0x080000)
            {
                rawPressure |= 0xFFF00000;
            }
            pressure_values[i] = ((float)rawPressure) / (1 << 17) * 40 + 70;
            position += 3;

            // 20-bit temperature value
            int32_t rawTemperature = (int32_t)(((data[position + 2] & 0x0F) << 16) | ((data[position + 1]) << 8) | ((data[position] & 0x0F)));
            if (rawTemperature & 0x080000)
            {
                rawTemperature |= 0xFFF00000;
            }
            temperature_values[i] = ((float)rawTemperature) / (1 << 18) * 65 + 25;
            position += 3;
        }
    }
    return true;
}

uint8_t ICP20100::readRegister(uint8_t reg)
{
    uint8_t value;
    if (readRegister(reg, 1, &value))
    {
        return value;
    }
    return -1;
}

bool ICP20100::readRegister(uint8_t reg, uint8_t length, uint8_t *buffer)
{
    Wire.beginTransmission(address);
    if (Wire.write(reg) != 1)
    {
        return false;
    }
    if (Wire.endTransmission(false) != 0) // ICP-20100 requires NO stop bit for read access!
    {
        return false;
    }

    if (Wire.requestFrom(address, length) != length)
    {
        return false;
    }
    for (uint8_t i = 0; i < length; i++)
    {
        buffer[i] = Wire.read();
    }

    if (reg != REG_EMPTY)
    {
        readRegister(REG_EMPTY);
    }

    return true;
}

bool ICP20100::writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    readRegister(REG_EMPTY);
    return true;
}

bool ICP20100::flushFIFO()
{
    uint8_t v;
    if (!readRegister(REG_FIFO_FILL, 1, &v))
    {
        return false;
    }

    v |= 0x80;

    if (!writeRegister(REG_FIFO_FILL, v))
    {
        return false;
    }

    return true;
}

bool ICP20100::checkFIFOempty(bool *empty)
{
    uint8_t v;
    if (!readRegister(REG_FIFO_FILL, 1, &v))
    {
        return false;
    }
    *empty = v & 0x40;
    return true;
}

bool ICP20100::checkFIFOfull(bool *full)
{
    uint8_t v;
    if (!readRegister(REG_FIFO_FILL, 1, &v))
    {
        return false;
    }
    *full = v & 0x20;
    return true;
}

bool ICP20100::getFIFOlevel(uint8_t *level)
{
    uint8_t v;
    if (!readRegister(REG_FIFO_FILL, 1, &v))
    {
        return false;
    }
    *level = v & 0x1F;
    if (*level > 16)
    {
        return false;
    }

    return true;
}

bool ICP20100::setMeasurementConfiguration(OperationMode mode)
{
    if (mode == Mode1 || mode == Mode2 || mode == Mode3)
    {
        // FIR filter needs to settle before measurement data should be used
        fir_warm_up = true;
    }

    uint8_t v = readRegister(REG_MODE_SELECT);
    v = (v & ~(0x7 << 5)) | ((mode & 0x7) << 5);
    return setMode(v);
}

bool ICP20100::setForcedMeasurementTrigger(ForcedMeasurementTrigger mode)
{
    uint8_t v = readRegister(REG_MODE_SELECT);
    v = (v & ~(1 << 4)) | ((mode & 0x01) << 4);
    return setMode(v);
}

bool ICP20100::setMeasurementMode(MeasurementMode mode)
{
    uint8_t v = readRegister(REG_MODE_SELECT);
    v = (v & ~(1 << 3)) | ((mode & 0x01) << 3);
    return setMode(v);
}

bool ICP20100::setPowerMode(PowerMode mode)
{
    uint8_t v = readRegister(REG_MODE_SELECT);
    v = (v & ~(1 << 2)) | ((mode & 0x01) << 2);
    return setMode(v);
}

bool ICP20100::setMode(uint8_t mode)
{
    uint8_t mode_sync_status = 0;
    do
    {
        uint8_t mode_sync_status = readRegister(REG_DEVICE_STATUS);
        if (mode_sync_status & 0x01)
        {
            break;
        }
        delay(1);
    } while (true);

    return writeRegister(REG_MODE_SELECT, mode);
}

bool ICP20100::waitForFIRFilterWarmUp()
{
    uint8_t count;
    float pressure_values[16];
    float temperature_values[16];
    while (!fir_warm_up)
    {
        if (!read(&count, pressure_values, temperature_values))
        {
            return false;
        }
        delay(500);
    }
    return true;
}
