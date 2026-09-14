# Setup guiado da dashboard

O primeiro acesso exige Wi-Fi configurado em `config.local.json`; o SoftAP e o captive portal foram removidos. Abra o IP mostrado no serial.

O wizard solicita, em ordem:

1. senha administrativa de 1–8 caracteres e reinício;
2. nome e MAC do primeiro computador;
3. senha opcional do agent, também com até 8 caracteres;
4. configuração de boot somente quando houver agent;
5. Sinric Pro e Tailscale opcionais;
6. conclusão e reinício final.

A rota pública só informa a etapa atual. Apenas a criação da primeira senha é pública e deixa de existir logicamente após ser concluída. As demais etapas exigem a senha administrativa. Cinco falhas consecutivas bloqueiam novas tentativas por 30 segundos.

A variante MicroLink recebe a Auth Key do Tailscale pelo wizard/dashboard. A chave fica na NVS, nunca é devolvida pela API e uma alteração exige reinício.

# Instalação, atualização e recuperação

Para o cliente residente, siga primeiro [Agent nativo C#](native-agent.md). Os installers atuais exigem o binário NativeAOT e iniciam C# via WebSocket. PowerShell/Bash abaixo são ferramentas de instalação e recuperação, não o processo residente. HTTP heartbeat permanece só para clientes legados.

Siga README para onboarding. A primeira sincronização pode ocorrer em apenas um OS; instale agent em cada OS que deseja identificar/reiniciar. Linux requer systemd; a descoberta/API não depende de Python. `jq` é dependência explícita.

Configure padrão e fallback antes do primeiro BootNext. Nenhum ID é escolhido automaticamente. A opção `exit_to_firmware` deixa a política do boot físico para o firmware e precisa ser testada na máquina.

Installers preservam BootOrder. Linux registra por `efibootmgr --create-only` e salva dumps binários/textuais em `/var/backups/remote-boot`. Windows salva variáveis e BCD em `%ProgramData%\RemoteBoot\backup-*`. Confirme a ESP correta; em dual boot podem existir várias. O installer Windows usa GPT e o Linux exige ESP GPT/FAT; MBR e Legacy não são suportados na instalação automatizada.

Rerun: uma única entrada compatível é reutilizada, inclusive quando `efibootmgr` imprime o device path depois do rótulo; entradas duplicadas exigem resolução manual. Windows compara o load option byte a byte. Linux mostra a entrada e exige `REUSE` após conferir a ESP. Cada escrita do arquivo guarda cópia anterior. Falha no meio não tem rollback transacional completo; BootOrder original e loaders existentes continuam disponíveis. Guarde o backup até concluir os testes.

Recuperação imediata: abra o menu de boot UEFI e escolha diretamente o loader anterior. No Linux, cancele um teste com `sudo efibootmgr --delete-bootnext`; restaure a ordem usando o conteúdo de `BootOrder` com `efibootmgr --bootorder`. No Windows, `Firmware.Write('BootNext', [byte[]]@())` remove BootNext após carregar Common.ps1; `Firmware.Write('BootOrder', [IO.File]::ReadAllBytes('CAMINHO_DO_BACKUP'))` restaura o backup correspondente. Não copie um dump de outra máquina.

Desinstalar agent Linux: `sudo systemctl disable --now remote-boot`; remova o serviço e configs se desejado. Windows: `Unregister-ScheduledTask -TaskName RemoteBootAgent -Confirm`, depois apague a pasta protegida se não precisar dos backups. Remova a entrada Remote Boot identificada na máquina somente após restaurar a ordem; não apague outros loaders.

Atualização ESP32: upload PlatformIO padrão não faz erase-flash. Não use erase-flash se quiser manter NVS. Schema incompatível é preservado. Não há OTA. Alterou código EFI ou IP ESP32? Recompile/reinstale iPXE. Atualizar somente o firmware ESP32 não altera o binário EFI no PC.

Para testar acesso remoto pelo próprio ESP32-C3, siga [MicroLink + Tailscale](microlink-tailscale.md). A variante experimental tem um environment separado; voltar a `esp32c3_4mb` não apaga a NVS.

Senhas do agent precisam ser atualizadas no respectivo PC se forem trocadas na dashboard. Alterações Sinric, rede ou Tailscale reiniciam a ESP32; salve trabalho primeiro.
