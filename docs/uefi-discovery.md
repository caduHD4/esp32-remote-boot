# Descoberta

Linux usa `efibootmgr -v`, ordena pelo BootOrder e deduplica IDs. Descrições de saída usuais são separadas por tab do Device Path. Paths mais comuns também têm tratamento de fallback na leitura textual; descrições exóticas devem ser conferidas na dashboard.

Windows usa `GetFirmwareEnvironmentVariableExW` / `SetFirmwareEnvironmentVariableExW` com `SeSystemEnvironmentPrivilege`. O catálogo padrão lê BootOrder; `-FullScan` no installer enumera o espaço Boot0000–BootFFFF incluindo entradas fora da ordem, podendo levar mais tempo. O agent usa BootOrder para o ciclo normal. Entradas malformadas são rejeitadas e relatadas, não corrigidas silenciosamente.

Entradas Remote Boot/iPXE e inativas são bloqueadas; Network/IPv4/IPv6/USB/DVD ocultas por heurística. O app EFI repete o bloqueio por descrição/caminho e usa marcador em protocolo para recursão na mesma sessão.

O installer EFI exige um agent pareado com `protocol: 2`, `pc_id` e URL da ESP (`%ProgramData%\RemoteBoot\agent.json` no Windows, `/etc/remote-boot/agent.json` no Linux por padrão). Gere o iPXE com `bash ipxe/build.sh ESP_IPV4 PC_ID [output-directory] [timeout-ms]`. O `manifest.json` registra ESP, PC e SHA256 da imagem iPXE e do loader; mantenha-o junto de `ipxe.efi` e `RemoteBoot.efi`. Antes de gravar, os installers comparam identidade, ESP e hashes. Catálogo é sincronizado pelo agent nativo autenticado; instalar EFI não usa token administrativo nem altera a associação do agent.

Cada imagem busca `http://ESP/boot/PC_ID.ipxe`, sem fallback para um PC implícito. Se o PC não existir mais, a rota deve encerrar com segurança sem consumir seleções de outro PC. BootOrder e os backups existentes continuam preservados; parear ou reinstalar agent não grava EFI.

Boot#### é entrada de loader: escolher GRUB não escolhe automaticamente uma distro no menu GRUB. Se quiser boot Linux direto, configure o loader local ou uma UKI opcional. Device Path completo e HD() são tratados pelo executor; demais short forms/múltiplas instâncias seguem experimentais, sem expansão completa.

Fontes: [UEFI load options](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html), [GetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfirmwareenvironmentvariableexw), [SetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfirmwareenvironmentvariableexw), [efibootmgr upstream](https://github.com/rhboot/efibootmgr).
