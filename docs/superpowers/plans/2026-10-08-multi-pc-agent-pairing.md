# Multi-PC and Agent Pairing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Controlar vários PCs isoladamente, parear agents Windows/Linux por código curto e manter Sinric limitado a um PC escolhido.

**Architecture:** Schema 3 com configuração global e registros por PC/agent. Políticas C++ testáveis sem Arduino, adapters HTTP/WebSocket/NVS e agent C# com pareamento nativo. Dashboard e iPXE identificam explicitamente o PC.

**Tech Stack:** ESP32-C3, PlatformIO 6.1.19, Arduino/ESP-IDF, ArduinoJson 7.3.1, .NET 10 NativeAOT, JavaScript, Playwright, PowerShell e Bash.

**Spec:** `docs/superpowers/specs/2026-10-08-multi-pc-agent-pairing-design.md`.

**Test matrix:** `docs/testing/multi-pc-agent-pairing.md`.

## Global Constraints

- Branch `feat/multi-pc-agent-pairing`, criada da base validada `01c9cce`.
- Meta: 4 PCs, 8 vínculos de agent, 24 entradas UEFI/PC, 8 slots Sinric; Sinric controla somente `sinric_pc_id`.
- Instalação limpa, schema 3, API v2; sem migração ou compatibilidade silenciosa.
- IDs de 128 bits; credenciais e segredo de dispositivo de 256 bits. Código Crockford de 8 caracteres `ABCD-EFGH`, válido por 5 minutos.
- Pareamento: 3 pendentes, início 6/min global; consulta por código 5/min e bloqueio 60 s. Aprovação administrativa e persistência antes da entrega.
- Filas por PC: TTL 30 s; ACK agent 15 s; keepalive 60 s; presença 90 s. Quatro sessões ativas e um hello pendente.
- Requests/frames até 12.000 bytes; snapshot persistido até 20.000 bytes; NVS de 64 KiB, blobs de dois bancos.
- Tailscale exige `esp32c3_4mb_microlink`. Preservar guards TLS normais: heap >=60.000 e maior bloco >=24.000 bytes.
- Testes automáticos usam FakeHost/fixtures; nunca desligam, reiniciam ou escrevem UEFI do host de CI.

## Review Focus

1. Resposta atrasada de A após selecionar B não altera interface nem destino de comando: T6/UI-04.
2. Perda de resposta e reboot entre gravações não criam agent parcialmente aprovado: T2/T4/T5, NV-01/PA-07.
3. Dois agents no mesmo PC e Boot ID repetido em PCs diferentes não misturam catálogo/ACK: T1/T3/T5, IS-01/04/05.
4. Troca/remoção do PC Sinric não redireciona switches antigos: T8/SI-01..03.
5. Lotação e reconexão com Tailscale não derrubam o painel ou reduzem guards: T9/HW-03..04.

## Contratos a congelar antes do trabalho paralelo

`pc_id` e `agent_id`: 32 caracteres hexadecimais minúsculos, aleatórios. Nome e hostname não são identidade. Registro de agent resolve seu PC no servidor; um `pc_id` fornecido pelo cliente não concede permissão.

Cadastro administrativo: GET/POST `/api/v2/setup` substitui o fluxo v1, grava senha e listas vazias pelo ConfigStore e reinicia; depois fecha cadastro. POST `/api/v2/system/reset` e `/system/reboot` exigem admin e confirmacao para reset. Zero PCs e configuracao valida.

Admin: GET `/api/v2/bootstrap` retorna globais e resumo dos PCs; GET/POST `/api/v2/pcs`; GET/PUT/DELETE `/api/v2/pcs/{pc_id}`; GET status/systems e POST boot/reboot/shutdown/discovery sob esse PC. GET `/api/v2/agents`, DELETE `/api/v2/agents/{agent_id}` e PUT `/api/v2/integrations/sinric`. Configuração e respostas nunca incluem segredos aninhados.

