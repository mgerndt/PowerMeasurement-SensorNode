#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ina3221.h>
#include "ssd1306.h"
#include <string.h>

#define WARNING_CHANNEL 1 
#define WARNING_CURRENT (40.0)
#define I2C_PORT 1
//#define I2C_SDA_PIN 25
//#define I2C_SCL_PIN 27
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define CONTINUOUS_MODE false // true : continuous  measurements // false : trigger measurements
static ina3221_t dev = {
    .shunt = {100, 100, 50}, // shunt values are 100 mOhm for each channel
    .config.config_register = INA3221_DEFAULT_CONFIG,
    .mask.mask_register = INA3221_DEFAULT_MASK};

void Ina3221Init(void)
{
    memset(&dev.i2c_dev, 0, sizeof(i2c_dev_t));

    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(ina3221_init_desc(&dev, INA3221_I2C_ADDR_VS, I2C_PORT, I2C_SDA_PIN, I2C_SCL_PIN));

    ESP_ERROR_CHECK(ina3221_set_options(&dev, CONTINUOUS_MODE, true, true));   // Mode selection, bus and shunt activated
    ESP_ERROR_CHECK(ina3221_enable_channel(&dev, true, false, false));         // Enable all channels
    ESP_ERROR_CHECK(ina3221_set_average(&dev, INA3221_AVG_64));                // 64 samples average
    ESP_ERROR_CHECK(ina3221_set_bus_conversion_time(&dev, INA3221_CT_2116));   // 2ms by channel
    ESP_ERROR_CHECK(ina3221_set_shunt_conversion_time(&dev, INA3221_CT_2116)); // 2ms by channel

    ESP_ERROR_CHECK(ina3221_set_warning_alert(&dev, WARNING_CHANNEL - 1, WARNING_CURRENT)); // Set overcurrent security flag
}

void Ina3221Measurement(void)
{
    static bool warning = false;
    static float bus_voltage;
    static float shunt_voltage;
    static float shunt_current;

#if !CONTINUOUS_MODE
    ESP_ERROR_CHECK(ina3221_trigger(&dev)); // Start a measure
    printf("trig done, wait: ");
    do
    {
        printf("X");

        ESP_ERROR_CHECK(ina3221_get_status(&dev)); // get mask

        if (dev.mask.wf & (1 << (3 - WARNING_CHANNEL)))
            warning = true;

        vTaskDelay(pdMS_TO_TICKS(20));

    } while (!(dev.mask.cvrf)); // check if measure done
#else
    ESP_ERROR_CHECK(ina3221_get_status(&dev)); // get mask

    if (dev.mask.wf & (1 << (3 - WARNING_CHANNEL)))
        warning = true;
#endif
    for (uint8_t i = 0; i < 1; i++)
    {
        // Get voltage in volts
        ESP_ERROR_CHECK(ina3221_get_bus_voltage(&dev, i, &bus_voltage));
        // Get voltage in millivolts and current in milliamperes
        ESP_ERROR_CHECK(ina3221_get_shunt_value(&dev, i, &shunt_voltage, &shunt_current));


        if (warning && (i + 1) == WARNING_CHANNEL)
           printf("C%u:Warning Current > %.2f mA !!\n", i + 1, WARNING_CURRENT);
        printf("C%u:Bus voltage: %.02f V\n", i + 1, bus_voltage);
        printf("C%u:Shunt voltage: %.02f mV\n", i + 1, shunt_voltage);
        printf("C%u:Shunt current: %.02f mA\n\n", i + 1, shunt_current);
        ssd1306_clearScreen();
         char buf[200];
        sprintf(buf, "%.02f V", bus_voltage);
        ssd1306_printFixedN(0, 0, buf, STYLE_NORMAL, 1);
        sprintf(buf, "%.02f mV", shunt_voltage);
        ssd1306_printFixedN(0, 20, buf, STYLE_NORMAL, 1);
        sprintf(buf, "%.02f mA", shunt_current);
        ssd1306_printFixedN(0, 40, buf, STYLE_NORMAL, 1);

    }
    warning = false;
}

void measurePowerTask(void *pvParameters)
{

    while (1)
    {
        Ina3221Measurement();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}