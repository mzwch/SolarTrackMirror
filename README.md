
🌞 Smart Solar Mirror Tracking System
📖 Project Description
This project is a solar reflection system built using Raspberry Pi Zero, ESP32, and stepper motors, designed to redirect sunlight to a specific area inside my house. The motivation comes from my home’s architecture: a 4-floor building with a vertical light well (atrium) running from the ground floor up to the rooftop. Due to the building’s height, natural light rarely reaches the ground floor, except during a short window around noon.

To solve this, I created a system that tracks the sun from sunrise to sunset and continuously reflects sunlight through a series of mirrors to the base of the light well, ensuring the ground floor is always illuminated during daylight hours. Precision is crucial, as even a 1° angular deviation results in more than 2 meters of displacement on the ground.

✨ Features
Aluminum Mirror Platform: A custom-built support platform made from machined aluminum holds the primary mirror. The platform is mounted on a dual-axis gimbal driven by stepper motors.
Stepper Motor Control: Each stepper motor is connected to a 50:1 harmonic reducer, achieving a total resolution of 100,000 pulses per 360°. The direction and pulse signals are used to precisely control motor rotation.
ESP32 Motor Driver: The ESP32 drives both horizontal and vertical motion axes using four GPIOs (2 for pulse, 2 for direction), providing low-latency response to external commands.
Serial Communication: ESP32 communicates with Raspberry Pi Zero via UART. It receives sun position angles and rotates the mirror accordingly. ESP32 also supports Bluetooth, allowing control through a custom mobile app.
High-Precision Solar Calculation: The Raspberry Pi Zero runs a Python script that uses NASA’s DE442 planetary ephemeris data for high-accuracy solar position calculation. It computes real-time azimuth and elevation of the sun relative to a fixed target point on the ground. The result is converted into motor angles and sent over serial to the ESP32.
Web UI Control Interface: A lightweight web server on the Raspberry Pi Zero allows users to monitor and adjust tracking in real-time from any browser.
Calibration & Compensation: To fine-tune precision, the system includes time-based correction coefficients in the web interface to account for minor reflection misalignments observed in practice.
🔧 Hardware Requirements
2 × NEMA 17 stepper motors (42mm)
2 × stepper motor drivers
1 × ESP32 development board
1 × Raspberry Pi Zero W
1 × harmonic gear reducer (50:1)
1 × 420mm-diameter mirror
Aluminum plates for the platform housing
🚧 Project Status & Challenges
Initially, I used the SolarCalculator library in Arduino on the ESP32, but its accuracy was insufficient for this high-precision reflection setup. With a required reflection precision of less than 1°, even small errors resulted in the sunlight missing the target by meters.

Switching to Python on Raspberry Pi Zero allowed me to use the DE442 ephemeris for astronomical calculations, significantly improving positional accuracy. The Pi calculates solar azimuth and altitude using Earth-centered coordinates and sends mirror alignment commands to the ESP32 via UART.

Despite the high theoretical precision, I still noticed slight misalignments. To address this, I added per-time-slot calibration factors to the control web page, which lets me fine-tune reflection angles based on real-world sunlight paths.

However, I remain puzzled by the residual inaccuracies and would deeply appreciate any expert feedback or optimization advice from the community. Thank you!

*项目介绍
    这是一个使用树莓派Zero+ESP32+步进电机控制太阳光反射镜的项目，项目原因是我家里是一个4层楼的那建筑，在房子的中心有一个直接贯穿到楼顶的天井，因为房子比较高，所以在一楼的光线比较暗，而一天之中，只有正中午的时候才会有阳光照射到天井下面的地面上，所以我需要一个设备，在太阳从地平线升起来的时候将太阳光通过反射镜把阳光反射到天井地面下，而且保持一天之中，太阳光始终都是反射到这个地面的，这样我的房子的一楼就光线充足了，所以我想到了使用ESP32+步进电机来控制镜子的角度，但要非常高精度的控制，所以就需要更加精准的天文计算方法了，所以我找到树莓派的Zero板，在这个板子上可以运行Python库，为了方便控制和查看控制结果，我在Zero上设置了WEB的网页服务，这样，通过进入这个控制网页就可以进行控制了。
*功能简介
    1、镜子平台为铝制板，板的力支持点由步进电机输出控制，反射的镜子安装在这个平台上。
    2、步进电机由专用配套的驱动器输入，步进电机输出是接在一个谐波减速器上的，这个减速器转换比例是50比1的，只需要告诉这个控制电机的方向和输出的脉冲数是什么就行，驱动机和步进电机结合起来的步进量是10万个脉冲使电机转动360度。
    3、ESP32为一种开发板，在ESP32有两根线分别控制支持镜子水平转动和仰角转动的两个步进电机的输入脉冲数，另外两根线控制方向是否正反转。
    4、ESP32的串口与树莓派Zero的串口连接，当接收到Zero发过来的电机控制信号时，就让电机运转指定的角度，另外ESP32有蓝牙功能，通过手机APP可以在其界面上控制这个反射镜电机。
    5、在ESP32的串口上接的是树莓派Zero主板，在这个主板上使用Python程序进行天文算法计算的太阳位置计算，这个天文计算是先通过使用的星历表文件路径(DE442是NASA的高精度行星/月球历表)计算所在地文的太阳方位和角度，然后再通过空间向量的计算得出电机上的镜子与目标位置的中转反射镜反射角度和方位，然后通过串口让镜子转动到对应的位置
*硬件需求（ESP32、步进电机、驱动板等）
    42步进电机2个，步进电机驱动器2个，ESP32开发板1块、一些铝切割好的板子组合成平台箱子和平台，1块直径为420毫米的镜子、1个树莓派ZeroW的开发板。
项目完成情况和问题
    通过实际安装使用后，原来想使用ESP32程序自己的ArduinoIDE的库文件的SolarCalculator库来计算天文的，但这样计算出来的太阳位置精度偏差比较大，因为我是需要在4楼之上将太阳光反射到1楼的地面的，所以稍微角度有偏差都会导致光线没有正确的反射，通过试验，当角度偏离正确的位置1度时，在地面偏离的距离超过了2米，这样将会导致光线没有正确反射在地面上的，所以，我开始使用树莓派的ZeroW来进行天文计算，因为在ZeroW中可以运行Python程序，还有就是存储空间够大足够放下NASA的高精度行星/月球历表文件de442.bsp，这个文件是星历表，然后通过Python计算出参照星体的相对位置，在这个项目中，我设置了本地坐标是地球，参照星体是太阳，根据计算得出的方位和角度就应该比较准确了，我想这个是计算太阳方位角度的方法里，是最准确的了，但是即使这样，计算出来的位置通过控制电机去使镜子反射太阳光时，还是会出现偏差的问题，于是我在ZeroW提供的WEB界面上设置有每个时间段的修正系数，如此来让镜子根据实际反射效果来修正。
    但是，我还是很困惑，为什么如果精度高的计算，而得到的控制结果还是有偏差，这里还请各位前辈指教，谢谢