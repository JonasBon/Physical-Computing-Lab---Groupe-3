# Physical Computing Lab - Groupe 3

A microelectronics project with an ESP32 about a refrigerator sensor that can detect if the refrigerator has been left open for too long and critically assesses consumer behavior with AI.

### Wiki Link: https://wiki.hci.uni-hannover.de/doku.php?id=teaching:s25:pcl:g3


### Einrichtung:
1. Host your own Text-to-Speech (TTS) server. You can use the provided stuff in the 'tts-server' folder, which sends and api call to the tts api from chatgpt and streams the received audio back to the ESP32.
You need to copy the '.env.template' file to '.env' and fill in your API Key.
2. Copy 'secrets.h.template' to 'secrets.h' and fill in your credentials and stuff.