# Descoberta

Linux usa `efibootmgr -v`, ordena pelo BootOrder e deduplica IDs. Descrições de saída usuais são separadas por tab do Device Path. Paths mais comuns também têm tratamento de fallback na leitura textual; descrições exóticas devem ser conferidas na dashboard.

Windows usa `GetFirmwareEnvironmentVariableExW` / `SetFirmwareEnvironmentVariableExW` com `SeSystemEnvironmentPrivilege`. O catálogo padrão lê BootOrder; `-FullScan` no installer enumera o espaço Boot0000–BootFFFF incluindo entradas fora da ordem, podendo levar mais tempo. O agent usa BootOrder para o ciclo normal. Entradas malformadas são rejeitadas e relatadas, não corrigidas silenciosamente.

Entradas Remote Boot/iPXE e inativas são bloqueadas; Network/IPv4/IPv6/USB/DVD ocultas por heurística. O app EFI repete o bloqueio por descrição/caminho e usa marcador em protocolo para recursão na mesma sessão.

Boot#### é entrada de loader: escolher GRUB não escolhe automaticamente uma distro no menu GRUB. Se quiser boot Linux direto, configure o loader local ou uma UKI opcional. Device Path completo e HD() são tratados pelo executor; demais short forms/múltiplas instâncias seguem experimentais, sem expansão completa.

Fontes: [UEFI load options](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html), [GetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfirmwareenvironmentvariableexw), [SetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfirmwareenvironmentvariableexw), [efibootmgr upstream](https://github.com/rhboot/efibootmgr).
