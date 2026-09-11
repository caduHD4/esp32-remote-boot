# Changelog

## 2.1.3-experimental

- Redesenha a dashboard com navegação responsiva para desktop/mobile, cards de status, ícones SVG locais, feedback de ações e acessibilidade básica.
- Separa HTML, CSS e JavaScript em fontes mantíveis, preservando um único asset gzip/PROGMEM e operação offline.
- Valida credenciais e slots Sinric antes do salvamento, com mensagens por campo e suporte a credenciais já armazenadas.
- Impede defensivamente `SinricPro.begin()` quando a integração está desativada ou sem credenciais mínimas.
- Adiciona os códigos `SINRIC_CREDENTIALS_REQUIRED` e `SINRIC_CONFIG_INCOMPLETE` sem expor segredos.

## 2.1.2-experimental

- Substitui o encadeamento direto de entradas `Boot####` por `BootNext` seguido de reset UEFI, delegando ao firmware o mesmo fluxo validado por `efibootmgr --bootnext`.
- Impede o ESP32 de entregar o mesmo alvo novamente durante 60 segundos, evitando ciclo rápido caso o firmware retorne ao iPXE.
- Mantém a validação de entradas inativas, inválidas e recursivas antes de gravar `BootNext`.

## 2.1.1-experimental

- Corrige spam `WiFiUdp parsePacket(): could not receive data: 9` no setup direto pela LAN.
- Mantém o AP de recuperação após perda do Wi-Fi durante o primeiro acesso.
- Preserva SSID/senha de `config.local.json` ao salvar a configuração pela dashboard.
- Valida limites em bytes UTF-8 e rejeita caracteres de controle nas credenciais compiladas.
- Corrige detecção/reutilização da entrada `Remote Boot iPXE` quando `efibootmgr` exibe o device path.

## 2.0.0-experimental

- Catálogo de 24 entradas Boot####; seleção com TTL, fallback e heartbeat.
- Dashboard Vanilla JS gzip/PROGMEM; NVS, setup AP protegido, Sinric opcional com 8 slots.
- Executor GNU-EFI com parsing limitado, OptionalData e resolução de HD() por assinatura.
- Installers/agents Linux e Windows, preservação de BootOrder e teste por BootNext.
- Build ESP32-C3 4 MB sem OTA, testes nativos e workflows CI/release.

Validação de software não substitui os testes em hardware descritos no relatório.

