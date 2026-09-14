# Dashboard

A dashboard usa HTML/CSS/Vanilla JS e opera totalmente na LAN, sem framework, CDN, fonte remota, LittleFS ou dependência de internet. As três fontes em `firmware/web/` são combinadas durante o build em um único asset gzip/PROGMEM.

## Navegação

- **Visão geral:** estado do PC, sistema detectado, IP/versão do ESP32, RSSI, Sinric e inicialização rápida.
- **Boot:** entradas UEFI, WoL, reboot pelo agent, desligamento e atualização do catálogo.
- **Configuração:** rede, computador, boot, tokens, visibilidade e ordem da dashboard.
- **Sinric Pro:** ativação, credenciais e até oito dispositivos Switch.
- **Sistema:** logs, reinício do ESP32 e restauração de fábrica.

No desktop, a navegação fica na lateral. Em telas menores ela vira uma faixa fixa no topo. A interface suporta 320 px, controles de toque de pelo menos 44 px, foco visível e `prefers-reduced-motion`.

Conecte usando o token; ele não é salvo no navegador. A dashboard consulta o status a cada 10 segundos. Os botões usam o ID real mesmo com descrições iguais. “Mostrar ocultos” não desbloqueia entradas iPXE/inativas. Alterar a ordem visual não modifica o `BootOrder`.

## Formulários e credenciais

DHCP oculta os campos de IP estático sem apagar os valores. Credenciais vazias mantêm o valor existente; os indicadores `*_set` informam apenas que existe um segredo armazenado. Para Wi-Fi aberto, envie explicitamente `wifi_password:""` pela API.

Ao ativar Sinric, App Key e App Secret novas precisam ter pelo menos 10 caracteres, ou já devem existir no ESP32. Device IDs devem conter exatamente 24 caracteres hexadecimais, não podem se repetir e precisam apontar para uma ação válida. A dashboard destaca o campo incorreto e não envia nem reinicia enquanto houver erro. Sinric ativo sem slots é permitido, mas exibe aviso porque não atenderá comandos.

O firmware repete a verificação antes de `SinricPro.begin()`. Configuração incompleta não inicia o SDK e registra uma vez `SINRIC_CONFIG_INCOMPLETE`.

## Operação

Rescan é pedido ao agent, não executado pela ESP32. Aguarde o evento WebSocket e reconecte para carregar um catálogo atualizado. Sem padrão válido, o script retorna ao firmware.

Forçar WoL exige confirmação e não reinicia um PC em uso. “Reiniciar aqui” usa o canal do agent e confirmação separada. Ações destrutivas permanecem na seção Sistema.