Pareamento admin: POST/DELETE `/api/v2/pairing/window`; POST `/api/v2/pairing/lookup` com `{code}`; POST `/api/v2/pairing/{pairing_id}/approve` com `{pc_id,installation_name}`; DELETE da solicitação para cancelar. Criar PC usa a rota PCs antes da aprovação.

Dispositivo: POST `/api/v2/pairing/start`, somente com janela aberta, retorna `{pairing_id,device_secret,user_code,expires_in:300,poll_interval:2}`. POST `/api/v2/pairing/{pairing_id}/poll` e `/confirm` usam segredo no header Authorization. Poll retorna pending/approved/canceled/expired; somente ao dono aprovado entrega `{agent_id,pc_id,token,protocol:2}`. Repetir entrega retorna a mesma credencial até confirmação/TTL; confirm é idempotente. Segredos não vão para URL ou argv.

Agent: WebSocket `/agent/v2`, hello `{protocol:2,agent_id,token,session_id,hostname,os,boot_id,permissions}`. Comandos/ACK/result incluem `pc_id,agent_id,session_id,id`, verificados em ambos os lados. Sync POST `/api/v2/agent/systems/sync` usa token individual e `X-Agent-Session`; alvo derivado do registro. Cliente antigo recebe `PROTOCOL_VERSION_UNSUPPORTED`.

Catálogo paginado por PC: `offset`, `limit<=8`, `generation`; repetir leitura se a geração mudar. Bootstrap não carrega 96 entradas de uma vez. Erros separados de rede, JSON, autenticação, estado e persistência. Exemplos: `PC_LIMIT`, `MAC_ALREADY_REGISTERED`, `PAIRING_CLOSED`, `PAIRING_EXPIRED`, `PAIRING_RATE_LIMIT`, `SESSION_CONFLICT`, `AGENT_REVOKED`, `NVS_WRITE_FAILED`, `CONFIG_TOO_LARGE`.

## T1 — Registro de PCs e estado isolado

**Files:** criar `firmware/include/pc_registry.hpp`, `tests/test_pc_registry.cpp`; adaptar `boot_state.hpp`, `power_command.hpp`, `tests/test_core.cpp`, `tests/test_boot_dispatch.cpp`, `tests/test_power.cpp`.

**Interfaces:** `PcRegistry::find(id)`, `add(PcConfig)`, `remove(id)`, `runtime(id)`; `PcRuntime` contém State, PowerCommand, presença/sessão e parâmetros WoL próprios.

- [ ] Escrever IS-01..06: A/B com Boot ID 0001, pending/cooldown/catálogos diferentes, quinto PC, MAC duplicado e conflito de sessões.
- [ ] Compilar e executar teste C++ com `-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined`; observar falha por recurso ausente antes da implementação.
- [ ] Implementar registro limitado e substituir estado global único por estado por PC. Preservar TTL e confirmações.
- [ ] Rodar teste novo e core/power/dispatch; corrigir falhas e fazer commit.

## T2 — Persistência e schema 3

**Files:** criar `firmware/include/config_store.hpp`, `firmware/src/config_store.cpp`, `tests/test_config_store.cpp`; adaptar `config_policy.hpp`, `tests/test_config.cpp`, `config.example.json`, `partitions/esp32c3_4mb.csv`, `scripts/size_gate.py`.

**Interfaces:** `ConfigStore::load()` e `save(snapshot)` com resultado tipado; adapter NVS lê/grava blobs e seletor. Snapshot contém globals, pcs e agents.

