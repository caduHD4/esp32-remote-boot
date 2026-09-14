# Dashboard UI/UX e validação Sinric — Design

**Data:** 2026-09-11  
**Status:** aprovado para planejamento  
**Branch:** `feat/dashboard-ui-sinric-validation`

## Objetivo

Modernizar a dashboard do ESP32 Remote Boot para desktop e mobile, com navegação clara, ícones SVG locais, animações leves, feedback de ações e validação de formulário, sem alterar o fluxo já validado de dual boot:

`Sinric Pro → ESP32 → WoL → iPXE local → /boot.ipxe → RemoteBoot.efi → BootNext → reset UEFI → loader escolhido`.

Também impedir que uma configuração Sinric incompleta provoque tentativa de inicialização do SDK, reinício inútil ou repetição de erros no serial.

## Restrições

- Continuar operando totalmente offline na LAN.
- Não usar framework frontend, CDN, fonte remota ou biblioteca de ícones.
- Continuar entregando um único asset HTML comprimido em gzip e armazenado em PROGMEM.
- Preservar todos os endpoints, nomes de campos, tokens, schema NVS e comportamento de WoL/boot.
- Não revelar credenciais existentes; usar somente os indicadores `*_set`.
- Manter limite de 24 entradas UEFI e 8 slots Sinric.
- Não alterar o `BootOrder`, o fluxo `BootNext` nem o comportamento dos agents.
- Ser utilizável em telas a partir de 320 px.
- Respeitar `prefers-reduced-motion`.

## Arquitetura do frontend

Separar a fonte da dashboard em três arquivos:

- `firmware/web/index.html`: estrutura semântica, regiões de navegação, templates e SVG sprite.
- `firmware/web/app.css`: design system, layout responsivo, estados, animações e acessibilidade.
- `firmware/web/app.js`: API, estado, renderização, validação e ações.

`scripts/embed_web.py` combinará os três arquivos de forma determinística e gerará o mesmo `firmware/include/web_asset.h` gzip usado pelo firmware. Nenhum request adicional será feito pelo navegador.

## Estrutura visual

### Desktop

Uma sidebar fixa dentro do container principal oferece cinco destinos:

1. Visão geral
2. Boot
3. Configuração
4. Sinric
5. Sistema

O conteúdo usa cards e grid responsivo. A visão geral mostra PC, sistema detectado, ESP32/IP, RSSI e Sinric. Boot mostra um card por entrada UEFI válida, com “Ligar” como ação primária e ações secundárias menos destacadas.

### Mobile

A navegação vira uma faixa horizontal sticky no topo. O conteúdo usa uma coluna, sem scroll horizontal. Campos e botões têm área interativa mínima de 44 px. A ação primária ocupa a largura disponível; ações secundárias quebram para novas linhas.

### Linguagem visual

Tema dark tecnológico baseado no visual existente, com fundo azul-escuro, superfícies elevadas, verde/teal para ações seguras, azul para informação, âmbar para avisos e vermelho apenas para ações destrutivas. Ícones são SVG inline com `currentColor`.

Transições ficam entre 120 e 180 ms e se limitam a cor, borda, opacidade e pequenos deslocamentos. Com `prefers-reduced-motion: reduce`, transições e animações são removidas.

## Estados e feedback

- Skeletons são exibidos enquanto configuração/status são carregados.
- Badges distinguem `online`, `offline`, `pendente`, `desabilitado` e `erro`.
- Toasts com `role=status` informam sucesso; erros usam `role=alert`.
- Botões em request ficam desabilitados e mostram estado de processamento.
- Erros de campo aparecem abaixo do input, atualizam `aria-invalid` e movem foco para o primeiro campo inválido.
- Erros conhecidos da API recebem texto em português; o código técnico permanece disponível na mensagem.
- Ações destrutivas permanecem separadas e confirmadas.
- DHCP ativo oculta os campos de IP estático sem apagar seus valores.
- Senhas vazias continuam significando “manter valor existente”.

## Validação Sinric

### Frontend

Ao ativar Sinric:

- App Key é obrigatória, salvo quando `sinric_app_key_set=true`.
- App Secret é obrigatória, salvo quando `sinric_app_secret_set=true`.
- Cada slot presente deve ter Device ID hexadecimal com exatamente 24 caracteres.
- Cada slot presente deve apontar para `default`, `shutdown` ou um Boot ID válido e não bloqueado.
- Device IDs duplicados são rejeitados.
- Slot totalmente vazio é removido antes do envio quando Sinric estiver desativado.
- Sinric ativado sem slots mostra aviso de que nenhum comando será atendido, mas não bloqueia o salvamento para preservar compatibilidade.
- Configuração inválida não é enviada, não reinicia o ESP32 e não chama o SDK.

### Firmware

A validação persistente continua sendo a autoridade final. Uma política Sinric testável, independente do SDK, verificará se a configuração está pronta antes de `SinricPro.begin()`.

Se `sinric_enabled=false`, o SDK não inicia.

Se estiver ativado, mas as credenciais armazenadas forem ausentes ou insuficientes:

- `SinricPro.begin()` não será chamado;
- `sinricStarted` permanecerá `false`;
- um único evento `SINRIC_CONFIG_INCOMPLETE` será registrado;
- o loop não fará tentativas de reconexão;
- o restante da dashboard, WoL e boot continuará funcionando.

A API continuará rejeitando configurações persistentes inválidas. A resposta passa a identificar o motivo Sinric de forma separada de `INVALID_CONFIG`, sem expor credenciais.

## Testes

### Frontend

Um teste Node sem dependências externas validará:

- sintaxe do JavaScript;
- montagem determinística do asset;
- credenciais novas e credenciais já armazenadas;
- rejeição de App Key/App Secret ausentes;
- slots vazios, incompletos e duplicados;
- filtragem de slots vazios quando Sinric estiver desativado;
- presença de viewport, landmarks, labels, `aria-live`, `aria-invalid`, media queries mobile e `prefers-reduced-motion`;
- ausência de URLs/CDNs externas.

### Firmware

Testes C++ nativos validarão a política Sinric:

- desativado nunca inicia;
- ativado sem App Key não inicia;
- ativado sem App Secret não inicia;
- ativado com credenciais mínimas inicia;
- configuração incompleta produz somente uma decisão de erro, sem loop.

### Regressão

Executar:

- `bash tests/run.sh`;
- `pio run -e esp32c3_4mb`;
- gate de tamanho da partição APP;
- inspeção do HTML montado em larguras 320, 768 e 1280 px;
- confirmação de que endpoints e payloads existentes não mudaram.

## Critérios de aceite

- A dashboard funciona sem dependência externa em desktop e mobile.
- Nenhum conteúdo causa overflow horizontal em 320 px.
- Todas as ações existentes permanecem disponíveis.
- Navegação, estados e mensagens são compreensíveis sem consultar o serial.
- Sinric incompleto é impedido antes do save e também antes de `SinricPro.begin()`.
- Configuração Sinric incompleta não gera repetição de erros.
- Build do ESP32 permanece abaixo do gate existente de 90% da APP.
- O fluxo de boot já validado permanece inalterado.
