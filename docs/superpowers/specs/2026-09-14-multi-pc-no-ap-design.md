# Multi-PC sem SoftAP — Design

## Objetivo

Permitir que uma única ESP32-C3 controle até quatro computadores, mantendo Wake-on-LAN disponível sem agent e habilitando catálogo UEFI, boot selecionado, reboot, shutdown e presença apenas nos computadores que possuírem o Remote Boot Agent. Remover integralmente o SoftAP/captive portal e manter a reconexão Wi-Fi automática pela configuração persistida ou pelas credenciais locais embutidas no build.

Base: `feat/microlink-tailscale-poc` no commit `958078f`.

## Decisões

- Schema NVS sobe de 2 para 3.
- `computers` é um array com no máximo quatro objetos.
- Cada computador possui estado de boot, fila WoL, presença, catálogo e sessão do agent independentes.
- O token do agent identifica o computador; não se adiciona um segundo identificador obrigatório ao instalador.
- Um computador sem token/agent continua válido e oferece Wake-on-LAN.
- Configuração administrativa, Wi-Fi, Sinric e MicroLink permanecem globais.
- MAC identifica o computador no fluxo público de iPXE. MAC não é credencial nem substitui autenticação administrativa.
- Um único agent autenticado por computador é aceito, com até quatro sessões simultâneas.

## Modelo de configuração

Campos globais continuam no objeto raiz: `config_version`, Wi-Fi, IP estático/DHCP, `admin_token`, credenciais Sinric e os campos específicos do build MicroLink.

```json
{
  "config_version": 3,
  "computers": [
    {
      "id": "pc-a1b2c3",
      "name": "Desktop",
      "mac": "AA:BB:CC:DD:EE:FF",
      "agent_token": "",
      "wol_port": 9,
      "wol_repeat": 3,
      "wol_interval_ms": 100,
      "pending_ttl_s": 180,
      "physical_boot_behavior": "exit_to_firmware",
      "default_target": "",
      "fallback_boot_id": "",
      "systems": []
    }
  ],
  "sinric_slots": [
    {
      "device_id": "SINRIC_DEVICE_ID",
      "computer_id": "pc-a1b2c3",
      "action": "wake",
      "boot_id": ""
    }
  ]
}
```

### Identificadores

- `computer.id`: ASCII estável, único, gerado pela ESP32 na criação/migração e imutável pela edição comum.
- `computer.mac`: único depois de normalização; usado por WoL e para resolver `/boot.ipxe`.
- `agent_token`: opcional, secreto e único entre os computadores configurados. Token vazio desabilita integração com agent para aquele computador.
- `systems[].id`: único somente dentro do computador ao qual pertence.

### Limites

- Quatro computadores.
- 24 entradas UEFI por computador.
- Oito slots Sinric globais.
- Uma conexão agent por computador.
- Um comando de energia pendente por computador.

## Migração schema 2 → 3

A migração cria um único item em `computers` e move para ele, sem alterar os valores:

- `pc_name` → `name`
- `mac`
- `agent_token`
- `wol_port`, `wol_repeat`, `wol_interval_ms`
- `pending_ttl_s`, `physical_boot_behavior`
- `default_target`, `fallback_boot_id`
- `systems`

Cada slot Sinric antigo recebe o `computer_id` migrado. `boot_id:"default"` e `boot_id:"shutdown"` são convertidos para ações explícitas. A configuração antiga só é persistida depois que o documento schema 3 completo passar pela validação. Schema futuro ou migração inválida permanece bloqueado e preservado.

O token atual continua apontando para o primeiro computador; portanto, o agent já instalado não precisa ser reconfigurado após o primeiro update.

## Estado de execução

O firmware mantém um runtime por computador:

- `rb::State` de pending/default/última escolha;
- agenda e cooldown de WoL;
- presença, hostname, OS e boot atual;
- socket WebSocket ou sessão HTTP legado;
- geração de descoberta;
- comando reboot/shutdown e ACK.

Uma ação em um computador nunca consulta nem altera o runtime de outro. Remover um computador encerra sua sessão, descarta filas RAM e remove slots Sinric associados antes de persistir.

## Identificação no iPXE

O template novo consulta diretamente:

```ipxe
imgfetch --name selection --timeout TIMEOUT http://ESP/boot.ipxe?mac=${net0/mac}
```

Para instalações existentes, `GET /boot.ipxe` sem `mac` retorna um script intermediário:

```ipxe
#!ipxe
chain http://ESP/boot.ipxe?mac=${net0/mac}
```

Isso mantém o EFI/iPXE atual funcional sem reinstalação imediata. `GET /boot.ipxe?mac=...` normaliza o valor, localiza exatamente um computador e gera o script usando apenas o estado desse computador. MAC ausente no segundo estágio, inválido ou desconhecido retorna um script seguro que executa `exit 1`; nunca usa o primeiro computador como fallback.

O MAC é informação pública da LAN e pode ser falsificado. Ele serve apenas para roteamento do pedido de boot; a criação do pending continua restrita à API administrativa/Sinric autenticado.