- [ ] Escrever NV-01..05 com FakeStore e falha em cada operação: interrupção, CRC, seletor, limite, falta de espaço e redação de segredos; mostrar falhas esperadas.
- [ ] Implementar dois bancos com geração/comprimento/CRC; gravar e reler inativo antes de atualizar seletor atômico. Usar putBytes, sem limite de 4 KiB de putString.
- [ ] NVS: offset `0x9000`, tamanho `0x10000`; phy `0x19000`, tamanho `0x1000`; app `0x20000`, tamanho `0x3E0000`. Atualizar size gate conforme partição real.
- [ ] Testar 4x24 entradas +8 agents +8 slots, nomes UTF-8 no limite, snapshot <=20.000 bytes, IDs ausentes/duplicados e schema antigo. Repetir teste com sanitizers; commit.

## T3 — API, transporte e comandos por PC

**Files:** criar `firmware/src/api_routes.cpp`, `agent_transport.cpp`, `firmware/include/agent_sessions.hpp`, `tests/test_agent_sessions.cpp`; adaptar `main.cpp`, `tests/api_smoke.py`.

**Interfaces:** contratos acima; servidor resolve token para agent/PC. Main compõe adapters e loop, sem duplicar regras nos handlers.

- [ ] Escrever IS-02..08/API-01..05: token A acessando B, ACK incorreto, revogação, sessões concorrentes, replay, frame excessivo/JSON inválido e strings JSON com ownership correto.
- [ ] Observar testes falhando; implementar adapters finos, quatro sessões e um hello com autenticação em até 5 s.
- [ ] Comandos, descoberta, WoL, presença e catálogo independentes; cancelar fila ao desconectar/revogar/remover. Sync exige sessão ativa e preserva preferências daquele PC.
- [ ] Implementar paginação e HTTP erros tipados; não aceitar senha admin como identidade de agent.
- [ ] Rodar suite core, contrato e compilar ambas as variantes; commit.

## T4 — Pareamento no firmware

**Files:** criar `firmware/include/pairing_policy.hpp`, `firmware/src/pairing_service.cpp`, `tests/test_pairing.cpp`; integrar rotas e ConfigStore.

**Interfaces:** `PairingService::open(now)`, `start(metadata,now)`, `lookup(code,now)`, `approve(id,pc,now)`, `poll(id,secret,now)`, `confirm(id,secret,now)`, `cancel(id)`. RNG/storage injetáveis para teste.

- [ ] Escrever PA-01..10: normalização do código, 300 s, wrap millis, limites, segredo incorreto, colisão, concorrência, cancelamento, reboot e NVS falhando; confirmar falhas antes do código.
- [ ] Implementar RNG seguro, comparação de segredo sem early exit, cotas globais, janela fechada por padrão e sem logs de credenciais.
- [ ] Se reboot interromper a entrega após aprovação persistida, UI identifica vínculo não conectado e orienta remover/repetir; não recuperar segredo por hostname. A aprovação persiste vínculo antes de entregar; resposta perdida permite recuperar o mesmo token, sem duplicar agent. Confirm duplicado é seguro. Reboot fecha janela; registro permanente aprovado persiste.
- [ ] Testar que terceiros, admin e outra solicitação não recebem o token pela rota de entrega; commit.

## T5 — Agent nativo e instaladores

**Files:** criar `agent/native/Pairing.cs`, `AgentIdentity.cs`; adaptar `Agent.cs`, `Host.cs`, `SelfTest.cs`, installers Windows/Linux e `docs/native-agent.md`.

**Interfaces:** CLI `--pair --url http://IP --config PATH` e `--config PATH`; config `{protocol:2,url,pc_id,agent_id,token,allow_reboot,allow_shutdown}`. PairingClient: timeout 8 s, poll 2 s, teto 5 min, sem proxy/redirect.

