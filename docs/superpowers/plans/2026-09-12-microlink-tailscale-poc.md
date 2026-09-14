# MicroLink + Tailscale PoC Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Disponibilizar a dashboard do ESP32-C3 pela tailnet usando MicroLink, sem hardware adicional e sem regressão no firmware padrão.

**Architecture:** Uma segunda environment PlatformIO usa Arduino como componente do ESP-IDF. MicroLink/WireGuard ficam vendorizados e adaptados para single-core/SRAM interna; um runtime isolado aplica a política fail-open para a LAN e publica diagnóstico na API/UI.

**Tech Stack:** PlatformIO, pioarduino, Arduino-ESP32, ESP-IDF 5.5.5, MicroLink, wireguard_lwip, C++17, Python, Vanilla JS.

**Spec:** `docs/superpowers/specs/2026-09-12-microlink-tailscale-poc-design.md`

## Global Constraints

- Preservar `env:esp32c3_4mb` e o fluxo UEFI/iPXE existente.
- Usar MicroLink `216da3300f0493b0860247d43f7af5ce29df63a5`.
- Não versionar auth key; não expor segredo em serial/API/UI.
- Não depender de PSRAM nem fixar tarefas no core 1 do ESP32-C3.
- O acesso LAN deve continuar operante quando MicroLink estiver indisponível.
- PlatformIO Core mínimo para o ambiente híbrido: 6.1.19.

---

### Task 1: Build híbrido isolado

**Files:**
- Modify: `platformio.ini`
- Create: `sdkconfig.defaults`
- Test: `tests/test_microlink_build_config.py`

**Interfaces:**
- Produces: `env:esp32c3_4mb_microlink` com `REMOTE_BOOT_ENABLE_MICROLINK=1`.

- [ ] Escrever teste que carrega o INI com `configparser`, valida os dois ambientes e garante que o padrão não recebe a macro MicroLink.
- [ ] Rodar o teste e observar falha pela ausência do ambiente.
- [ ] Adicionar o ambiente híbrido fixando pioarduino/ESP-IDF e defaults de memória do MicroLink.
- [ ] Rodar o teste e o baseline; confirmar sucesso.
- [ ] Commitar `build: add hybrid microlink environment`.

### Task 2: MicroLink adaptado ao ESP32-C3

**Files:**
- Create: `components/microlink/**`
- Create: `components/wireguard_lwip/**`
- Modify: `THIRD_PARTY.md`
- Test: `tests/test_microlink_vendor.py`

**Interfaces:**
- Consumes: componentes ESP-IDF da Task 1.
- Produces: API pública `microlink.h` compilável no C3.

- [ ] Escrever teste comportamental que compila um probe das macros de afinidade/limites para `SOC_CPU_CORES_NUM=1` e valida a árvore/licenças.
- [ ] Rodar e observar falha porque os componentes não existem.
- [ ] Copiar o commit fixo, mover WireGuard para componente irmão e manter licenças.
- [ ] Aplicar afinidade `tskNO_AFFINITY`, frame Noise de 24 KiB, MTU 1280 e entrega por `netif->input`.
- [ ] Rodar o teste e baseline; confirmar sucesso.
- [ ] Commitar `feat: vendor microlink for esp32c3`.

### Task 3: Credenciais locais e política de start

**Files:**
- Create: `scripts/embed_microlink_config.py`
- Create: `config.microlink.example.json`
- Create: `firmware/include/microlink_policy.hpp`
- Modify: `.gitignore`, `tests/run.sh`
- Test: `tests/test_embed_microlink_config.py`, `tests/test_microlink_policy.cpp`

**Interfaces:**
- Produces: macros `REMOTE_BOOT_MICROLINK_CONFIGURED`, `REMOTE_BOOT_MICROLINK_AUTH_KEY`, `REMOTE_BOOT_MICROLINK_DEVICE_NAME` e `microlinkDecision(...)`.

- [ ] Escrever testes para arquivo ausente, JSON inválido, key inválida, escaping C e matriz de decisões.
- [ ] Rodar e observar falhas pelas implementações ausentes.
- [ ] Implementar gerador sem imprimir segredo e política pura Disabled/NotConfigured/ConfigLocked/SetupMode/WifiOffline/Ready.
- [ ] Rodar testes específicos e baseline.
- [ ] Commitar `feat: add safe microlink configuration`.

### Task 4: Runtime, API e dashboard

**Files:**
- Create: `firmware/include/microlink_runtime.hpp`, `firmware/src/microlink_runtime.cpp`
- Modify: `firmware/src/main.cpp`, `firmware/web/index.html`, `firmware/web/app.js`, `firmware/web/app.css`
- Test: `tests/test_microlink_runtime.cpp`, `tests/test_microlink_dashboard.js`

**Interfaces:**
- Produces: `MicrolinkRuntime::begin`, `MicrolinkRuntime::tick`, `MicrolinkRuntime::snapshot` e objeto JSON `tailscale`.

- [ ] Escrever teste do runtime com adapter fake para provar start tardio no Wi-Fi, tentativa única e falha sem afetar estado local.
- [ ] Escrever teste DOM/JS para estados disabled, starting, connected e error.
- [ ] Rodar e observar falhas esperadas.
- [ ] Implementar runtime condicional e telemetria de heap.
- [ ] Integrar API e quinto card responsivo sem campo de auth key.
- [ ] Regenerar asset web e rodar testes específicos/baseline.
- [ ] Commitar `feat: expose tailscale dashboard over microlink`.

### Task 5: Documentação e verificação real

**Files:**
- Modify: `README.md`, `CHANGELOG.md`, `docs/api.md`
- Create: `docs/microlink-tailscale.md`

**Interfaces:**
- Documents: comandos `pipx upgrade platformio`, configuração local, build, upload, monitor e riscos.

- [ ] Documentar PlatformIO Core >=6.1.19, criação da chave, upload e acesso pelo IP Tailscale.
- [ ] Documentar SRAM, tailnet pequena, falta de OTA e extração de chave da flash.
- [ ] Rodar `ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh`.
- [ ] Rodar `pio run -e esp32c3_4mb` e registrar RAM/flash.
- [ ] Rodar `pio run -e esp32c3_4mb_microlink` sem credencial real e registrar RAM/flash.
- [ ] Inspecionar `git diff --check`, secrets e status.
- [ ] Commitar `docs: document microlink tailscale poc`.
- [ ] Publicar a branch no remote autorizado.