## API

As rotas administrativas que atuam em um PC passam a exigir `computer_id`:

| Rota | Seleção |
|---|---|
| `POST /api/v1/boot` | body `computer_id` |
| `POST /api/v1/reboot` | body `computer_id` |
| `POST /api/v1/shutdown` | body `computer_id` |
| `POST /api/v1/discovery/request` | body `computer_id` |

`GET /api/v1/status` devolve `computers[]`, cada um com capacidades, presença, catálogo/boot e fila próprios. `GET /api/v1/config` devolve `computers[]` com `agent_token_set`, nunca o token.

As rotas do agent (`heartbeat` e `systems/sync`) resolvem o computador pelo Bearer token. O admin pode usá-las somente informando `computer_id`. WebSocket resolve o computador durante `hello` pelo token. Token ausente, duplicado ou sem correspondência é rejeitado.

Durante uma versão de compatibilidade, payload administrativo sem `computer_id` é aceito apenas quando existe exatamente um computador. Com dois ou mais, retorna `COMPUTER_ID_REQUIRED`. Isso evita ações no PC errado e permite atualizar dashboard e firmware sem janela de quebra no setup atual.

## Sinric Pro

Cada slot possui `computer_id` e ação explícita:

- `wake`: WoL sem seleção de sistema;
- `boot`: arma `boot_id` e envia WoL;
- `shutdown`: exige agent online e permissão de shutdown;
- `reboot`: exige agent online e permissão de reboot; `boot_id` pode ser informado.

`ON` dispara a ação. `OFF` continua sem efeito. Slots que apontem para computador removido ou boot inexistente são rejeitados/retirados pela mesma transação de configuração. O status online do dispositivo Sinric não será tratado como prova de que o agent ou PC está online.

## Dashboard

- Visão geral lista até quatro cards de computadores.
- Cada card mostra WoL sempre disponível.
- Status, catálogo, reboot e shutdown aparecem somente quando a capacidade correspondente existe.
- Seletor ativo controla as páginas Boot e Configuração do computador.
- Formulário permite adicionar, editar e remover computador, respeitando o máximo de quatro.
- A criação oferece gerar/copiar um token de agent, mas não obriga o uso.
- Remoção exige confirmação pelo nome e informa que slots Sinric associados serão removidos.
- Requisições e polling carregam somente dados necessários; nenhum polling separado por card.

## Remoção do SoftAP

Serão removidos:

- `DNSServer` e captive portal;
- `setupAP()` e toda chamada a `WiFi.softAP*`;
- `SetupNetworkPolicy` e seus testes;
- modo `WIFI_AP_STA` e fallback `192.168.4.1`.

Boot normal usa `WIFI_STA`, `WiFi.setAutoReconnect(true)` e credenciais NVS ou `config.local.json`. Se o roteador ainda estiver inicializando, a ESP32 permanece offline e continua tentando reconectar; não transforma a falha temporária em modo de configuração.

Em primeiro uso sem credenciais válidas, a ESP32 permanece offline e imprime diagnóstico no serial. Isso é consequência intencional da remoção do AP: não haverá configuração inicial wireless. Com SSID/senha embutidos, ela entra na LAN e o primeiro acesso administrativo continua pelo token exibido no serial.

## Compatibilidade e segurança

- Build padrão e MicroLink usam o mesmo schema e funcionalidades.
- MicroLink permanece opcional e não participa da decisão de qual computador controlar.
- A dashboard continua protegida por Bearer token na LAN e tailnet.
- Tokens não aparecem em status, config sanitizada ou logs.
- Reboot da ESP32 descarta pending e comandos de todos os computadores.
- Nenhum target é escolhido implicitamente quando a identidade é ambígua.

## Testes obrigatórios

- Migração schema 2 → 3 preservando todos os campos e slots.
- Rejeição de schema futuro, IDs/MACs/tokens duplicados e quinto computador.
- Redação de todos os tokens por computador.
- Isolamento de pending, WoL, catálogo, presença e comandos entre dois computadores.
- `/boot.ipxe` legado encadeando com MAC; MAC válido, inválido e desconhecido.
- Compatibilidade de API com um PC e exigência de `computer_id` com múltiplos.
- Agent WebSocket e HTTP autenticando no computador correto; sessões simultâneas.
- Sinric roteando ação para o computador correto e rejeitando referências órfãs.
- Reconexão Wi-Fi sem criação de AP.
- Dashboard desktop/mobile e payload/status com quatro computadores.
- Builds `esp32c3_4mb` e `esp32c3_4mb_microlink`, testes nativos, scripts/shellcheck e verificação de secrets.

## Fora de escopo

- Controlar mais de quatro computadores.
- Descobrir automaticamente MAC ou instalar agent remotamente.
- Usar MAC como autenticação.
- Fazer proxy/subnet routing entre a tailnet e outros PCs.
- Manter o captive portal como opção oculta.

## Referências

- [iPXE MAC setting e passagem em URL](https://ipxe.org/cfg/mac)
- [iPXE chain](https://ipxe.org/cmd/chain)