- [ ] Escrever AG-01..07 com servidor loopback/FakeHost: happy path, cancel/expire, resposta perdida, segredo incorreto, redirect, arquivo interrompido e identidade errada; rodar `dotnet run --project agent/native/RemoteBoot.Agent.csproj -- --self-test` antes de implementar.
- [ ] Implementar pareamento e gravação temp/flush/rename; imprimir somente código curto e estado. Preservar opt-in de energia e trava de instância.
- [ ] Installers protegem pastas antes de salvar segredo, executam pair, testam hello e só então habilitam serviço/tarefa. Windows ACL SYSTEM/Administradores; Linux arquivo0600/root. Cancelar não deixa serviço falsamente instalado.
- [ ] Testar installers com comandos OS substituídos por fixtures; sintaxe PowerShell/Bash e shellcheck. Publicar NativeAOT win-x64/linux-x64 e rodar self-test dos binários; commit.

## T6 — Dashboard multi-PC e pareamento

**Files:** criar `firmware/web/pc-model.js`, `pairing-ui.js`, `tests/ui/*.spec.js`, `tests/ui/fake-api.js`, `playwright.config.js`, `package.json` e lockfile; adaptar HTML/CSS/app/embed_web e testes JS.

**Interfaces:** estado `{selectedPcId,requestGeneration,pcs,agents}`; ação captura pcId na origem. API client diferencia HTTP/JSON/rede e cancela requests antigos. Asset único e determinístico preservado.

- [ ] Escrever UI-01..10 primeiro, fake API determinística com fixtures do contrato. Fixar Playwright no lockfile; `npx playwright test` deve falhar nos fluxos ausentes.
- [ ] Implementar cards, Adicionar PC, editar/remover, seleção e estados vazios. Separar globais dos ajustes do PC; remover entrada manual do token do agent.
- [ ] Implementar dialog pareamento, código, revisão, PC novo/existente, confirmar, timer/cancelar/erros. Não mostrar conectado antes do hello.
- [ ] Testar Chromium/Firefox/WebKit em 360/390/768/1280 px; teclado/foco/modal/labels, duas abas e resposta de A após seleção B. Asserções de DOM/rede/estado, não apenas screenshots.
- [ ] Rodar testes JS existentes e `python tests/test_web_asset.py`; commit.

## T7 — iPXE e instalação EFI por PC

**Files:** adaptar `ipxe/build.sh`, `remote-boot.ipxe.in`, installers EFI Windows/Linux e rota boot; criar `tests/test_pc_boot.sh`, atualizar fixtures EFI/CI.

**Interfaces:** `build.sh ESP_IPV4 PC_ID [output-directory] [timeout-ms]`; manifesto `{esp_ipv4,pc_id,ipxe_sha256,loader_sha256}`; rota `/boot/{pc_id}.ipxe`.

- [ ] Escrever BT-01..05: dois PCs com 0001/pending distintos, ID inválido/ausente, manifesto de outro PC, hash adulterado, guard por PC e fallback local.
- [ ] Build incorpora PC ID e gera manifesto; installer verifica o vínculo salvo e manifesto antes da escrita. Sem ID/PC removido, script sai com segurança sem consumir pending de outro PC.
- [ ] Testar com fixtures/mock efivars; compilar iPXE para dois IDs e verificar URLs/manifestos distintos. Preservar BootOrder/backups; parear agent não altera EFI.
- [ ] Rodar testes de boot/EFI existentes; commit.

## T8 — Sinric para apenas um PC escolhido

**Files:** adaptar `sinric_policy.hpp`, dispatch/api, UI Sinric, `tests/test_sinric_policy.cpp`, `tests/ui/sinric.spec.js`, `docs/sinric.md`.

**Interfaces:** global `sinric_pc_id`; slots `{device_id,boot_id}` somente para esse PC. Sem PC separado por switch nesta versão.

- [ ] Escrever SI-01..04/UI-08: só o escolhido recebe evento, IDs repetidos não colidem, troca exige confirmar e limpa slots, remoção desativa integração, sem catálogo/offline gera erro claro.
- [ ] Implementar seleção única e persistência atômica; trocar PC nunca reaproveita silenciosamente mapeamentos. Remover PC escolhido limpa slots e desativa Sinric.
- [ ] Rodar teste C++ e Playwright Sinric; commit.

