# Sinric Pro: Wake-on-LAN dual boot

Sinric Pro é a integração principal para ligar o PC e selecionar o sistema. Crie dois dispositivos do tipo **Switch** no portal: por exemplo, “PC Windows” e “PC Linux”. Copie App Key, App Secret e os dois Device IDs para a dashboard e mapeie cada um para a respectiva entrada `Boot####` descoberta. Os rótulos são livres; o mapeamento usa o ID UEFI, não o nome do sistema.

O fluxo completo é `Switch ON` → ESP32 salva o target em NVS → ESP32 envia Wake-on-LAN → UEFI inicia iPXE local → ESP32 entrega `/boot.ipxe` → `RemoteBoot.efi` carrega o loader EFI local correspondente. Portanto, iPXE e `RemoteBoot.efi` são parte necessária da seleção remota dual boot; agents são opcionais e servem para status/reboot remoto com o PC já ligado.

## Configuração

1. Conclua o onboarding Wi-Fi, MAC Ethernet, broadcast e reserva DHCP da ESP32 descritos no README.
2. Configure WoL no BIOS/UEFI e nos dois sistemas operacionais.
3. Instale a entrada Remote Boot e execute um teste único (`TEST`) antes de promovê-la.
4. Na dashboard, sincronize o catálogo e confirme quais entradas `Boot####` carregam Windows e Linux.
5. No portal Sinric Pro, crie dois Switches e cole App Key, App Secret e os Device IDs na dashboard.
6. Mapeie cada Device ID ao Boot ID correto, salve e teste um Switch por vez com o PC desligado.

Não mapeie um Switch a `default` enquanto ainda estiver validando os dois sistemas. Use `default` somente depois de definir padrão/fallback e testar ambos.

A implementação usa `SinricProSwitch`, `onPowerState`, `sendPowerStateEvent` e `SinricPro.begin` do SDK 3.3.1. Não cria dispositivos no portal nem inventa enumeração de modes. O limite local de oito slots não promete oito dispositivos gratuitos: plano/limites da conta são separados.

ON enfileira boot; OFF não desliga o PC. Após o evento aceito, o Switch é devolvido para OFF. Pedido com PC online é rejeitado. Após edição de credenciais/slots, salvar reinicia o ESP32 para registrar os callbacks atualizados.

Fonte: [SDK SinricPro 3.3.1](https://github.com/sinricpro/esp8266-esp32-sdk/tree/3.3.1). Integração cloud não validada nesta execução.
