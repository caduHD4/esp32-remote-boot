# MicroLink + Tailscale no ESP32-C3

Esta branch contém uma variante experimental que coloca o próprio ESP32-C3 na tailnet. Não exige Raspberry Pi, roteador compatível nem outro hardware. O firmware padrão continua disponível e não contém MicroLink.

## Antes de começar

- Use exatamente o PlatformIO Core 6.1.19. A versão 6.2.0 tem uma incompatibilidade conhecida com o SCons usado pelo PIOArduino. No CachyOS com instalação via `pipx`: `pipx install --force platformio==6.1.19`.
- O computador ou celular que acessará a dashboard precisa estar conectado à mesma tailnet.
- Crie uma auth key em **Tailscale Admin Console → Settings → Keys**. Para o primeiro teste, use chave de uso único e não marque o dispositivo como ephemeral. Tags são opcionais e devem respeitar a política da sua tailnet.
- Este é um POC para placa sem PSRAM. Comece com uma tailnet pequena e observe a memória no card/API.

## Configuração pela dashboard

1. Grave a variante `esp32c3_4mb_microlink`.
2. Entre na dashboard pelo IP LAN.
3. Em **Configuração → Tailscale**, informe a Auth Key e, opcionalmente, o nome do dispositivo.
4. Salve. A ESP reinicia e tenta registrar na tailnet.

A chave fica na NVS e nunca é retornada pela API; a dashboard informa apenas se ela está configurada. Não coloque a chave em `platformio.ini`, arquivos de configuração, commits, screenshots ou logs.

## Build, gravação e monitor no CachyOS

```bash
pipx install --force platformio==6.1.19
pio --version
pio run -e esp32c3_4mb_microlink
pio run -e esp32c3_4mb_microlink -t upload
pio device monitor -b 115200
```

O comando antigo `pio run -e esp32c3_4mb -t upload` continua válido, mas grava o firmware padrão **sem Tailscale**.

Depois que o monitor indicar conexão, abra primeiro a dashboard pelo IP LAN e confira o card **Tailscale**. Quando aparecer `CONECTADO`, use o IP `100.x.y.z` mostrado no card:

```text
http://100.x.y.z/
```

A dashboard continua exigindo o token administrativo. O IP Tailscale não é um endereço público da Internet: ele só funciona a partir de dispositivos autorizados na tailnet. MagicDNS não é obrigatório neste POC; prefira inicialmente o IP exibido.

## Estados e diagnóstico

| Estado no card | Significado | Ação |
|---|---|---|
| `DESATIVADO` | Foi gravado o ambiente padrão | Grave `esp32c3_4mb_microlink` |
| `NÃO CONFIGURADO` | Chave ainda não cadastrada | Informe a Auth Key na dashboard e salve |
| `AGUARDANDO WI-FI` | A LAN ainda não conectou | Corrija o Wi-Fi em `config.local.json` e grave novamente |
| `REGISTRANDO` | Autenticação/registro em andamento | Aguarde e confira a máquina no console Tailscale |
| `CONECTADO` | Interface Tailscale pronta | Abra o IP `100.x.y.z` com o token admin |
| `ERRO` | MicroLink falhou ao iniciar | Use a dashboard LAN; reinicie e confira os logs |

`GET /api/v1/status` também informa `tailscale.heap_free`, `heap_minimum` e `largest_block`. Durante o teste, evite tailnets grandes. Reinícios, desconexões frequentes ou `largest_block` muito baixo indicam pressão/fragmentação de SRAM.

## Limites e segurança

- O ESP32-C3 é single-core e esta placa não tem PSRAM. O port limita peers e buffers, mas estabilidade precisa ser confirmada no hardware real.
- Não há OTA nem rollback automático. Tenha o cabo USB disponível.
- A auth key fica na NVS e pode ser extraída da flash se Secure Boot e Flash Encryption não estiverem habilitados. Use chave de uso único e revogue-a após o registro.
- A integração não cria Funnel, exit node ou subnet router e não publica a dashboard na Internet aberta.
- Falha do MicroLink é `fail-open` apenas para a LAN: dashboard, Sinric, agent e boot local continuam independentes.

## Voltar imediatamente ao firmware estável

Sem apagar a NVS:

```bash
pio run -e esp32c3_4mb -t upload
pio device monitor -b 115200
```

Não use `erase-flash` se quiser preservar a configuração existente. A troca de variante não modifica `boot.ipxe`, `RemoteBoot.efi`, `BootNext` ou o `BootOrder` do PC.

Referências: [auth keys do Tailscale](https://tailscale.com/kb/1085/auth-keys/) e [endereços Tailscale 100.x](https://tailscale.com/docs/concepts/tailscale-ip-addresses).

## Inicialização e resposta da dashboard

O registro inicial da Tailscale começa antes de novas conexões TLS do Sinric e DERP,
para que seu workspace contíguo caiba na memória da ESP32-C3. Enquanto esse
registro está pendente, a nova prioridade adia essas novas conexões por até20s;
depois volta à política normal de admissão. Sessões conectadas e upgrades já
em andamento continuam recebendo serviço. O workspace temporário é reservado
antes de DNS/sockets e liberado no fim do mapa inicial, nas falhas e ao encerrar.
As credenciais não são alteradas.

O envio do HTML não escreve diagnósticos na USB: uma saída serial sem leitor
pode bloquear o atendimento da consulta seguinte por segundos. Falhas de envio
continuam no log de eventos. Para repetir o teste na placa, feche o monitor:

```sh
python3 tests/hardware_http_latency.py http://IP_DA_ESP
```

Esse teste só lê a dashboard e o estado de configuração, sem senha ou mudanças.
Ele mede a pausa entre receber o HTML e concluir a consulta seguinte; não mede
sozinho o tempo total de abertura pelo 4G.

Esta implantação no Brasil prefere o relay de São Paulo (região11). O componente
continua com padrão compatível Dallas; o projeto seleciona São Paulo no
`sdkconfig.defaults`. Para outro local, configure `CONFIG_ML_PREFERRED_DERP_REGION`
e `CONFIG_ML_FALLBACK_DERP_HOST` com região/hostname correspondentes do DERPMap
público atual. Builds já configurados também precisam atualizar essas opções
no sdkconfig. A preferência não muda a identidade nem impede UDP direto.

Ao alternar entre Wi-Fi e rede móvel, respostas WireGuard seguem o último
caminho autenticado daquele peer. Um endereço UDP antigo é mantido para probes,
mas não substitui o relay usado pela solicitação. Probes diretos não mudam essa
preferência; tráfego direto autenticado permite retomar o caminho direto.
