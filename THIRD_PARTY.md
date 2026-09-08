# Dependências

O código novo deste repositório usa MIT. As dependências não são relicenciadas.

- SinricPro 3.3.1: `library.json` upstream declara CC-BY-SA-4.0; https://github.com/sinricpro/esp8266-esp32-sdk/tree/3.3.1 . Bibliotecas e exemplos upstream mantêm seus avisos. A redistribuição do firmware combinado exige revisão das condições aplicáveis; MIT aqui não substitui essas condições.
- ArduinoJson 7.3.1: https://github.com/bblanchon/ArduinoJson/tree/v7.3.1 .
- WebSockets 2.6.1: commit `7a4c416082b80dfc2c0da10731effc8d3a69abc6`, https://github.com/Links2004/arduinoWebSockets .
- Arduino-ESP32 / ESP-IDF e toolchain: versões resolvidas por espressif32 6.10.0, avisos próprios nos pacotes PlatformIO.
- GNU-EFI: https://github.com/ncroxon/gnu-efi . Bibliotecas de suporte mantêm as licenças upstream.
- iPXE: https://github.com/ipxe/ipxe/tree/7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1 . O builder usa o commit de referência; iPXE tem licenças upstream próprias, incluindo GPL. Não há binário iPXE universal nesta entrega.
- Sintaxe iPXE consultada no projeto iPXE: https://ipxe.org/cmd/ifconf e https://ipxe.org/cmd/imgfetch .

O ZIP de código não incorpora o snapshot privado nem fontes vendorizadas dessas dependências.
