# API v1

JSON; header `Authorization: Bearer TOKEN`. Corpo máximo aceito pelo handler: 12.000 bytes. O WebServer pode alocar o corpo antes dessa checagem: não é proteção integral contra DoS. Admin é aceito em todas as rotas; token agent só nas rotas marcadas.

| Método / rota | Permissão | Corpo/resultado |
|---|---|---|
| GET `/api/v1/status` | admin | online, os, IP, RSSI, heap, uptime, Sinric, Tailscale, padrão/pending/última escolha |
| GET `/api/v1/config` | admin | configuração sem valores secretos |
| PUT `/api/v1/config` | admin | patch de campos permitidos; valida, salva e reinicia ESP32 |
| GET `/api/v1/systems` | admin/agent | `{systems:[{id,name,hidden,blocked}]}` |
| POST `/api/v1/systems/sync` | admin/agent | mesmo objeto de catálogo; máximo 24, IDs únicos |
| POST `/api/v1/boot` | admin | `{boot_id:"0001"}`; 202 fila WoL, 409 online |
| POST `/api/v1/discovery/request` | admin | `{}`; envia evento discover ao agent conectado; mantém geração para legado |
| POST `/api/v1/heartbeat` | admin/agent | hostname, os, boot_id opcional, uptime, ack opcional; devolve geração e eventual comando |
| GET `/api/v1/logs` | admin | últimas 32 mensagens RAM |
| POST `/api/v1/reboot` | admin | `{boot_id:"0001",confirm:"REBOOT"}`; exige heartbeat online |
| POST `/api/v1/system/reboot` | admin | `{}`; reinicia ESP32 |
| POST `/api/v1/system/reset` | admin | `{confirm:"FACTORY_RESET"}` |
| GET `/boot.ipxe` | público | script de boot, sem secrets |

`force:true` no boot exige `confirm:"FORCE_BOOT"`; não reinicia o PC. Reboot do PC fica disponível ao agent por 30 s. O agent valida Boot####, grava BootNext e persiste o ID de comando antes do ack e reboot; falha no ack cancela BootNext. Um comando consumido não é repetido automaticamente.

Pending TTL: 30–3.600 s, padrão 180. Agent nativo: WebSocket/keepalive 60 s, expiração de presença 90 s; HTTP legado: heartbeat 12 s, offline 45 s. WoL: porta 1–65535, 1–10 repetições, intervalo 20–1000 ms e cooldown 3 s. Offline significa ausência de heartbeat, não confirmação elétrica de desligamento.

Schema/config: veja `config.example.json` para patch sanitizado; ele não contém credenciais utilizáveis. `default_target`, `fallback_boot_id` aceitam string vazia para nenhum. `physical_boot_behavior`: `default_target`, `last_selected`, `exit_to_firmware`. Sinric slots: `[{device_id:"ID_REAL",boot_id:"0001"}]` ou `boot_id:"default"`. Ativar Sinric sem App Key/App Secret válidos retorna `SINRIC_CREDENTIALS_REQUIRED`.

Erros são `{error:"CODIGO"}`. 400 entrada inválida, 401/403 autenticação, 409 conflito, 413 corpo grande, 429 cooldown, 500 NVS, 503 indisponível. A API não garante que WoL acordou a máquina: 202 confirma apenas fila aceita.

## Estado MicroLink/Tailscale

No firmware padrão, `tailscale` retorna `built:false` e `state:"disabled"`. No ambiente experimental, o objeto contém `built`, `configured`, `connected`, `state`, `ip`, `peers`, `heap_free`, `heap_minimum` e `largest_block`. Estados possíveis incluem `not_configured`, `config_locked`, `setup_mode`, `wifi_offline`, `starting`, `connecting`, `registering`, `connected`, `reconnecting` e `error`.

A API nunca retorna a auth key. Ela continua protegida pelo Bearer token administrativo tanto na LAN quanto pelo IP Tailscale. Falha do túnel não muda a disponibilidade da API na LAN.

## Shutdown

`POST /api/v1/shutdown`, token administrativo, body `{"confirm":"SHUTDOWN"}`. Retorna 202 com `queued: true` quando o agent está online, com sessão e shutdown habilitado. 400 exige confirmação, 403 indica permissão desabilitada, 409 indica agent offline ou comando pendente, 503 indica firmware indisponível/setup bloqueado.

O heartbeat HTTP legado envia `session_id` (nonce novo por inicialização do processo) e `shutdown_enabled`. A resposta pode trazer `command: {id, action:"shutdown", boot_id:"", session_id}`. O agent persiste o ID, envia um novo heartbeat com `ack` e executa somente se a resposta tiver `ack_accepted: true`. A fila é única para reboot/shutdown, reside em RAM e expira em 30 segundos; sessão diferente ou permissão revogada cancelam shutdown. Não enviar comandos arbitrários de shell. Status inclui `shutdown_enabled` efetivo.

Slots Sinric também aceitam `boot_id:"shutdown"`; ON solicita shutdown. OFF permanece sem ação para todos os slots. O ACK significa consumo autorizado do comando, não conclusão do shutdown físico.

## Canal WebSocket do agent nativo

`ws://ESP32:81/agent`, subprotocolo `arduino`. Autenticação inicial em até 5 s: `{type:"hello",token:"AGENT_TOKEN",session_id:"32_HEX",hostname,os,boot_id,reboot_enabled,shutdown_enabled}`. A resposta `ready` devolve `session_id`. Somente o agent token é aceito neste canal. Um único agent autenticado; novos concorrentes são rejeitados.

ESP32 envia `{type:"command",id,action:"reboot"|"shutdown",boot_id,session_id}`. Agent responde `{type:"ack",id,session_id}`; ESP32 devolve `{type:"ack",id,session_id,accepted:true|false}`. Somente ACK positivo autoriza a operação. Agent informa `{type:"result",id,requested:true|false}`; isso indica aceitação local pelo OS, não término físico. Descoberta: ESP32 envia `{type:"discover"}`, agent envia catálogo por HTTP `/systems/sync`. Catálogo também é enviado uma vez ao conectar. Não há varredura periódica.

A fila RAM usa a mesma expiração de 30 s. Agent limita espera/validade do ACK a 15 s; reconexão gera nova sessão. Reboot nativo só escreve BootNext depois do ACK, restaurando o valor anterior se o OS recusar reboot. Mensagens de aplicação limitadas a 12.000 bytes e sem fragmentação no firmware; o SDK pode alocar até 15 KiB antes do handler. Keepalive não transporta comandos. Status inclui `agent_transport` (`websocket`/`http-legacy`). Heartbeat HTTP retorna 409 enquanto o WebSocket está autenticado.
