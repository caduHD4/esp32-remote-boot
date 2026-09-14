# MicroLink + Tailscale no ESP32-C3 — Design

## Objetivo

Criar uma variante experimental do firmware que coloque o próprio ESP32-C3 na tailnet via MicroLink, sem hardware auxiliar, preservando o firmware padrão e todo o fluxo UEFI/iPXE já validado.

## Arquitetura

- Base: `feat/dashboard-ui-sinric-validation`.
- Build padrão: `esp32c3_4mb`, Arduino puro, comportamento existente inalterado.
- Build experimental: `esp32c3_4mb_microlink`, Arduino como componente do ESP-IDF.
- MicroLink vendorizado em commit fixo, com licença e registro de patches locais.
- Credencial Tailscale somente em `config.local.microlink.json`, ignorada pelo Git e convertida em header durante o build.
- O servidor `WebServer` existente continua na porta 80 e deve responder pela interface LAN e pela interface WireGuard.
- Falha, ausência de credencial, setup inicial, configuração bloqueada ou Wi-Fi offline nunca devem impedir a dashboard local.

## Restrições do ESP32-C3

- Chip single-core: nenhuma tarefa pode ser fixada no core 1.
- Placa alvo sem PSRAM: buffers do controle devem caber na SRAM interna.
- HTTP/2 aceita frames padrão de até 16.384 bytes; o buffer temporário Noise será 24 KiB para incluir overhead sem manter o upstream de 64 KiB.
- Máximo de 8 peers ativos, cache de 16 peers, buffers H2/JSON de 64 KiB e MTU WireGuard 1280.
- Sem OTA/rollback; o ambiente experimental permanece opt-in.

## Segurança

- Nenhum auth key real em Git, logs, testes ou exemplo.
- O gerador rejeita JSON inválido, chave sem prefixo `tskey-auth-`, controles ASCII e nomes acima de 63 bytes.
- A chave é compilada no firmware e pode ser extraída da flash sem Secure Boot/Flash Encryption; isso deve constar na documentação.
- A dashboard continua exigindo Bearer token mesmo através da tailnet.

## Estado e interface

`GET /api/v1/status` expõe `tailscale` com `built`, `configured`, `connected`, `state`, `ip`, `peers`, `heap_free`, `heap_minimum` e `largest_block`. A dashboard mostra um quinto card responsivo com estado/IP e não oferece edição remota da auth key.

## Fora de escopo

- Exit node, subnet router, MagicDNS obrigatório e administração da tailnet.
- Alterar `boot.ipxe`, `RemoteBoot.efi`, `BootNext`, fallback, Sinric ou agents.
- Prometer estabilidade em tailnets grandes ou produção sem teste físico.

