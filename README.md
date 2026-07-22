
How it works?

The ESP8266 runs a web server. The microcontroller controls LED lighting and an electric valve.

The microcontroller handles the following tasks:
  The user can adjust the LED lighting using either the slider or the potentiometer.
  The valve was controlled based on the temperature measured on the radiator

You can access the web server by entering the IP address into your browser: http://192.168.1.230/ .
Additionally, you need to configure the WiFi SSID and password.

You can see the circuit schematic below:

<br>
<img width="550" height="343" alt="Schematic" src="https://github.com/user-attachments/assets/c0ea4d4c-58a2-4208-bb16-3639bc11951e" />
<br>
Here is the PCB layout:
<br>
<img width="300" height="300" alt="pcb" src="https://github.com/user-attachments/assets/c544fca6-bbae-450b-be01-5200c2a46a88" />
<br>
You can find the AutoCAD Eagle design files as LightController_PCB.

If the web server is live, you should see the following view. The most important elements are the slider, the temperature value:
<br>
<img width="286" height="186" alt="webserver" src="https://github.com/user-attachments/assets/0dd325b7-429a-40e9-88e8-20e3ba14d823" />
