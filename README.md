# ESP32 Remote Boot V2

Escolha uma entrada UEFI pela dashboard ou por um Switch Sinric Pro e ligue o PC por Wake-on-LAN. A seleção usa `Boot####`, sem nomes de sistemas fixos.

**Experimental: builds e testes de software não equivalem a validação da V2 na placa/PC.** Consulte `IMPLEMENTATION_REPORT.md` e `docs/hardware-test.md` antes de instalar.

O fluxo preservado é ESP32 → WoL → iPXE local → `/boot.ipxe` → `RemoteBoot.efi` embutido → loader EFI local. A UKI é opcional. O PC precisa de Ethernet com WoL; a ESP32 usa Wi-Fi na mesma rede/broadcast.

## Primeiro uso

1. Instale Python 3 e PlatformIO Core 6.1.18: `python -m pip install platformio==6.1.18`.
2. Extraia o repositório e execute na raiz:

   ```bash
   pio run -e esp32c3_4mb
   pio run -e esp32c3_4mb -t upload
   pio device monitor -b 115200
   ```

3. Conecte ao AP `RemoteBoot-XXXX`. A senha aleatória aparece no monitor serial; ela também é o token do primeiro acesso.
4. Abra `http://192.168.4.1`, conecte usando esse token e configure SSID, senha Wi-Fi, MAC Ethernet e dois tokens **diferentes**, com 24–128 caracteres ASCII de `A–Z`, `a–z`, `0–9`, `_`, `-`. Gere-os com `python -c "import secrets; print(secrets.token_hex(24))"`. Guarde-os.
5. Salve, reconecte à LAN e abra o IP da ESP32 com o token administrativo. Reserve o IP no DHCP. O endereço é embutido no build iPXE e não deve mudar.
6. Configure UEFI/WoL seguindo `docs/bios.md`, `docs/linux-wol.md` e `docs/windows-wol.md`.
7. Execute o installer do host. Configure sistema padrão/fallback na dashboard após a primeira sincronização e antes do teste.

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

O installer mostra a ESP e faz backup antes de escrever. Cria a entrada com `--create-only`, preservando BootOrder. A opção `TEST` agenda um único boot; não reinicia o PC. A opção `AGENT` instala heartbeat e, se habilitado explicitamente, reboot remoto confirmado pela dashboard.

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

## Uso

- Dashboard: botões de boot, visibilidade/ordem das entradas, rede, WoL, padrão, fallback, comportamento do botão físico do **PC**, TTL e Sinric.
- PC online: boot comum retorna `409 PC_ALREADY_ON`. “Forçar WoL” só envia o pacote; não reinicia.
- “Reiniciar neste sistema” exige confirmação e agent habilitado. O agent agenda diretamente `BootNext` para o target.
- Heartbeat a cada 12 segundos; offline após 45 segundos sem heartbeat.
- Sinric: configure até 8 Switch IDs reais no portal e mapeie cada um para um Boot ID ou `default`. A dashboard permanece a interface completa.

## Build e testes

```bash
bash tests/run.sh
pio run -e esp32c3_4mb
make -C uefi/remote-boot
bash ipxe/build.sh 192.0.2.10
```

`192.0.2.10` é endereço de documentação para validar build, **não** um endereço funcional de instalação. APP: `0x3F0000` bytes, sem OTA ou filesystem de UI. O size gate falha acima de 90% dessa partição.

Em Linux que usa ptrace e impede LeakSanitizer: `ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh` mantém AddressSanitizer/UBSan, mas não valida leaks.

Os workflows estão em `.github/workflows`. Não foi feito push/publicação. Para subir manualmente, revise arquivos e execute `git init`, `git add .`, `git commit -m "Initial public release"`.

## Documentação

- [Instalação e recuperação](docs/setup.md)
- [Arquitetura e limites](docs/architecture.md)
- [API](docs/api.md)
- [Dashboard](docs/dashboard.md)
- [Descoberta UEFI](docs/uefi-discovery.md)
- [Sinric](docs/sinric.md)
- [UKI opcional](docs/uki.md)
- [Teste em hardware](docs/hardware-test.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Segurança](SECURITY.md)

O snapshot privado, suas UUIDs, endereços, credenciais e UKI não fazem parte deste repositório.
