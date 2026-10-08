# API v2 e protocolo do agent

O firmware aceita somente a API v2 e configuração schema 3. Clientes v1 recebem `410 PROTOCOL_VERSION_UNSUPPORTED`; não há heartbeat HTTP legado nem token global de agent. JSON é usado em todas as rotas. O handler limita corpos a 12.000 bytes, mas o `WebServer` pode alocar o corpo antes dessa checagem.

Rotas administrativas usam `Authorization: Bearer <admin_token>`, exceto cadastro inicial e boot público. Catálogos e comandos são escopados a um `pc_id`. O token de um agent autentica somente o vínculo salvo e a sessão correspondente; o servidor deriva o PC desse vínculo.

## Cadastro e configuração

| Método e rota | Acesso | Comportamento |
|---|---|---|
| GET `/api/v2/setup` | Público | Retorna `required` e `config_locked`. |
| POST `/api/v2/setup` | Público, somente no primeiro cadastro | Aceita `{password,repeat_password}`; salva senha admin e listas vazias, depois reinicia. |
| GET `/api/v2/bootstrap` | Admin | Configuração global sanitizada, até quatro PCs sem catálogos, até oito vínculos sem tokens e estado resumido. Segredos são substituídos por flags `*_set`. |
| GET `/api/v2/status` | Admin | Estado global, Wi-Fi, Tailscale, Sinric, limites e heap. |
| PUT `/api/v2/config` | Admin | Patch global: `admin_token`, `tailscale_auth_key`, `tailscale_device_name`, `dhcp`, `ip`, `subnet`, `gateway`, `dns`. Salva e reinicia. |
| POST `/api/v2/system/reboot` | Admin | Reinicia a ESP32. |
| POST `/api/v2/system/reset` | Admin | Exige `{confirm:"FACTORY_RESET"}`, apaga a configuração NVS e reinicia para novo cadastro. |

A senha administrativa tem 8–128 bytes e não pode conter caracteres ASCII de controle. Configuração salva contém `config_version: 3`. A API não fornece uma rota que devolva segredos: bootstrap só informa se estão definidos.

## PCs, catálogo e comandos

| Método e rota | Acesso | Comportamento |
|---|---|---|
| GET `/api/v2/pcs` | Admin | Lista PCs sem o array de sistemas. |
| POST `/api/v2/pcs` | Admin | Cria PC. Aceita `name`, `mac`, WoL, TTL e política de boot; o firmware gera `pc_id`. MAC Ethernet deve ser válido e único entre PCs. |
| GET `/api/v2/pcs/{pc_id}` | Admin | Configuração do PC, `systems_count` e estado; sem catálogo completo. |
| PUT `/api/v2/pcs/{pc_id}` | Admin | Atualiza nome, MAC, WoL, TTL, política, padrão ou fallback. |
| DELETE `/api/v2/pcs/{pc_id}` | Admin | Exige `{confirm:"DELETE_PC"}`; remove vínculos e estado do PC. Remover o PC Sinric selecionado também desativa Sinric e limpa seus slots. |
| GET `/api/v2/pcs/{pc_id}/status` | Admin | Estado daquele PC e agent. |
| GET `/api/v2/pcs/{pc_id}/systems?offset=0&limit=8` | Admin | Página do catálogo, máximo oito entradas. Resposta inclui `offset`, `total` e `generation`; se a geração mudar durante a leitura, repita a paginação. |
| PUT `/api/v2/pcs/{pc_id}/systems` | Admin | Atualiza visibilidade para o catálogo atual daquele PC. |
| POST `/api/v2/pcs/{pc_id}/discovery` | Admin | Solicita descoberta ao agent conectado; `/discovery/request` também é aceito. |
| POST `/api/v2/pcs/{pc_id}/boot` | Admin | `{boot_id:"0001"}` agenda WoL e boot. `force:true` exige `confirm:"FORCE_BOOT"`. |
| POST `/api/v2/pcs/{pc_id}/reboot` | Admin | `{boot_id:"0001",confirm:"REBOOT"}`; exige agent online com reboot habilitado. |
| POST `/api/v2/pcs/{pc_id}/shutdown` | Admin | `{confirm:"SHUTDOWN"}`; exige agent online com shutdown habilitado. |
| GET `/api/v2/agents` | Admin | Lista PC, instalação, OS, presença e permissões reportadas; não retorna tokens. |
| DELETE `/api/v2/agents/{agent_id}` | Admin | Revoga vínculo e desconecta sua sessão. |
| GET `/api/v2/logs` | Admin | Últimos 32 eventos em RAM, sem tokens ou payloads. |

Limites: quatro PCs, oito instalações de agent no total e 24 entradas UEFI por PC. Dois agents podem pertencer ao mesmo PC, por exemplo Windows e Linux, mas somente uma sessão por PC fica ativa. Nomes e hostnames não são identidade. `pc_id` e `agent_id` têm 32 dígitos hexadecimais minúsculos. IDs Boot#### são strings de quatro dígitos hexadecimais.

