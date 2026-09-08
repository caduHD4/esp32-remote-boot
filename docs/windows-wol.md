# WoL Windows

No driver Ethernet, procure Wake on Magic Packet e, se existir, Shutdown Wake-On-Lan. Na aba de energia, habilite permissão para acordar e restrinja a Magic Packet quando disponível. Os nomes/opções variam por NIC e driver.

Desative Fast Startup no fluxo de referência. Microsoft diferencia desligamento híbrido de estados em que WoL é suportado; não prometa que qualquer NIC/firmware acorda de qualquer estado. Verifique `powercfg /devicequery wake_armed`, mas valide também o desligamento real.

Fonte: [Microsoft: Wake on LAN behavior](https://learn.microsoft.com/en-us/troubleshoot/windows-client/setup-upgrade-and-drivers/wake-on-lan-feature).
