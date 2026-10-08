# Sinric Pro: controle do PC selecionado

A integração Sinric usa um único PC selecionado por vez. Até oito Switches podem ser configurados, e todos enviam ações ao mesmo PC. Para alternar entre PCs, altere o PC Sinric selecionado na dashboard e configure novamente os alvos; a troca limpa os slots existentes. Remover o PC selecionado desativa e limpa Sinric. A integração não oferece roteamento de cada Switch para PCs diferentes.

Cada slot pode apontar para uma entrada `Boot####`, para `default` ou para `shutdown` (que requer agent pareado e permissão de desligamento). Entradas de sistemas diferentes no catálogo do mesmo PC podem ser controladas por Switches separados, por exemplo Windows e Linux em dual boot.

## Configuração

1. Configure WoL no BIOS/UEFI, na placa Ethernet e nos sistemas operacionais. O PC deve permanecer ligado à energia e à Ethernet.
2. Pareie o agent do sistema operacional com o PC desejado e sincronize o catálogo. Teste os destinos pela dashboard antes de configurar Sinric.
3. No portal [Sinric Pro](https://portal.sinric.pro), crie uma app e copie App Key e App Secret. Crie um ou mais dispositivos do tipo **Switch** e copie cada Device ID.
4. Na dashboard ESP32, selecione o PC Sinric. Ative Sinric, informe App Key/App Secret e associe cada Device ID a uma entrada UEFI, `default` ou `shutdown`, conforme necessário. Salve para aplicar; o ESP32 reinicia os callbacks Sinric.
5. Vincule a conta ao Alexa/Google Home usando a integração do portal e faça descoberta dos dispositivos. Teste cada Switch com o PC desligado.

O nome do Switch é livre; a seleção usa Device ID e alvo cadastrado. Os oito slots locais não garantem que o plano da conta Sinric aceite oito dispositivos.

## Comportamento e limites

ON de um slot de boot enfileira a seleção e envia Wake-on-LAN. O PC inicia o iPXE local, que consulta o URL de boot vinculado ao PC, e `RemoteBoot.efi` solicita a entrada UEFI. ON de `shutdown` encaminha desligamento ao agent. OFF não desliga o PC. Após evento aceito, o estado do Switch volta para OFF. Um pedido de boot é rejeitado se o agent informa que o PC está online. Sinric e o agent precisam estar conectados.

Para shutdown, habilite `SHUTDOWN` ao instalar o agent e confirme a permissão do PC na dashboard. Veja [shutdown](shutdown.md) para detalhes.

Se Sinric não ficar online, confira Wi-Fi/DNS, credenciais e se os Device IDs pertencem à app informada. Se o PC ligar no sistema errado, ajuste o mapeamento do Device ID para o `Boot####` correto. Não altere BootOrder para corrigir um slot Sinric.

A implementação usa `SinricProSwitch`, `onPowerState`, `sendPowerStateEvent` e `SinricPro.begin` do SDK 3.3.1. Ela não cria dispositivos no portal. Não inclua App Key, App Secret ou Device IDs em commits, issues ou capturas públicas. Credenciais são armazenadas na NVS e não são criptografadas por este profile.

Fonte: [SDK SinricPro 3.3.1](https://github.com/sinricpro/esp8266-esp32-sdk/tree/3.3.1). A integração cloud não foi validada nesta execução.
