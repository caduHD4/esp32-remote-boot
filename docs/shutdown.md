# Shutdown remoto

O agent C# NativeAOT do sistema que estiver rodando recebe o pedido por WebSocket e solicita shutdown local. Não abre porta no PC e não consulta comandos em loop HTTP. Não exige .NET instalado, mas depende das bibliotecas nativas do OS. Consumo real ainda não foi medido.

### Instalar ou atualizar o agent

Siga o [guia do agent nativo](native-agent.md): primeiro atualize a ESP32, obtenha o binário Windows/Linux, execute o installer com o caminho do binário e habilite `SHUTDOWN`. Instale nos dois sistemas. Para shutdown não é necessário reconstruir iPXE nem alterar BootOrder.

A configuração local usa `allow_shutdown: true`. Para revogar, troque para `false` e reinicie serviço/tarefa. Linux: `/etc/remote-boot/agent.json`; Windows: `%ProgramData%\RemoteBoot\agent.json`.

### Usar na dashboard

1. Com o PC ligado e o agent iniciado, abra a dashboard da ESP32.
2. Aguarde a conexão do agent. O botão **Desligar PC** fica habilitado quando o agent anuncia permissão de shutdown.
3. Salve os arquivos abertos, clique no botão e confirme. A ESP32 envia a solicitação pela conexão ativa; não é preciso esperar pelo keepalive.
4. O Linux executa `systemctl poweroff`; o Windows usa `InitiateSystemShutdownExW` sem forçar aplicativos. O pedido não altera BootNext nem seleciona outro sistema. Aplicações/inhibitors podem impedir o desligamento; a aceitação pela ESP32 não comprova que o sistema desligou.

### Usar pelo Sinric Pro

1. Preserve os dois Switches de boot já configurados.
2. Crie um Switch adicional no Sinric Pro chamado **Desligar PC**, conforme a disponibilidade do seu plano.
3. Na dashboard, abra **Sinric • até 8 dispositivos Switch**, clique em **Adicionar dispositivo**, cole o novo Device ID e selecione **Desligar PC (agent)**. Salve e aguarde `Sinric online`.
4. Envie **ON** para esse Switch para desligar o sistema atualmente rodando. Ele volta automaticamente para OFF após o pedido ser aceito. O evento OFF continua sem ação.
5. Para uma frase natural como “Alexa, desligar computador”, configure uma rotina no assistente cuja ação seja **ligar o Switch Desligar PC**. O Switch representa um comando momentâneo, não o estado elétrico do computador.

Não existe confirmação adicional no comando Sinric: habilitar `allow_shutdown` e mapear o Switch autoriza o seu acionamento remoto. Um único Switch de shutdown atende Windows e Linux, conforme o agent que estiver online.

### Entrega e diagnóstico

O firmware só aceita shutdown com agent online e permissão explícita. O comando fica em RAM, expira em 30 segundos e está vinculado à sessão atual do processo agent. Troca de sistema, reinício do agent ou da ESP32 invalida o comando pendente. O agent registra o ID recebido e exige confirmação da ESP32 por WebSocket antes de executar. Se essa confirmação se perder, o shutdown é cancelado; faça um novo pedido.

| Sintoma | Verificação |
|---|---|
| Botão desabilitado | Agent atualizado, `allow_shutdown: true`, token/IP corretos e conexão WebSocket ativa. |
| Sinric liga o PC em vez de desligar | O novo slot deve apontar para **Desligar PC (agent)**, não para um Boot ID. |
| Pedido aceito, PC continua ligado | Veja os logs do agent; confirmação pode ter expirado/falhado ou o OS pode ter recusado shutdown. |
| Funciona em um OS somente | Instale e habilite shutdown também no outro OS. |

No Linux: `systemctl status remote-boot` e `journalctl -u remote-boot -n 50`. No Windows: confira `RemoteBootAgent` no Task Scheduler; para diagnóstico interativo, pare a tarefa e execute `remote-boot-agent.exe --config CAMINHO_DO_AGENT_JSON` em um PowerShell elevado. Reinicie a tarefa ao terminar. A dashboard pode mostrar offline até cerca de 90 segundos depois de uma queda silenciosa; isso não prova o estado elétrico do PC.

Referências dos comandos locais: [Microsoft InitiateSystemShutdownExW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-initiatesystemshutdownexw) e [systemctl](https://www.freedesktop.org/software/systemd/man/latest/systemctl.html).

Validação desta alteração: testes de fila/sessão/expiração e dos handlers com comandos de energia simulados. Shutdown real, cloud Sinric e consumo dos agents ainda precisam ser testados na máquina.
