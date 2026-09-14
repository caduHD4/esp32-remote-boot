# Multi-PC sem SoftAP — Implementation Plan

> Branch: `feat/multi-pc-no-ap`
> Base: `958078f`
> Design: `docs/superpowers/specs/2026-09-14-multi-pc-no-ap-design.md`

## Objetivo

Migrar o firmware para schema 3 com até quatro computadores isolados, agent opcional, roteamento iPXE por MAC e nenhuma lógica SoftAP, preservando o firmware padrão e a variante MicroLink.

## Task 1 — Políticas testáveis de configuração e seleção

**Files**

- Add: `firmware/include/computer_policy.hpp`
- Modify: `firmware/include/config_policy.hpp`
- Modify: `firmware/include/boot_state.hpp`
- Add: `tests/test_computer_policy.cpp`
- Modify: `tests/test_config.cpp`
- Modify: `tests/run.sh`

**TDD**

1. Criar testes inicialmente falhos para:
   - migração schema 2 → 3;
   - preservação de todos os campos legados;
   - conversão dos slots Sinric;
   - máximo de quatro computadores;
   - IDs, MACs e tokens únicos;
   - agent token vazio permitido;
   - redação de tokens dentro de `computers[]`;
   - resolução administrativa com um computador e erro ambíguo com múltiplos;
   - lookup por MAC e lookup do agent por token.
2. Implementar helpers sem dependência de Arduino para permitir testes nativos.
3. Executar somente os testes novos e confirmar passagem.
4. Commit: `feat: add schema v3 multi-computer policies`.

## Task 2 — Runtime independente e Wake-on-LAN

**Files**

- Add: `firmware/include/computer_runtime.hpp`
- Modify: `firmware/src/main.cpp`
- Add: `tests/test_computer_runtime.cpp`
- Modify: `tests/run.sh`

**TDD**

1. Criar testes falhos para isolamento de:
   - pending/default/last;
   - cooldown e repetição WoL;
   - heartbeat/presença;
   - comandos reboot/shutdown;
   - geração de descoberta.
2. Substituir os globais de PC único por quatro slots de runtime indexados pelo `computer.id`.
3. Fazer `reloadState()` reconstruir cada runtime sem compartilhar estado.
4. Alterar WoL para consumir MAC/porta/repetição/intervalo do computador selecionado.
5. Garantir que remoção/reload descarte filas e sessões órfãs.
6. Executar testes nativos relacionados.
7. Commit: `feat: isolate runtime for up to four computers`.

## Task 3 — API schema 3 e compatibilidade

**Files**

- Modify: `firmware/src/main.cpp`
- Modify: `docs/api.md`
- Modify: `config.example.json`
- Add: `tests/test_multi_pc_api.py`
- Modify: `tests/api_smoke.py`
- Modify: `tests/run.sh`

**TDD**

1. Criar testes falhos para `computers[]` em config/status/bootstrap.
2. Exigir `computer_id` em boot/reboot/shutdown/discovery com múltiplos PCs.
3. Aceitar ausência do ID somente quando houver exatamente um PC.
4. Resolver `systems`, `systems/sync` e heartbeat por agent token; admin deve informar ID.
5. Manter secrets fora de config/status/logs.
6. Aplicar config como transação: validar tudo, limpar referências órfãs e só então persistir.
7. Commit: `feat: expose multi-computer api`.

## Task 4 — iPXE por MAC com transição compatível

**Files**

- Modify: `ipxe/remote-boot.ipxe.in`
- Modify: `firmware/src/main.cpp`
- Add: `firmware/include/ipxe_dispatch.hpp`
- Add: `tests/test_ipxe_dispatch.cpp`
- Modify: `tests/run.sh`
- Modify: `docs/api.md`
- Modify: `docs/architecture.md`

**TDD**

1. Testar script intermediário quando `mac` estiver ausente.
2. Testar normalização e seleção por MAC válido.
3. Testar MAC inválido/desconhecido retornando saída segura.
4. Testar isolamento de pending/fallback entre dois PCs.
5. Atualizar o template novo para `/boot.ipxe?mac=${net0/mac}`.
6. Confirmar que o script existente continua funcional via `chain`.
7. Commit: `feat: route ipxe boot requests by mac`.

## Task 5 — Agents simultâneos e opcionais

**Files**

