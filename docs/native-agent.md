# Agent C# com WebSocket

A partir do firmware 2.1, o cliente principal é C# compilado em NativeAOT. O mesmo código atende Windows x64 e Linux x64 UEFI. O binário inclui o necessário do .NET: o usuário não instala .NET nem executa PowerShell/Bash continuamente. NativeAOT ainda depende das bibliotecas do sistema operacional; o build Linux do workflow usa Ubuntu 22.04 como base. O fluxo de boot iPXE/RemoteBoot.efi continua x86-64.

## Obter os binários

1. Abra **Actions → Native agent** neste repositório.
2. Escolha uma execução bem-sucedida referente à versão que você baixou.
3. Baixe `agent-win-x64` e/ou `agent-linux-x64` em **Artifacts** (repositório privado exige login e acesso).
4. Extraia o arquivo e confira o hash SHA256 fornecido junto dele. Os artifacts têm prazo de retenção do GitHub; execute novamente o workflow se expirarem.
5. No Linux, marque o executável com `chmod +x remote-boot-agent`.

Para compilar por conta própria, instale .NET SDK 10 e os [pré-requisitos NativeAOT](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/). Compile Windows em Windows, Linux em Linux:

```bash
# Linux x64 (requer clang e headers zlib)
dotnet publish agent/native/RemoteBoot.Agent.csproj -c Release -r linux-x64 -o build/agent/linux-x64
./build/agent/linux-x64/remote-boot-agent --self-test
```

```powershell
# Windows x64 (requer build tools C++ do Visual Studio)
dotnet publish agent/native/RemoteBoot.Agent.csproj -c Release -r win-x64 -o build/agent/win-x64
.\build\agent\win-x64\remote-boot-agent.exe --self-test
```

`--self-test` usa host simulado e sockets locais: não desliga, não reinicia e não altera variáveis UEFI. Linux ARM64 pode ser compilado em uma máquina ARM64 com `-r linux-arm64`, mas não faz parte da matriz inicial validada e não torna o bootloader x64 compatível com PCs ARM.

## Instalar ou migrar

Atualize **primeiro a ESP32**: `pio run -e esp32c3_4mb -t upload`. Isso não exige reconstruir iPXE. Depois execute os installers com os binários baixados/compilados.

Linux, da raiz do repositório:

```bash
sudo bash installer/linux/install-agent.sh SEU_IP_DA_ESP32 /caminho/remote-boot-agent
```

Windows PowerShell como administrador, da raiz do repositório:

```powershell
.\installer\windows\install-agent.ps1 -EspAddress 'SEU_IP_DA_ESP32' -AgentFile 'C:\caminho\remote-boot-agent.exe'
```

Substitua todos os placeholders. Informe o agent token e habilite `REBOOT`/`SHUTDOWN` apenas para as ações desejadas. O installer roda o self-test antes de substituir o cliente antigo. Se compilou nas pastas padrão acima, pode omitir o caminho do binário. O installer completo do Linux também utiliza essa pasta padrão ao escolher `AGENT`.

A migração para a mesma unidade/tarefa para o agent antigo e inicia o novo. Scripts antigos permanecem como referência/compatibilidade, mas não são iniciados pelos installers atuais. Não inicie o agent legado simultaneamente. Os campos `url`, `token`, `allow_reboot` e `allow_shutdown` continuam válidos. O campo opcional `ws_port` tem padrão 81.

- Linux: serviço systemd `remote-boot`, executável em `/opt/remote-boot/remote-boot-agent`, configuração em `/etc/remote-boot/agent.json`.
- Windows: tarefa `RemoteBootAgent` como SYSTEM executa diretamente `%ProgramData%\RemoteBoot\remote-boot-agent.exe`, configuração na mesma pasta. É uma tarefa de inicialização, não um Windows Service registrado no SCM.
- Um lock na pasta de configuração impede dois processos usando a mesma configuração.

## Funcionamento

1. Agent abre `ws://IP_DA_ESP32:81/agent` e autentica com o **agent token** em uma mensagem `hello` (admin token não substitui o agent token neste canal).
2. Envia OS, hostname, BootCurrent, permissões e sessão aleatória nova por conexão. Sincroniza o catálogo UEFI via HTTP uma vez.
3. Fica aguardando `ReceiveAsync`. A ESP32 envia reboot/shutdown imediatamente quando autorizado pela dashboard ou Sinric.
4. O catálogo só é atualizado novamente ao reconectar ou clicar **Atualizar catálogo pelo agent**. Não há varredura de cinco em cinco minutos.
5. Keepalive WebSocket de 60 s detecta falhas de conexão. Isso é tráfego de controle, não consulta periódica de comandos ou de UEFI. Comandos não precisam esperar pelo keepalive. Reconexão usa backoff de 1 até 60 s com jitter.

O comando expira em 30 s na ESP32. O agent valida ação, permissão, ID e sessão, registra o ID consumido e pede confirmação por WebSocket. Só executa após ACK positivo, em até 15 s da recepção. Falha/timeout/reconexão cancela a ação. Reboot grava BootNext somente após esse ACK; se a solicitação de reboot ao OS falhar, restaura o BootNext anterior. Shutdown não modifica BootNext. Comandos não executam texto de shell arbitrário.

Windows usa APIs nativas de firmware e `InitiateSystemShutdownExW` sem forçar fechamento de aplicativos. Linux lê efivarfs diretamente; usa `efibootmgr` somente para BootNext e `systemctl` para poweroff/reboot. Aplicações/inhibitors podem recusar a operação. ACK e status online não comprovam desligamento elétrico.

## Rede, status e diagnóstico

Não abra portas no PC nem exponha a ESP32 à internet. O PC inicia as conexões TCP 81 (WebSocket) e TCP 80 (configuração/catálogo). `ws://`/HTTP não criptografam o token; use a LAN confiável já exigida pelo projeto. Sinric continua a integração pela internet.

A ESP32 aceita apenas um agent autenticado por vez. Conexões não autenticadas são encerradas após 5 s. O handler do firmware aceita mensagens de aplicação de até 12.000 bytes; o SDK WebSockets pode alocar até 15 KiB por frame antes dessa checagem. O firmware rejeita fragmentação de mensagens de aplicação. Enquanto o WebSocket está ativo, o heartbeat HTTP legado recebe 409. Uma queda detectada encerra a sessão e invalida comandos pendentes; conexão silenciosamente perdida pode levar até cerca de 90 s para ser marcada offline. A dashboard ainda consulta **seu próprio status** a cada 10 s enquanto está aberta; isso é independente do agent.

Linux: `systemctl status remote-boot` e `journalctl -u remote-boot -n 50`. Windows: confira a tarefa no Task Scheduler. Para diagnóstico, pare a tarefa e execute o binário em terminal elevado com `--config`; reinicie a tarefa ao terminar. Erros também aparecem nos logs da ESP32 como `AGENT_DISCONNECTED`, `POWER_ACK_REJECTED` ou `OS_POWER_REFUSED`.

## Limites de validação

Os testes usam sockets TCP/WebSocket reais e comandos de energia simulados; verificam permissão, sessão, ACK, duplicação e descoberta por evento. O workflow publica e testa binários NativeAOT para cada sistema. Consulte o resultado da execução específica antes de baixar. Não há medição em seu hardware de RAM/CPU/FPS, nem validação física de UEFI/shutdown/Sinric. NativeAOT evita runtime externo; não significa consumo zero.
