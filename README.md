# ESP32 Remote Boot 3 — múltiplos PCs

Controle computadores por dashboard, Wake-on-LAN, UEFI/iPXE e Sinric Pro. Versão **3.0.0-alpha.1**, configuração **v3**, API **v2**.

- Até **4 PCs**, cada um com MAC, catálogo, estado, padrão/fallback, WoL e comandos próprios.
- Até **8 instalações de agent** Windows/Linux e **24 entradas UEFI por PC**. Uma sessão ativa por computador.
- Instalação do agent por **código curto**, sem copiar token longo. Windows e Linux no mesmo computador são pareados ao mesmo PC.
- **Sinric controla apenas um PC escolhido**. Trocar a seleção limpa os Switches; remover esse PC desativa a integração.
- A senha administrativa é criada no primeiro acesso, com os campos Senha e Repetir senha.

Esta versão exige configuração nova: agents/API antigos são rejeitados. NVS: **64 KiB**; APP: **0x20000**, tamanho **0x3E0000**. Ao substituir versão 2, faça backup e erase-flash antes da gravação. Testes de software/ESP não substituem testes físicos de WoL, energia e UEFI nos seus computadores.

## Downloads

Em [Releases](https://github.com/caduHD4/esp32-remote-boot/releases), baixe a versão correspondente:

- `remote-boot-agent-win-x64.zip`
- `remote-boot-agent-linux-x64.zip`
- `agent-SHA256SUMS`

Extraia mantendo as pastas: os ZIPs incluem executável NativeAOT, installer e documentação. Não precisa instalar .NET. Para EFI/iPXE, baixe também **Source code (zip)**. No repositório privado, entre em uma conta com acesso. Artifacts de Actions estão disponíveis para desenvolvimento.

## Tailscale exige a variante MicroLink

**Para conectar a ESP32 ao Tailscale, grave obrigatoriamente `esp32c3_4mb_microlink`.** A variante padrão `esp32c3_4mb` não inclui Tailscale: salvar uma Auth Key nela não habilita a conexão.

Use PlatformIO Core **6.1.19**; 6.2.0 tem incompatibilidade com o SCons usado pelo PIOArduino:

```bash
pipx install --force platformio==6.1.19
pio run -e esp32c3_4mb_microlink
pio run -e esp32c3_4mb_microlink -t upload
```

Depois, pelo IP LAN, preencha a Auth Key em **Configuração → Tailscale** e salve. A ESP reinicia e tenta registrar na tailnet. Confira o card; `DESATIVADO` indica a variante padrão. Novos uploads devem usar a variante MicroLink para manter Tailscale. Consulte [MicroLink/Tailscale](docs/microlink-tailscale.md).

## Primeiro uso

1. Instale Python 3 e PlatformIO Core 6.1.19.
2. Copie `config.local.example.json` para `config.local.json`; preencha SSID/senha de Wi-Fi **2,4 GHz**. O arquivo é ignorado pelo Git e embutido no firmware. Não o compartilhe.
3. Na raiz, compile e grave a variante MicroLink acima, ou `pio run -e esp32c3_4mb -t upload` sem Tailscale.
4. Veja o IP em `pio device monitor -b 115200` (USB atual: `pio device monitor -p COM8 -b 115200`). Reserve o IP no DHCP.
5. Abra o IP, preencha **Senha** e **Repetir senha** e clique **Salvar e reiniciar ESP**. A senha aceita 8–128 caracteres sem controles. Wi-Fi é alterado em `config.local.json`, seguido de novo upload.
6. Entre com sua senha, clique **Adicionar PC** e informe nome e MAC Ethernet.
7. Na área **Agents**, abra a janela de sincronização. Execute o installer no Windows/Linux: ele mostra um código como `ABCD-EFGH`. Digite-o no painel, confira hostname/OS e escolha o PC. O installer salva a credencial automaticamente, verifica hello/catálogo, confirma o pareamento e inicia o serviço.
8. Para Windows e Linux do mesmo computador, pareie ambas as instalações ao **mesmo PC**. Para outro computador, adicione outro PC. Reboot/shutdown exigem autorização local durante instalação.
9. Configure UEFI/WoL, construa iPXE com o ID do PC e configure padrão/fallback. Para Sinric, escolha um único PC, salve a seleção e configure seus Switches. Teste antes de promover Remote Boot no BootOrder.

## Agent Windows e Linux

O cliente principal é **C# NativeAOT + WebSocket persistente**, com keepalive de 60 s e reconexão. Não abre porta no computador. Requer firmware 3 e protocolo 2. O [guia completo](docs/native-agent.md) explica configuração e diagnóstico.

Na raiz do ZIP extraído, com a janela de pareamento aberta no painel:

```powershell
# PowerShell elevado; substitua o IP
.\installer\windows\install-agent.ps1 -EspAddress 'SEU_IP_DA_ESP32'
```

```bash
# Linux UEFI com systemd; substitua o IP
chmod +x build/agent/linux-x64/remote-boot-agent
sudo bash installer/linux/install-agent.sh SEU_IP_DA_ESP32
```

Para usar um executável baixado separadamente, passe `-AgentFile CAMINHO` no Windows ou o caminho como segundo argumento no Linux. Digite `SHUTDOWN` e/ou `REBOOT` para autorizar cada ação. Configurações privadas ficam em `%ProgramData%\RemoteBoot` ou `/etc/remote-boot`; tokens não aparecem no dashboard. Instalar o agent não modifica EFI/BootOrder.

## UEFI/iPXE

Fluxo: Sinric/dashboard → ESP → WoL → iPXE local → `/boot/<pc_id>.ipxe` → `RemoteBoot.efi` embutido → loader local. O PC precisa de Ethernet com WoL e UEFI. Cada imagem iPXE contém o ID do PC; não reutilize imagens entre PCs. Placas que dependem do driver de rede UEFI/SNP, como a RTL8125 na revisão iPXE usada, exigem **Network Stack e IPv4 PXE Support habilitados na BIOS**; veja [BIOS e rede UEFI](docs/bios.md#rede-disponível-no-ipxe).

Dependências para construir em Linux: Git, GNU Make/GCC/binutils, GNU-EFI, Perl e liblzma. Ubuntu/Debian:

```bash
sudo apt install build-essential binutils gnu-efi git perl liblzma-dev jq curl efibootmgr
bash ipxe/build.sh SEU_IP_DA_ESP32 PC_ID
```

O `pc_id` está no config pareado do agent. O build gera `build/PC_ID/ipxe.efi`, `RemoteBoot.efi`, `SHA256SUMS` e `manifest.json`, vinculados à ESP/PC/hashes. Mantenha juntos.

Depois de parear o agent, use `sudo bash installer/linux/install.sh` ou, em PowerShell elevado:

```powershell
.\installer\windows\install.ps1 -EspAddress 'SEU_IP_DA_ESP32' -IpxeFile '.\build\PC_ID\ipxe.efi' -FullScan
```

Os installers verificam config/manifesto/hashes antes de escrever EFI, fazem backup e preservam BootOrder. O build Windows é feito em Linux/WSL; **não execute o installer Linux no WSL para alterar EFI do Windows**. Após teste, use `promote.sh XXXX` ou `promote.ps1 -BootId XXXX`; nunca um ID fixo. Consulte [UEFI](docs/uefi-discovery.md), [BIOS](docs/bios.md), [Windows WoL](docs/windows-wol.md) e [Linux WoL](docs/linux-wol.md).

## Sinric Pro — um PC por ESP

Crie uma app e dispositivos Switch no [portal Sinric](https://portal.sinric.pro). No painel, escolha **um PC** para a integração, salve a seleção e configure App Key/Secret e os Device IDs. Até 8 slots mapeiam entradas do catálogo desse PC, padrão ou desligamento. Alterar PC limpa os slots para evitar que Switches antigos controlem outro computador.

Vincule Sinric ao seu assistente de voz. **ON** solicita a ação; **OFF** não faz nada. Shutdown exige agent online com permissão local. Os demais PCs continuam disponíveis pela dashboard. Veja [passo a passo](docs/sinric.md).

## Testes e limites

```bash
bash tests/run.sh
pio run -e esp32c3_4mb
pio run -e esp32c3_4mb_microlink
npm ci
npx playwright install
npm run test:ui
```

O size gate usa a partição real e falha acima de 90%. A UI é um asset gzip determinístico, sem filesystem/OTA. Configuração é gravada em dois bancos NVS com CRC, leitura de confirmação e seleção atômica por geração. Catálogos são paginados, no máximo 8 entradas por página. Pareamento dura 5 minutos, com limites de solicitações e tentativas.

Testes automáticos de energia usam FakeHost. Testes físicos de dois PCs, perda de energia durante gravação e soak prolongado exigem equipamento. Consulte [matriz](docs/testing/multi-pc-agent-pairing.md), [dashboard](docs/dashboard.md), [API](docs/api.md), [arquitetura](docs/architecture.md), [segurança](SECURITY.md) e [hardware](docs/hardware-test.md).

Credenciais, snapshots privados, UUIDs reais e UKIs não pertencem ao repositório.
