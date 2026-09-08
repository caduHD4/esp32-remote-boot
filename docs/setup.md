# Instalação, atualização e recuperação

Para o cliente residente, siga primeiro [Agent nativo C#](native-agent.md). Os installers atuais exigem o binário NativeAOT e iniciam C# via WebSocket. PowerShell/Bash abaixo são ferramentas de instalação e recuperação, não o processo residente. HTTP heartbeat permanece só para clientes legados.

Siga README para onboarding. A primeira sincronização pode ocorrer em apenas um OS; instale agent em cada OS que deseja identificar/reiniciar. Linux requer systemd; a descoberta/API não depende de Python. `jq` é dependência explícita.

Configure padrão e fallback antes do primeiro BootNext. Nenhum ID é escolhido automaticamente. A opção `exit_to_firmware` deixa a política do boot físico para o firmware e precisa ser testada na máquina.

Installers preservam BootOrder. Linux registra por `efibootmgr --create-only` e salva dumps binários/textuais em `/var/backups/remote-boot`. Windows salva variáveis e BCD em `%ProgramData%\RemoteBoot\backup-*`. Confirme a ESP correta; em dual boot podem existir várias. O installer Windows usa GPT e o Linux exige ESP GPT/FAT; MBR e Legacy não são suportados na instalação automatizada.

Rerun: uma única entrada compatível é reutilizada; entradas duplicadas exigem resolução manual. Windows compara o load option byte a byte. Linux mostra a entrada e exige `REUSE` após conferir a ESP. Cada escrita do arquivo guarda cópia anterior. Falha no meio não tem rollback transacional completo; BootOrder original e loaders existentes continuam disponíveis. Guarde o backup até concluir os testes.

Recuperação imediata: abra o menu de boot UEFI e escolha diretamente o loader anterior. No Linux, cancele um teste com `sudo efibootmgr --delete-bootnext`; restaure a ordem usando o conteúdo de `BootOrder` com `efibootmgr --bootorder`. No Windows, `Firmware.Write('BootNext', [byte[]]@())` remove BootNext após carregar Common.ps1; `Firmware.Write('BootOrder', [IO.File]::ReadAllBytes('CAMINHO_DO_BACKUP'))` restaura o backup correspondente. Não copie um dump de outra máquina.

Desinstalar agent Linux: `sudo systemctl disable --now remote-boot`; remova o serviço e configs se desejado. Windows: `Unregister-ScheduledTask -TaskName RemoteBootAgent -Confirm`, depois apague a pasta protegida se não precisar dos backups. Remova a entrada Remote Boot identificada na máquina somente após restaurar a ordem; não apague outros loaders.

Atualização ESP32: upload PlatformIO padrão não faz erase-flash. Não use erase-flash se quiser manter NVS. Schema incompatível é preservado. Não há OTA. Alterou código EFI ou IP ESP32? Recompile/reinstale iPXE. Atualizar somente o firmware ESP32 não altera o binário EFI no PC.

Tokens precisam ser atualizados nos agents se forem trocados na dashboard. Alterações Sinric/rede reiniciam ESP32; salve trabalho na UI primeiro.