Cada PC tem `default_target`, `fallback_boot_id`, `last_selected_target`, `pending_ttl_s` (30–3.600, padrão 180) e `physical_boot_behavior` (`default_target`, `last_selected`, `exit_to_firmware`). Catálogo é sincronizado pelo agent nativo autenticado, limitado a 24 entradas. O firmware preserva nome/visibilidade editados e remove referências a entradas que deixaram de existir.

`GET /boot/{pc_id}.ipxe` é público na LAN e entrega somente o estado de boot daquele PC. Não existe destino implícito. Ausência de PC retorna 404 sem consumir pending de outro PC.

## Pareamento

| Método e rota | Acesso | Comportamento |
|---|---|---|
| POST `/api/v2/pairing/window` | Admin | Abre janela de cinco minutos. |
| DELETE `/api/v2/pairing/window` | Admin | Fecha a janela e cancela solicitações ainda não aprovadas. |
| POST `/api/v2/pairing/start` | Agent sem vínculo, durante a janela | Recebe `{hostname,os}` e retorna `pairing_id`, `device_secret`, código de oito caracteres, `expires_in:300` e `poll_interval:2`. |
| POST `/api/v2/pairing/lookup` | Admin | `{code}` retorna hostname/OS não confiáveis e ID da solicitação correspondente. |
| POST `/api/v2/pairing/{pairing_id}/approve` | Admin | `{pc_id,installation_name}` persiste novo agent e vínculo ao PC antes de permitir entrega. |
| DELETE `/api/v2/pairing/{pairing_id}` | Admin | Cancela a solicitação. |
| POST `/api/v2/pairing/{pairing_id}/poll` | Agent solicitante, Bearer `device_secret` | Retorna `pending`, `approved`, `canceled`, `expired` ou `confirmed`; após aprovação entrega `agent_id`, `pc_id`, `token` e `protocol:2` somente ao dono da solicitação. |
| POST `/api/v2/pairing/{pairing_id}/confirm` | Agent solicitante, Bearer `device_secret` | Confirma a credencial depois de salvá-la e validar hello. Repetir confirmação é idempotente por cinco minutos. |

Há no máximo três solicitações simultâneas, seis inícios por minuto e cinco consultas administrativas por minuto; exceder consultas bloqueia novas tentativas por 60 segundos. A credencial permanente do agent é de 256 bits. O segredo temporário e a credencial nunca são enviados em URL. O fluxo usa HTTP na rede configurada, sem TLS.

## Agent nativo

O WebSocket é `ws://ESP32:81/agent/v2`. Hello usa `protocol:2`, `agent_id`, token, `session_id`, hostname, OS, Boot ID e permissões `{reboot,shutdown}`. O servidor resolve o PC pelo vínculo do agent; `pc_id` não concede acesso. A resposta `ready` e os frames subsequentes carregam `pc_id`, `agent_id` e `session_id`.

Comandos, ACK e resultado também incluem `id`. A sessão precisa coincidir em cada mensagem. A fila reside em RAM por PC e expira em 30 segundos; o agent precisa ACK em até 15 segundos. Keepalive é 60 segundos e presença expira em 90 segundos. O agent grava o ID do comando antes do ACK e só executa após confirmação positiva; não executa shell arbitrário.

O agent sincroniza seu catálogo em `POST /api/v2/agent/systems/sync`, com Bearer token individual e `X-Agent-Session`. O servidor deriva o PC do token e exige sessão WebSocket ativa. Mensagens de aplicação têm limite de 12.000 bytes.

## Sinric

Sinric pode ser vinculado a somente um `sinric_pc_id` por vez, com até oito slots globais. Cada slot usa um Device ID e `boot_id` pertencente a esse PC, ou `default`/`shutdown`. Alterar o PC selecionado limpa os slots; remover esse PC desativa a integração. Ver [Sinric](sinric.md).

## Erros e limites

Erros têm formato `{error:"CODIGO"}`. Os mais comuns são `AUTH_REQUIRED` (401), `FORBIDDEN` (403), `NOT_FOUND`/`PC_NOT_FOUND` (404), `SETUP_CLOSED`, conflito de estado/sessão (409), `BODY_TOO_LARGE` (413), limite de pareamento/WoL (429), `NVS_WRITE_FAILED` (500) e `PROTOCOL_VERSION_UNSUPPORTED` (410). Um `202` confirma aceitação/enfileiramento, não que o PC acordou, reiniciou ou desligou fisicamente.

O padrão de firmware retorna `tailscale.built:false`; a variante MicroLink inclui estado de túnel e métricas. A auth key nunca é devolvida. API e WebSocket não têm TLS; restrinja-os à LAN confiável ou à rede privada configurada.
