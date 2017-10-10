# ESP8266-HTTP-POST
The ESP8266 interfaced with DHT22 and LDR sends data over http to the local server as a json string.
UDP is used here to get the epoch time through a local server pool, this epoch is sent along with the data.
Battery information is also obtained by reading internal the voltage of the ESP and is sent along with sensor data for battery monitoring.
