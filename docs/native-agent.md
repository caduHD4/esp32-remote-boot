# Agent nativo C# — protocolo 2 e pareamento por código

O agent NativeAOT funciona em Windows x64 e Linux x64 UEFI sem instalar o runtime .NET. Cada instalação tem `agent_id` e token individuais, vinculados a um `pc_id` pelo administrador. Dois sistemas operacionais no mesmo PC usam vínculos diferentes para o mesmo PC. Hostname e nome da instalação são metadados, não identidade.

## Instalar

Em [Releases](https://github.com/caduHD4/esp32-remote-boot/releases), baixe `remote-boot-agent-win-x64.zip` ou `remote-boot-agent-linux-x64.zip` e `agent-SHA256SUMS` da mesma versão. Confira o SHA-256 do ZIP com `Get-FileHash ARQUIVO.zip -Algorithm SHA256` no Windows ou `sha256sum ARQUIVO.zip` no Linux e compare com a linha correspondente em `agent-SHA256SUMS` antes de extrair.

Extraia o ZIP preservando as subpastas. A pasta extraída contém o binário em `build/agent/win-x64/remote-boot-agent.exe` ou `build/agent/linux-x64/remote-boot-agent`, o installer em `installer/windows/` ou `installer/linux/`, o serviço Linux em `agent/linux/remote-boot.service` e este guia em `docs/native-agent.md`. Execute os comandos abaixo a partir da raiz dessa pasta; os installers encontram o binário nesse layout sem um argumento de caminho adicional.

O firmware precisa oferecer schema 3 e API v2; configurações antigas exigem instalação limpa e novo pareamento.

Na dashboard, abra a janela de pareamento. Execute como administrador/root:

```powershell
.\installer\windows\install-agent.ps1 -EspAddress '192.168.1.50'
```

```bash
chmod +x build/agent/linux-x64/remote-boot-agent
sudo bash installer/linux/install-agent.sh 192.168.1.50
```

O installer solicita opt-in separado para reboot e shutdown. O agent imprime um código como `ABCD-EFGH`. Na dashboard, procure esse código, confira hostname/OS e selecione ou crie o PC correto antes de aprovar. O código dura cinco minutos e o agent consulta a aprovação a cada dois segundos. Cancelamento, expiração ou erro encerram a instalação sem habilitar um novo serviço/tarefa.

O installer protege a pasta antes do pareamento. A credencial é salva por arquivo temporário, flush e rename somente após aprovação; o segredo do dispositivo fica em memória e nunca é passado em URL/argv. Depois de salvar, o agent testa hello e sincronização do catálogo com a identidade aprovada; somente então confirma a entrega. Respostas de consulta perdidas são repetidas sem criar outro vínculo. Se a ESP reiniciar antes da entrega, remova o vínculo ainda desconectado na dashboard e repita o pareamento.

O installer verifica hello autenticado e sincronização do catálogo antes de habilitar a inicialização. Se falhar após a aprovação, confira/remova o vínculo na dashboard antes de tentar novamente. Uma instalação anterior é parada durante a substituição; em caso de falha, sua configuração permanece e pode ser reiniciada manualmente.

- Windows: tarefa `RemoteBootAgent`, executada como SYSTEM na inicialização; arquivos em `%ProgramData%\RemoteBoot`, ACL somente SYSTEM/Administradores.
- Linux: serviço `remote-boot`, binário em `/opt/remote-boot/remote-boot-agent`, configuração `/etc/remote-boot/agent.json` com modo 0600/root em pasta 0700/root.
- O pareamento não altera UEFI, BootOrder ou arquivos EFI. A instalação de boot remoto é uma etapa separada e deve usar o PC vinculado.

## CLI e configuração

Crie primeiro uma pasta protegida e use um terminal elevado:

```text
remote-boot-agent --pair --url http://192.168.1.50 --config PATH
remote-boot-agent --pair --url http://192.168.1.50 --config PATH --allow-reboot --allow-shutdown
remote-boot-agent --config PATH --check
remote-boot-agent --config PATH
remote-boot-agent --self-test
```

`--check` realiza hello e sync uma vez, sem processar comandos de energia. Permissões são desabilitadas por padrão. A configuração gerada contém `protocol:2`, `url`, `pc_id` e `agent_id` de 32 hexadecimais minúsculos, `token` de 64 hexadecimais e `allow_reboot`/`allow_shutdown`. O campo opcional `ws_port` tem padrão 81. Não copie tokens nem use a senha administrativa como credencial do agent. Um lock na pasta impede processos simultâneos.

## Protocolo e segurança

O canal é `ws://IP:81/agent/v2`. Hello inclui protocolo, agent, token, sessão aleatória, hostname, OS, BootCurrent e objeto `permissions:{reboot,shutdown}`. O agent confere `pc_id`, `agent_id` e sessão da resposta ready. Sync usa `POST /api/v2/agent/systems/sync`, bearer token individual e header `X-Agent-Session`; o servidor deriva o PC do registro autenticado.

Comandos, ACK e resultados incluem PC, agent, sessão e ID. Identidade divergente, comando repetido, ação sem permissão e confirmação atrasada são recusados. O agent persiste o ID antes do ACK; só executa energia após confirmação positiva dentro de 15 segundos. Reboot grava BootNext nesse momento e restaura o valor anterior se o OS recusar a operação. Shutdown não grava BootNext. Nenhum texto de shell arbitrário é executado.

O catálogo é sincronizado ao conectar e por descoberta solicitada pelo servidor. Keepalive é de 60 s; reconexão usa backoff até 60 s. Até quatro sessões autenticadas são aceitas pela ESP; dois agents vinculados ao mesmo PC não podem manter sessões simultâneas. Revogue uma instalação na dashboard para invalidar sua credencial. Versões antigas não recebem compatibilidade silenciosa.

HTTP/WebSocket usam LAN confiável ou a rede privada configurada; não exponha as portas publicamente. O cliente não usa proxy nem segue redirecionamentos. Pareamento limita cada request a oito segundos, a operação a cinco minutos e respostas a 12.000 bytes. Logs normais mostram somente estado/código, nunca token ou segredo.

Linux: `systemctl status remote-boot` e `journalctl -u remote-boot -n 50`. Windows: confira a tarefa no Task Scheduler. Para diagnóstico interativo, pare a tarefa/serviço e execute `--config PATH`; reinicie ao terminar. Online/ACK não comprovam desligamento elétrico.

## Compilar e validar

Para compilar ou executar os fixtures, use um checkout do repositório ou o arquivo de código-fonte da release; o ZIP do agent contém somente o executável, installer, serviço e guia. Instale .NET SDK 10 e os [pré-requisitos NativeAOT](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/). Compile Windows em Windows e Linux em Linux:

```powershell
dotnet run --project agent/native/RemoteBoot.Agent.csproj -- --self-test
dotnet publish agent/native/RemoteBoot.Agent.csproj -c Release -r win-x64 -o build/agent/win-x64
.\build\agent\win-x64\remote-boot-agent.exe --self-test
```

```bash
dotnet publish agent/native/RemoteBoot.Agent.csproj -c Release -r linux-x64 -o build/agent/linux-x64
./build/agent/linux-x64/remote-boot-agent --self-test
```

Fixtures dos installers substituem comandos do sistema e escrevem somente em pastas temporárias:

```powershell
pwsh -File agent/native/tests/installer-fixtures.ps1
```

```bash
python agent/native/tests/installer-fixtures.py
bash -n installer/linux/install-agent.sh
shellcheck installer/linux/install-agent.sh
```

Self-tests usam servidor loopback/FakeHost para pareamento, reconexão, identidade, permissões, persistência e ACK. Não desligam, reiniciam ou escrevem UEFI do host. Linux ARM64 pode ser compilado nativamente, mas não torna o bootloader x64 compatível com ARM. Publicação e validação de hardware são gates separados; consulte os resultados da execução específica.
