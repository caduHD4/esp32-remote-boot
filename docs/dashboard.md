# Dashboard

UI única em HTML/CSS/Vanilla JS, comprimida com gzip em PROGMEM no build. Sem frontend framework, CDN, LittleFS ou dependência de internet para operar localmente.

Conecte usando token; ele não é salvo no navegador. A home consulta status a cada 10 s. Os botões usam ID real mesmo com descrições iguais. “Mostrar entradas ocultas” não desbloqueia iPXE/entradas inativas. A seta sobe uma entrada na ordem; salve para persistir.

A seção Rede permite DHCP ou IP/subnet/gateway/DNS estáticos; prefira reserva DHCP para manter o endereço do build iPXE. A seção Boot controla o botão físico do PC, não GPIO da ESP32. Tokens e credenciais vazios no formulário são mantidos; para Wi-Fi aberto, envie explicitamente `wifi_password:""` pela API.

Rescan é pedido ao agent, não enumeração feita pela ESP32. Aguarde o evento WebSocket e a sincronização e reconecte na dashboard para carregar o catálogo atualizado. Se o agent estiver offline, a atualização aguarda sua volta. Sem padrão válido, o script retorna ao firmware.

Forçar WoL exige confirmação e não desliga/reinicia um PC em uso. Reboot into usa canal do agent e confirmação separada.
