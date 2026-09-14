# MicroLink + Tailscale no ESP32-C3

Esta branch contém uma variante experimental que coloca o próprio ESP32-C3 na tailnet. Não exige Raspberry Pi, roteador compatível nem outro hardware. O firmware padrão continua disponível e não contém MicroLink.

## Antes de começar

- Use exatamente o PlatformIO Core 6.1.19. A versão 6.2.0 tem uma incompatibilidade conhecida com o SCons usado pelo PIOArduino. No CachyOS com instalação via `pipx`: `pipx install --force platformio==6.1.19`.
- O computador ou celular que acessará a dashboard precisa estar conectado à mesma tailnet.
- Crie uma auth key em **Tailscale Admin Console → Settings → Keys**. Para o primeiro teste, use chave de uso único e não marque o dispositivo como ephemeral. Tags são opcionais e devem respeitar a política da sua tailnet.
- Este é um POC para placa sem PSRAM. Comece com uma tailnet pequena e observe a memória no card/API.

## Configuração pela dashboard

Grave primeiro a variante MicroLink sem incluir qualquer chave no código. No primeiro acesso, o wizard oferece a etapa Tailscale. Cole a Auth Key e escolha o nome do dispositivo; os valores são persistidos na NVS e aplicados após o reinício final.

Em uma instalação já configurada, altere a integração pela dashboard autenticada. A API nunca devolve a Auth Key, apenas informa se ela está configurada.

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

A dashboard continua exigindo a senha administrativa. O IP Tailscale não é um endereço público da Internet: ele só funciona a partir de dispositivos autorizados na tailnet. MagicDNS não é obrigatório neste POC; prefira inicialmente o IP exibido.

## Estados e diagnóstico

| Estado no card | Significado | Ação |
|---|---|---|
| `DESATIVADO` | Foi gravado o ambiente padrão | Grave `esp32c3_4mb_microlink` |
| `NÃO CONFIGURADO` | Build experimental sem arquivo/chave | Configure a Auth Key pela dashboard |
| `AGUARDANDO WI-FI` | A LAN ainda não conectou | Corrija o Wi-Fi na configuração local |
| `REGISTRANDO` | Autenticação/registro em andamento | Aguarde e confira a máquina no console Tailscale |
| `CONECTADO` | Interface Tailscale pronta | Abra o IP `100.x.y.z` com o token admin |
| `ERRO` | MicroLink falhou ao iniciar | Use a dashboard LAN; reinicie e confira os logs |

`GET /api/v1/status` também informa `tailscale.heap_free`, `heap_minimum` e `largest_block`. Durante o teste, evite tailnets grandes. Reinícios, desconexões frequentes ou `largest_block` muito baixo indicam pressão/fragmentação de SRAM.

## Limites e segurança

- O ESP32-C3 é single-core e esta placa não tem PSRAM. O port limita peers e buffers, mas estabilidade precisa ser confirmada no hardware real.
- Não há OTA nem rollback automático. Tenha o cabo USB disponível.
- A Auth Key fica na NVS e pode ser extraída da flash se Secure Boot e Flash Encryption não estiverem habilitados. Use uma chave de uso único ou revogue-a depois do registro.
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