- Modify: `firmware/src/main.cpp`
- Modify: `platformio.ini`
- Modify: `agent/**` somente se o protocolo exigir compatibilidade adicional
- Add: `tests/test_agent_routing.cpp`
- Modify: `tests/test_shutdown.sh`
- Modify: `tests/test_discovery.sh`

**TDD**

1. Testar token → computador e rejeição de token vazio/duplicado.
2. Testar até quatro sessões, uma por computador.
3. Testar rejeição de segundo socket para o mesmo computador.
4. Vincular heartbeat, discovery, catálogo e ACK ao runtime correto.
5. Aumentar `WEBSOCKETS_SERVER_CLIENT_MAX` apenas ao mínimo necessário e verificar impacto de RAM nos dois builds.
6. Preservar protocolo do agent atual para o computador migrado.
7. Commit: `feat: route optional agents per computer`.

## Task 6 — Sinric por computador

**Files**

- Modify: `firmware/include/sinric_policy.hpp`
- Modify: `firmware/src/main.cpp`
- Modify: `tests/test_sinric_policy.cpp`
- Modify: `docs/sinric.md`

**TDD**

1. Testar migração e validação de `computer_id`, `action` e `boot_id`.
2. Testar `wake`, `boot`, `reboot` e `shutdown` no runtime correto.
3. Testar remoção/rejeição de slots órfãos.
4. Preservar o reset visual OFF sem repetir a ação.
5. Não usar “Sinric conectado” como presença do PC.
6. Commit: `feat: route sinric actions per computer`.

## Task 7 — Remover SoftAP e manter reconexão STA

**Files**

- Modify: `firmware/src/main.cpp`
- Delete: `firmware/include/setup_network_policy.hpp`
- Delete: `tests/test_setup_network.cpp`
- Modify: `platformio.ini`
- Modify: `docs/setup.md`
- Modify: `docs/troubleshooting.md`
- Modify: `tests/run.sh`
- Add: `tests/test_wifi_policy.cpp`

**TDD**

1. Testar política: falha inicial e perda posterior nunca iniciam AP.
2. Remover `DNSServer`, captive portal, `setupAP()`, `WIFI_AP_STA` e processamento DNS.
3. Manter `WIFI_STA`, credenciais locais/NVS e auto-reconnect.
4. Exibir diagnóstico serial quando não houver credenciais ou DHCP.
5. Garantir que Wi-Fi offline não bloqueie a retomada automática.
6. Commit: `refactor: remove softap setup mode`.

## Task 8 — Dashboard multi-PC responsiva

**Files**

- Modify: `firmware/web/index.html`
- Modify: `firmware/web/app.js`
- Modify: `firmware/web/app.css`
- Modify: `tests/test_dashboard_ui.py`
- Modify: `tests/test_dashboard_validation.js`
- Add: `tests/test_dashboard_multi_pc.js`
- Regenerate: `firmware/include/web_asset.h`

**TDD**

1. Criar testes DOM/política para seletor ativo, quatro cards e payloads com `computer_id`.
2. Exibir WoL em todos os PCs e ocultar/desabilitar recursos que dependam de agent.
3. Implementar adicionar/editar/remover com máximo quatro e confirmação nominal.
4. Adaptar slots Sinric para computador/ação/boot.
5. Manter um único polling de status, pausa em aba oculta, timeout e bootstrap único.
6. Validar desktop/mobile e regenerar asset pelo script existente.
7. Commit: `feat: add multi-computer dashboard`.

## Task 9 — Verificação integral e publicação

1. Executar `bash tests/run.sh`.
2. Executar ShellCheck e verificação de secrets.
3. Executar `pio run -e esp32c3_4mb`.
4. Executar `pio run -e esp32c3_4mb_microlink`.
5. Conferir tamanho de flash/RAM e rejeitar regressão que ultrapasse os gates existentes.
6. Revisar diff completo contra `958078f`.
7. Atualizar arquitetura, API, setup, Sinric, troubleshooting e versão.
8. Publicar todos os commits em `feat/multi-pc-no-ap` usando a integração GitHub.
9. Confirmar workflow verde antes de declarar concluído.

## Validação física após firmware

- Atualizar a ESP32 sem `erase-flash` e confirmar migração do PC atual.
- Reiniciar ESP32 antes do roteador e confirmar reconexão posterior sem AP.
- Validar WoL em dois PCs com MACs diferentes.
- Validar agent ausente em um PC e presente no outro.
- Validar iPXE antigo e template novo.
- Validar Sinric direcionando os dois PCs.
- Validar dashboard pela LAN e pelo IP Tailscale.
