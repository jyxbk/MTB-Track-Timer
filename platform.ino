[platformio]
src_dir = .

[env:Node1_Start]
platform      = espressif32
board         = heltec_wifi_lora_32_V3
framework     = arduino
monitor_speed = 115200

lib_deps =
    jgromes/RadioLib @ ^6.6.0
    olikraus/U8g2    @ ^2.35.19
