#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ina3221.h>
#include <ssd1306.h>
#include <string.h>
#include "esp_timer.h"

#define WARNING_CHANNEL 1 
#define WARNING_CURRENT (200.0)
#define I2C_PORT 1
//#define I2C_SDA_PIN 25
//#define I2C_SCL_PIN 27
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define toggleEnergyInterrupt 18
#define resetEnergyInterrupt 19
#define CONTINUOUS_MODE true // true : continuous  measurements // false : trigger measurements
static ina3221_t dev = {
    .shunt = {100, 100, 50}, // shunt values are 100 mOhm for each channel
    .config.config_register = INA3221_DEFAULT_CONFIG,
    .mask.mask_register = INA3221_DEFAULT_MASK};


float bus_voltage;
float shunt_voltage;
float shunt_current2;
float shunt_current3;
bool measureEnergy=false;
uint64_t energy=0,lastEnergy=0;
uint64_t lastMeasurement=0;


void IRAM_ATTR toggleEnergy(void* arg){
    if (measureEnergy){
        ets_printf("stop energy measurement\n");
        measureEnergy=false;
        lastEnergy=energy;
    } else {
        ets_printf("start energy measurement\n");
        measureEnergy=true;
        //energy=0;
    }
}

void IRAM_ATTR resetEnergy(void* arg){
    ets_printf("reset energy measurement\n");
    measureEnergy=false;
    energy=0;
    //lastEnergy=0;
    lastMeasurement=esp_timer_get_time();
 }


void Ina3221Init(void)
{
    memset(&dev.i2c_dev, 0, sizeof(i2c_dev_t));

    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(ina3221_init_desc(&dev, INA3221_I2C_ADDR_VS, I2C_PORT, I2C_SDA_PIN, I2C_SCL_PIN));

    ina3221_set_options(&dev, CONTINUOUS_MODE, true, true);   // Mode selection, bus and shunt activated
    ESP_ERROR_CHECK(ina3221_set_options(&dev, CONTINUOUS_MODE, true, true));   // Mode selection, bus and shunt activated
    ESP_ERROR_CHECK(ina3221_enable_channel(&dev, false, true, true));         // Enable all channels
    ESP_ERROR_CHECK(ina3221_set_average(&dev, INA3221_AVG_1));                // 64 samples average
    ESP_ERROR_CHECK(ina3221_set_bus_conversion_time(&dev, INA3221_CT_2116));   // 2ms by channel
    ESP_ERROR_CHECK(ina3221_set_shunt_conversion_time(&dev, INA3221_CT_2116)); // 2ms by channel

    ESP_ERROR_CHECK(ina3221_set_warning_alert(&dev, WARNING_CHANNEL - 1, WARNING_CURRENT)); // Set overcurrent security flag
}

void Ina3221Measurement(void){

#if !CONTINUOUS_MODE
    ESP_ERROR_CHECK(ina3221_trigger(&dev)); // Start a measure
    printf("trig done, wait: ");
    do
    {
        printf("X");

        ESP_ERROR_CHECK(ina3221_get_status(&dev)); // get mask


        vTaskDelay(pdMS_TO_TICKS(20));

    } while (!(dev.mask.cvrf)); // check if measure done
#else
    ESP_ERROR_CHECK(ina3221_get_status(&dev)); // get mask

#endif
    ESP_ERROR_CHECK(ina3221_get_shunt_value(&dev, 1, &shunt_voltage, &shunt_current2));
    ESP_ERROR_CHECK(ina3221_get_shunt_value(&dev, 2, &shunt_voltage, &shunt_current3));
}

void showMeasurements(){
        char buf[200];

        ssd1306_clearScreen();
           
        sprintf(buf, "%lld mAs", energy/1000000);
        ssd1306_printFixedN(0, 0, buf, STYLE_NORMAL, 1);
        // sprintf(buf, "%lld mAs", lastEnergy/1000000);
        // ssd1306_printFixedN(0, 20, buf, STYLE_NORMAL, 1);

        sprintf(buf, "%.01f mA", shunt_current2);
        ssd1306_printFixedN(0, 20, buf, STYLE_NORMAL, 1);
         sprintf(buf, "%.01f mA", shunt_current3);
        ssd1306_printFixedN(0, 40, buf, STYLE_NORMAL, 1);
   
}

void measureTask(void *pvParameters){
    lastMeasurement=esp_timer_get_time();
    while (1){
        uint64_t diffMicroSeconds;
        uint64_t currentTimestamp;

        Ina3221Measurement();
        currentTimestamp=esp_timer_get_time();

        if (measureEnergy){
            diffMicroSeconds=(currentTimestamp-lastMeasurement);
            energy=energy+shunt_current3*diffMicroSeconds;
        }
        lastMeasurement=currentTimestamp;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void showTask(void *pvParameters){

    while (1){
        showMeasurements();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main()
{
    Ina3221Init();
    ssd1306_128x64_i2c_init();
    ssd1306_128x64_init();
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    ssd1306_clearScreen();
    char buf[200];
    sprintf(buf, "Hallo");
    ssd1306_printFixedN(0, 0, buf, STYLE_NORMAL, 2);
    vTaskDelay(pdMS_TO_TICKS(1000));  
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_pulldown_en (toggleEnergyInterrupt));
    ESP_ERROR_CHECK(gpio_set_intr_type(toggleEnergyInterrupt, GPIO_INTR_POSEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(toggleEnergyInterrupt, toggleEnergy, NULL));
    ESP_ERROR_CHECK(gpio_pulldown_en (resetEnergyInterrupt));
    ESP_ERROR_CHECK(gpio_set_intr_type(resetEnergyInterrupt, GPIO_INTR_POSEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(resetEnergyInterrupt, resetEnergy, NULL));
  
    //while(true){}

    xTaskCreate(measureTask, "ina3221_test", configMINIMAL_STACK_SIZE * 8, NULL, 2, NULL);
    xTaskCreate(showTask, "ina3221_test", configMINIMAL_STACK_SIZE * 8, NULL, 1, NULL);
}
