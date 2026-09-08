# API v1

JSON; header `Authorization: Bearer TOKEN`. Corpo máximo aceito pelo handler: 12.000 bytes. O WebServer pode alocar o corpo antes dessa checagem: não é proteção integral contra DoS. Admin é aceito em todas as rotas; token agent só nas rotas marcadas.

| Método / rota | Permissão | Corpo/resultado |
|---|---|---|
| GET `/api/v1/status` | admin | online, os, IP, RSSI, heap, uptime, Sinric, padrão/pending/última escolha |
| GET `/api/v1/config` | admin | configuração sem valores secretos |
| PUT `/api/v1/config` | admin | patch de campos permitidos; valida, salva e reinicia ESP32 |
| GET `/api/v1/systems` | admin/agent | `{systems:[{id,name,hidden,blocked}]}` |
| POST `/api/v1/systems/sync` | admin/agent | mesmo objeto de catálogo; máximo 24, IDs únicos |
| POST `/api/v1/boot` | admin | `{boot_id:"0001"}`; 202 fila WoL, 409 online |
| POST `/api/v1/discovery/request` | admin | `{}`; incrementa geração consultada pelo agent |
| POST `/api/v1/heartbeat` | admin/agent | hostname, os, boot_id opcional, uptime, ack opcional; devolve geração e eventual comando |
| GET `/api/v1/logs` | admin | últimas 32 mensagens RAM |
| POST `/api/v1/reboot` | admin | `{boot_id:"0001",confirm:"REBOOT"}`; exige heartbeat online |
| POST `/api/v1/system/reboot` | admin | `{}`; reinicia ESP32 |
| POST `/api/v1/system/reset` | admin | `{confirm:"FACTORY_RESET"}` |
| GET `/boot.ipxe` | público | script de boot, sem secrets |

`force:true` no boot exige `confirm:"FORCE_BOOT"`; não reinicia o PC. Reboot do PC fica disponível ao agent por 30 s. O agent valida Boot####, grava BootNext e persiste o ID de comando antes do ack e reboot; falha no ack cancela BootNext. Um comando consumido não é repetido automaticamente.

Pending TTL: 30–3.600 s, padrão 180. Heartbeat: 12 s; offline 45 s. WoL: porta 1–65535, 1–10 repetições, intervalo 20–1000 ms e cooldown 3 s. Offline significa ausência de heartbeat, não confirmação elétrica de desligamento.

Schema/config: veja `config.example.json` para patch sanitizado; ele não contém credenciais utilizáveis. `default_target`, `fallback_boot_id` aceitam string vazia para nenhum. `physical_boot_behavior`: `default_target`, `last_selected`, `exit_to_firmware`. Sinric slots: `[{device_id:"ID_REAL",boot_id:"0001"}]` ou `boot_id:"default"`.

Erros são `{error:"CODIGO"}`. 400 entrada inválida, 401/403 autenticação, 409 conflito, 413 corpo grande, 429 cooldown, 500 NVS, 503 indisponível. A API não garante que WoL acordou a máquina: 202 confirma apenas fila aceita.
