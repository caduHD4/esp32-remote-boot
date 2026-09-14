# ESP32 Remote Boot V2

Ligue o PC e escolha o próximo sistema por voz com Sinric Pro: um Switch aciona Windows e outro Linux. A ESP32 grava o target, envia Wake-on-LAN e o boot UEFI local carrega a entrada `Boot####` selecionada. Não há nomes de sistemas fixos no firmware.

**Experimental: builds e testes de software não equivalem a validação da V2 na placa/PC.** Consulte `IMPLEMENTATION_REPORT.md` e `docs/hardware-test.md` antes de instalar.

Fluxo principal: Sinric Pro → ESP32 → WoL → iPXE local → `/boot.ipxe` → `RemoteBoot.efi` embutido → loader EFI local. A UKI é opcional. O PC precisa de Ethernet com WoL; a ESP32 usa Wi-Fi na mesma rede/broadcast. Dashboard e agents ampliam o controle, mas não substituem o fluxo Sinric.

## Downloads por versão

Baixe os agents em [Releases](https://github.com/caduHD4/esp32-remote-boot/releases), na seção **Assets** da versão desejada:

- **Windows x64:** `remote-boot-agent-win-x64.exe`.
- **Linux x64:** `remote-boot-agent-linux-x64`.
- **Verificação:** `SHA256SUMS`.

Baixe também **Source code (zip)** da mesma versão para obter os installers. A versão inicial é uma **pré-release experimental**. Como o repositório é privado, é necessário entrar no GitHub com uma conta que tenha acesso.

## Primeiro uso

1. Instale Python 3 e PlatformIO Core 6.1.19.
2. Copie `config.local.example.json` para `config.local.json` e informe o Wi-Fi 2,4 GHz. Sem credenciais válidas a ESP32 permanece offline; não existe mais SoftAP.
3. Grave o firmware e abra o IP exibido no monitor serial:

   ```bash
   pio run -e esp32c3_4mb -t upload
   pio device monitor -b 115200
   ```

4. No primeiro acesso, a dashboard abre automaticamente o setup guiado:
   - crie uma senha administrativa de 1–8 caracteres;
   - aguarde o reinício;
   - cadastre nome e MAC do computador;
   - informe uma senha de agent de até 8 caracteres ou deixe vazia para usar somente Wake-on-LAN;
   - configure boot quando houver agent;
   - configure ou pule Sinric Pro e Tailscale.
5. Finalize o setup e entre com a senha criada. Reserve o IP da ESP32 no DHCP.
6. Configure UEFI/WoL seguindo `docs/bios.md`, `docs/linux-wol.md` e `docs/windows-wol.md`.

Computadores adicionais usam uma senha de agent exclusiva. O agent é opcional para Wake-on-LAN.

## Sinric Pro: passo a passo principal

1. Acesse [portal.sinric.pro](https://portal.sinric.pro), crie uma conta e, em **Apps**, crie uma app, por exemplo `ESP32 Remote Boot`.
2. Na área **Credentials** da app, copie **App Key** e **App Secret**. Nunca publique esses valores.
3. Em **Devices**, crie dois dispositivos do tipo **Switch**: `PC Windows` e `PC Linux` (ou o nome da distribuição). Copie o **Device ID** de cada um.
4. Vincule a conta Sinric Pro ao Alexa ou Google Home pelo fluxo oferecido no portal e execute a descoberta de dispositivos. Os dois Switches devem aparecer no assistente.
5. Na dashboard da ESP32, em **Sinric**, marque **Ativar Sinric**, cole App Key/App Secret e adicione dois slots. Em cada slot, cole um Device ID e selecione o `Boot####` já testado para o sistema correspondente.
6. Salve e aguarde a ESP32 reiniciar. O status deve indicar `Sinric online`.
7. Com o PC desligado, teste `PC Windows` e `PC Linux` separadamente. Cada comando `ON` deve ligar o PC por WoL e iniciar apenas o `Boot####` mapeado.

Não use `default` durante a validação inicial. Se um Switch iniciar o sistema errado, corrija apenas o mapeamento `Device ID → Boot####`; não altere o `BootOrder`. O guia detalhado, com recuperação e diagnóstico, está em [docs/sinric.md](docs/sinric.md).

## Agent nativo C# (Windows e Linux)

O cliente principal usa **C# NativeAOT + WebSocket persistente**. Recebe comandos por evento, sem polling HTTP de 12 s. Não exige .NET instalado; há executáveis separados para Windows x64 e Linux x64. Keepalive de 60 s e reconexão continuam necessários.

Antes de instalar o agent, baixe o executável Windows/Linux em [Releases](https://github.com/caduHD4/esp32-remote-boot/releases), ou compile o código. Os artifacts de Actions continuam disponíveis para builds de desenvolvimento. Atualize a ESP32 para firmware 2.1. O [guia do agent nativo](docs/native-agent.md) mostra build, download, instalação e migração completos.

```bash
# Linux: marque o binário baixado como executável e instale
chmod +x /caminho/remote-boot-agent
sudo bash installer/linux/install-agent.sh SEU_IP_DA_ESP32 /caminho/remote-boot-agent
```

```powershell
# Windows: PowerShell elevado apenas durante a instalação
.\installer\windows\install-agent.ps1 -EspAddress 'SEU_IP_DA_ESP32' -AgentFile 'C:\caminho\remote-boot-agent.exe'
```

Substitua os placeholders e informe o **agent token**. Digite `SHUTDOWN` e/ou `REBOOT` para autorizar cada ação. Linux usa systemd; Windows inicia o `.exe` diretamente pelo Task Scheduler como SYSTEM. O cliente não abre porta no PC. Linux ainda usa bibliotecas nativas do OS e ferramentas `efibootmgr`/`systemctl` quando necessário. Consumo real ainda não foi medido.

## Linux

Dependências básicas: Bash, `jq`, `curl`, `efibootmgr`, `util-linux`, `systemd`. Para construir: Git, GNU Make, GCC, binutils, GNU-EFI, Perl e headers de desenvolvimento usados pelo iPXE.

Ubuntu/Debian:

```bash
sudo apt install build-essential binutils gnu-efi git perl liblzma-dev jq curl efibootmgr
sudo bash installer/linux/install.sh
```

Arch/CachyOS:

```bash
sudo pacman -S --needed base-devel binutils gnu-efi git perl xz jq curl efibootmgr
sudo bash installer/linux/install.sh
```

O installer mostra a ESP e faz backup antes de escrever. Cria a entrada com `--create-only`, preservando BootOrder. A opção `TEST` agenda um único boot; não reinicia o PC. A opção `AGENT` instala o agent nativo WebSocket e, se habilitado explicitamente, reboot remoto confirmado pela dashboard.

Após testar, `sudo bash installer/linux/promote.sh XXXX` pede confirmação para colocar a entrada em primeiro. `XXXX` é o ID mostrado pelo installer, nunca um valor fixo.

## Windows 10/11 x64 UEFI

O installer Windows usa PowerShell e APIs firmware nativas. O build GNU-EFI/iPXE precisa ser feito em Linux, inclusive WSL2, ou em outra máquina Linux:

```bash
bash ipxe/build.sh SEU_IP_DA_ESP32
```

O argumento acima deve ser substituído por IPv4 real. Copie `build/ipxe.efi` e confira `build/SHA256SUMS`.

Em PowerShell elevado, na raiz do repositório:

```powershell
.\installer\windows\install.ps1 -EspAddress 'SEU_IP_DA_ESP32' -IpxeFile '.\build\ipxe.efi' -FullScan
.\installer\windows\install-agent.ps1 -EspAddress 'SEU_IP_DA_ESP32'
```

Não execute o installer Linux no WSL para alterar o firmware do host Windows: use WSL somente para o build. O installer Windows mostra as ESPs, exporta o estado e pede confirmação antes da escrita. Após o teste, use `installer/windows/promote.ps1 -BootId XXXX`. Política de execução corporativa permanece sob controle do administrador; os scripts não a alteram.

## Shutdown remoto (Windows e Linux)

O mesmo agent recebe desligamento pela dashboard e por um Switch Sinric dedicado. Instale/atualize o agent em cada sistema e habilite a permissão `SHUTDOWN`:

```bash
# Linux UEFI com systemd
sudo bash installer/linux/install-agent.sh
```

```powershell
# Windows PowerShell como administrador
.\installer\windows\install-agent.ps1 -EspAddress 'SEU_IP_DA_ESP32'
```

Informe o IP reservado/token do agent e atualize também o firmware ESP32. Na dashboard, use **Desligar PC** e confirme. No Sinric, crie um Switch **Desligar PC**, adicione seu Device ID e mapeie para **Desligar PC (agent)**. Enviar **ON** a esse Switch desliga o OS que estiver rodando; **OFF não faz nada**. Para dizer “desligar computador”, use uma rotina do assistente que acione esse Switch com ON.

O cliente C# recebe comandos por WebSocket, sem consultas periódicas de comandos. A instalação nativa está descrita acima. Shutdown é normal, sem modo forçado; salve o trabalho. Consumo e shutdown físico ainda não foram medidos/testados. Veja [instalação, uso e diagnóstico completos](docs/shutdown.md).

## Uso

- Dashboard: botões de boot, visibilidade/ordem das entradas, rede, WoL, padrão, fallback, comportamento do botão físico do **PC**, TTL e Sinric.
- PC online: boot comum retorna `409 PC_ALREADY_ON`. “Forçar WoL” só envia o pacote; não reinicia.
- “Reiniciar neste sistema” exige confirmação e agent habilitado. O agent agenda diretamente `BootNext` para o target.
- Agent nativo: WebSocket com keepalive de 60 s; queda detectada encerra a sessão. Cliente HTTP legado: intervalo de 12 s e timeout de 45 s, apenas para compatibilidade.
- Sinric: é a integração principal de Wake-on-LAN dual boot. Configure dois Switch IDs reais, um para cada Boot ID; até 8 slots são suportados. A dashboard permanece a interface completa.

## Build e testes

```bash
bash tests/run.sh
pio run -e esp32c3_4mb
make -C uefi/remote-boot
bash ipxe/build.sh 192.0.2.10
```

`192.0.2.10` é endereço de documentação para validar build, **não** um endereço funcional de instalação. APP: `0x3F0000` bytes, sem OTA ou filesystem de UI. O size gate falha acima de 90% dessa partição.

Em Linux que usa ptrace e impede LeakSanitizer: `ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh` mantém AddressSanitizer/UBSan, mas não valida leaks.

### POC MicroLink + Tailscale

A variante `esp32c3_4mb_microlink` permite acessar a dashboard pelo IP Tailscale do próprio ESP32-C3, sem hardware auxiliar. Ela é opt-in; a Auth Key e o nome do dispositivo são informados no setup da dashboard e persistidos na NVS.

```bash
pipx install --force platformio==6.1.19
pio run -e esp32c3_4mb_microlink -t upload
pio device monitor -b 115200
```

Leia [configuração, riscos, diagnóstico e rollback do MicroLink/Tailscale](docs/microlink-tailscale.md) antes de gravar essa variante experimental.

Os workflows estão em `.github/workflows`. Para trabalhar localmente, clone o repositório, revise os arquivos e crie commits normalmente.

## Documentação

- [Instalação e recuperação](docs/setup.md)
- [Arquitetura e limites](docs/architecture.md)
- [API](docs/api.md)
- [Dashboard](docs/dashboard.md)
- [MicroLink + Tailscale experimental](docs/microlink-tailscale.md)
- [Descoberta UEFI](docs/uefi-discovery.md)
- [Sinric Pro: fluxo principal](docs/sinric.md)
- [UKI opcional](docs/uki.md)
- [Teste em hardware](docs/hardware-test.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Segurança](SECURITY.md)

O snapshot privado, suas UUIDs, endereços, credenciais e UKI não fazem parte deste repositório.
