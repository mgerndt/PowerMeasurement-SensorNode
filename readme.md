## INA3221 current measurement sensor

This code is mostly copied from [Espressif-components-lib library](https://github.com/UncleRus/esp-idf-lib/tree/master/examples/ina3221). This library contains many handy drivers for different types of sensors. 

In this sample application we use the power measurement sensor INA3221 to measure the current that the board drains from the battery, as well as the actual output voltage of the battery.

#### How to use

![circuiting](./img/circuiting.png)

1. turn around the LilyGo board, at the back of it there is a socket for power supply. Connect a Lipo battery to it with the correct connector.

   <img src="img/image-20210928165107980.png" alt="power socket" style="zoom: 80%;" />

2. Attach `V_in+` and `V_in-` to the immediate output of the battery. Maybe you need some modifications to the battery because the Molex or JST socket is not compatible with 1.25mm standard jumper wire. My battery outputs 3.7V when fully charged so it is not so dangerous. **Be careful when you are using a large battery that outputs higher than 6V**, better turn to official kits when possible. 

   <img src="img/image-20210928165832084.png" alt="image-20210928165832084" style="zoom:50%;" />

3. connect the power lines and I2C lines for INA3221 module and SSD1306 OLED display.

   | Module pin               | ESP32 pin |
   | ------------------------ | --------- |
   | SSD1306 SDA              | GPIO 21   |
   | SSD1306 SCL              | GPIO 22   |
   | INA3221 SDA              | GPIO 25   |
   | INA3221 SCL              | GPIO 27   |
   | SSD1306 VCC, INA3221 VS  | 3V3       |
   | SSD1306 GND, INA3221 GND | GND       |

   The INA3221 may have multiple GND pin, it is sufficient to connect one of them to the ESP32. The INA3221 has two power supply pin, VS (voltage supply) and VPU (voltage pull-up). The VPU is intended to used for I2C line pull up, but it seems just fine to supply power through this pin. 

#### Why use a display?

We use a display module SSD1306 to show the measurements instead of printing them in the console. Because when the USB is connected, the LilyGo battery peripheral will start to charge the battery! And we will see a negative current measurement, which is still a correct measurement though, but not useful for us to optimize the battery usage. 

Anyway we must detach the board from the computer and let it run exclusively on the battery. An OLED display is just one of the solutions. If you don't have a display module, sending the messages through MQTT is also an option.

#### Basic principle of the INA3221 sensor

The INA3221 sensor can only measure voltage. The sensor contains a very tiny (100 mOhm) shunt resistor in each channel. When current flows through the resistor it will cause a slight voltage drop between two sides of the resistor. The sensor then measures the voltage drop and divides the voltage with the predefined resistor value to calculate the current value.

It is defined that `V_in+` is closer to the power supply and `V_in-` is closer to the power consumer.  Beside the calculated current value the sensor also outputs the `V_in+` as the power supply voltage (called bus voltage in the datasheet), assuming `V_in+ == V_in-`.

![image-20210928154305385](img/image-20210928154305385.png)

The shunt voltage measurement precision is up tp 40µV therefore we have a current measurement precision of approximate 0.4mA. The measurement range is -163.84mV to 163.84mV, therefore a current measurement range of -1.6A to 1.6A. 

For more details about sensor principle please see [INA3221 datasheet section 8.3.1](../docs/ina3221.pdf).

#### On-board power regulator

On the LilyGo board there is a low dropout power regulator ME6211, see [LilyGO shematic](../docs/LilyGo_t7_v1.4_schematic.pdf). A fully charged Lipo battery may output 5V but the ESP32 module is powered at 3.3V. The power regulator divides sufficient voltage for the load and dropout the rest voltage which then converts to heat. 

According to the data sheet, the power measurement device should be places as close to the power supply side as possible. If we attached INA3221 to a power line before the power regulator, we should measure a voltage higher than 3.3V on the power supply side.

![image-20210928153033757](img/image-20210928153033757.png)  

#### Alert pins

The INA3221 has 4 alert pins which could be used for alert utilities.

* critical alert (active low): if any single shunt voltage measurement exceeds the predefined threshold
* summation alert (active low): if summation of shunt voltage on multiple chosen channels exceeds a predefined threshold
* warning alert (active low): the actual measurement could be set to be the moving average of several samples. For instance, when the sample number is set to 64, the final output is the average value of 64 individual measurements. The warning alert indicates if the average measurement exceeds a predefined threshold
* power valid (active high): if `V_in+` value of all three channels are higher than a predefined threshold. If not all three channels are used, we must wire the `V_in+` pin of a non-used channel to the `V_in+` pin of other channels.

The four alert pins offers the potential to monitor the power status by interrupt instead of polling, which may be helpful in a power-efficient project. Nevertheless, because the battery voltage usually drains very slowly, it is sufficient to poll the measurement once per hour. So an interrupt routine may not outperform polling the battery status because this issue is trivial.