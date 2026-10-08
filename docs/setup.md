# Instalação, atualização e recuperação

## Configurar cada PC

1. Configure Wi-Fi e a rede da ESP32 em `config.local.json`, grave o firmware e abra a dashboard pelo IP reservado. A API v2 usa o token administrativo; não há SoftAP de recuperação.
2. Na dashboard, crie um PC e copie seu ID de 32 caracteres hexadecimais minúsculos e MAC Ethernet. Há limite de quatro PCs.
3. Em **Pareamento**, abra uma janela temporária e instale o agent nativo no sistema operacional do PC. Use o código exibido pelo agent e associe-o ao PC correto na dashboard; aprove a solicitação. O agent grava configuração protocol 2, verifica hello/catálogo e confirma o pareamento. Repita para outros sistemas operacionais, se necessário. Há até oito agents no total; apenas uma sessão ativa por PC.
4. O agent descobre e sincroniza apenas o catálogo do PC pareado. Confirme na dashboard as entradas `Boot####`; são aceitas até 24 por PC. Defina padrão/fallback e teste cada destino antes de habilitar o boot remoto.
5. Compile o iPXE para esse PC com `build.sh ESP_IPV4 PC_ID [output-dir] [timeout-ms]`. O builder aceita somente PC ID com 32 caracteres hexadecimais minúsculos e embute `http://ESP_IPV4/boot/PC_ID.ipxe`. O manifesto registra IP, PC ID e SHA-256 de iPXE e loader. Mantenha os arquivos juntos.
6. Execute o installer da plataforma como administrador/root, fornecendo o diretório de artefatos gerados. O installer lê `pc_id` e URL da configuração pareada do agent e valida manifesto, PC alvo e hashes antes de escrever na ESP. Ele salva backup e preserva BootOrder. Não use a imagem iPXE de um PC para outro.

Consulte [Agent nativo C#](native-agent.md) para instalação, permissões e atualização. O agent residente é NativeAOT e usa WebSocket; os scripts PowerShell/Bash são instaladores. Não há sincronização HTTP legada nem token global de agent. Uma troca de PC exige novo vínculo/configuração e os artefatos iPXE daquele PC.

## Firmware e armazenamento

O firmware grava schema 3 no namespace NVS `remote-boot-v3`. Dois bancos (`bank0`, `bank1`) armazenam snapshots JSON com geração, tamanho e CRC32; o marcador `commit` seleciona o último banco validado. O limite do snapshot é 20.000 bytes. Na partição padrão, NVS começa em `0x9000` com tamanho `0x10000`, PHY em `0x19000` e app em `0x20000`. Upload normal do PlatformIO não apaga NVS. Evite `erase-flash` se quiser preservar configuração. Formato incompatível/corrompido não deve ser apagado automaticamente. Não há OTA.

## Instalação EFI, atualização e recuperação

Windows automatizado exige GPT; Linux exige ESP GPT/FAT. MBR e Legacy não são suportados. Confirme a ESP correta: máquinas dual boot podem ter várias. Linux cria entrada com `efibootmgr --create-only` e salva dumps em `/var/backups/remote-boot`; Windows salva variáveis e BCD em `%ProgramData%\RemoteBoot\backup-*`. Em rerun, uma única entrada compatível pode ser reutilizada. Linux pede `REUSE` após mostrar a entrada e ESP. Duplicatas precisam de resolução manual. Cada arquivo substituído recebe cópia anterior.

Falha no meio não tem rollback transacional completo. Guarde o backup até concluir os testes. Para recuperação imediata, abra o menu UEFI e escolha o loader anterior. Linux: `sudo efibootmgr --delete-bootnext`; restaure a ordem usando o `BootOrder` do backup com `efibootmgr --bootorder`. Windows, após carregar `Common.ps1`: `Firmware.Write('BootNext', [byte[]]@())` remove BootNext; restaure `BootOrder` com os bytes do backup correspondente. Não use dump de outra máquina.

Desinstalar agent Linux: `sudo systemctl disable --now remote-boot`, depois remova serviço/config se desejado. Windows: `Unregister-ScheduledTask -TaskName RemoteBootAgent -Confirm`; apague a pasta protegida somente se não precisar dos backups. Remova apenas a entrada Remote Boot identificada depois de restaurar a ordem.

Atualizar código EFI ou IP/PC ID da ESP32 exige recompilar e reinstalar iPXE/loader. Atualizar somente o firmware ESP32 não muda os arquivos EFI no PC. Para testar acesso remoto pelo próprio ESP32-C3, veja [MicroLink + Tailscale](microlink-tailscale.md); a variante usa environment separado. Não apague NVS ao voltar para `esp32c3_4mb`.

Senhas Wi-Fi ficam exclusivamente em `config.local.json`. Não publique token admin, token do agent ou segredos Sinric. Reboot/shutdown remotos exigem permissões habilitadas no agent e consentimento correspondente na dashboard. Quedas de Wi-Fi disparam reconexão automática do firmware e do agent.