## T9 — Integração, CI e hardware

**Files:** criar `tests/integration/test_multi_agent.py`, simulador de agents somente para testes, `tests/hardware/multi_pc_smoke.py`; atualizar run.sh/workflows/README/scan_secrets. Resultados em `docs/testing/results/`.

**Interfaces:** simulador segue fixtures compartilhadas; smoke real recebe URL/senha admin por entrada segura/env local, nunca os imprime. Mock não substitui firmware real.

- [ ] CI Linux: C++ ASan/UBSan, shellcheck, Python/JS, build das duas variantes/iPXE. Windows: installers e NativeAOT. Playwright nos três browsers. Guardar traces de falha sem credenciais reais.
- [ ] Validar offsets, app size, NVS e contratos no firmware real. Reset/upload apenas após gates de software, mantendo a branch anterior recuperável.
- [ ] Hardware: cadastro admin, dois PCs reais, dois OS no mesmo PC, pareamento/revogação LAN/Tailscale. Completar carga com quatro agents simulados sem operações de energia.
- [ ] Soak 2 h com quatro PCs, oito vínculos, Tailscale, polling e reconexões. Nenhum reset/watchdog, queda monótona de memória ou HTTP >8 s sob carga controlada. Medir baseline/peak/idle e manter guards TLS; falha bloqueia entrega.
- [ ] Testes deliberados de WoL/reboot/shutdown/boot/fallback em PCs de teste, depois simultâneos; registrar hardware e limitações. Não declarar validação Windows/Linux se apenas simulados.
- [ ] Documentar limites medidos/fluxo novo, empacotar binários/manifestos e preencher matriz PASS/FAIL/BLOCKED com evidências; commit final.

## Ordem e divisão entre subagents

Dependências: T1 → T2 → T3 → T4; depois T5 e T6 em paralelo; T7 depende de T1/T3/T5; T8 depende de T1/T3/T6; T9 integra todos. Não gravar implementação parcial sobre a ESP validada.

Recomendação: usar no máximo três subagents simultâneos mais o agente principal. Primeiro o principal fecha modelo/contratos e T1–T4. Depois distribuir:

| Responsável | Tarefas e propriedade exclusiva | Entrega e dependências |
|---|---|---|
| Principal/integrador | Firmware core/API/NVS/pairing; partições; workflows; testes de integração/hardware | T1–T4/T9; mantém contrato e revisa integração |
| Agent Windows/Linux | `agent/native/*`, `installer/*/install-agent.*`, testes de pareamento/installer e guia native-agent | T5; usa contrato congelado, sem editar firmware/UI |
| Dashboard | `firmware/web/*`, `scripts/embed_web.py`, `tests/ui/*`, package/Playwright e guia dashboard | T6 e UI de T8; consome fixtures do contrato |
| Boot/EFI | `ipxe/*`, installers EFI `install.ps1/install.sh`, testes EFI/boot e guia boot | T7; não edita installers install-agent nem partições |

Quando Boot/EFI terminar, liberar o slot para revisão independente de isolamento, pareamento e persistência. O principal faz firmware Sinric de T8; coordena interface com o responsável da dashboard. Workers não estão sozinhos no repo: não reverter mudanças alheias, usar arquivos atribuídos e comunicar alterações de contrato antes de editar. Commits por tarefa; integração em ordem das dependências.

Para economizar tokens, reservar modelos leves a tarefas delimitadas de UI, installers, documentação e revisão de testes. Core de autenticação, isolamento e NVS exige revisão cuidadosa; modelo leve não deve ser a única validação. Escolher modelos disponíveis no início da execução, sem custo paralelo durante esta etapa de planejamento.

## Gate de conclusão

Matriz preenchida e evidências de software/hardware. Testes futuros ainda não executados ficam marcados como planejados. O plano reduz riscos e regressões; não promete ausência absoluta de bugs.
