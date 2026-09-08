# Sinric Pro opcional

Crie dispositivos Switch no portal Sinric Pro, copie App Key/App Secret e Device IDs para a dashboard e mapeie até oito slots a entradas Boot####. O slot `default` usa pending válido ou sistema padrão.

A implementação usa `SinricProSwitch`, `onPowerState`, `sendPowerStateEvent` e `SinricPro.begin` do SDK 3.3.1. Não cria dispositivos no portal nem inventa enumeração de modes. O limite local de oito slots não promete oito dispositivos gratuitos: plano/limites da conta são separados.

ON enfileira boot; OFF não desliga o PC. Após o evento aceito, o Switch é devolvido para OFF. Pedido com PC online é rejeitado. Após edição de credenciais/slots, salvar reinicia o ESP32 para registrar os callbacks atualizados.

Fonte: [SDK SinricPro 3.3.1](https://github.com/sinricpro/esp8266-esp32-sdk/tree/3.3.1). Integração cloud não validada nesta execução.
