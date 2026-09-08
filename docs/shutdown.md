# Shutdown remoto

O agent do sistema que estiver rodando recebe o pedido da ESP32 e solicita shutdown local. Ele usa o mesmo heartbeat a cada 12 segundos; não abre porta de entrada no PC. Linux usa Bash/systemd; Windows usa Windows PowerShell 5.1 em uma tarefa SYSTEM. Não há GUI nem Python no cliente, mas o consumo de CPU/RAM ainda não foi medido em hardware real.

### Instalar ou atualizar o agent

Atualize o firmware da ESP32 com `pio run -e esp32c3_4mb -t upload`. Para shutdown não é necessário reconstruir iPXE nem alterar BootOrder. Baixe também o código atualizado nos dois sistemas.

No Linux UEFI com systemd, na raiz do repositório:

```bash
sudo bash installer/linux/install-agent.sh
```

Informe o IP da ESP32 e o **agent token**. Na pergunta sobre shutdown, digite `SHUTDOWN`. A permissão de reboot é independente. Este installer instala/atualiza somente o serviço, sem escrever na ESP/UEFI.

No Windows, abra Windows PowerShell como administrador na raiz do repositório:

```powershell
.\installer\windows\install-agent.ps1 -EspAddress 'SEU_IP_DA_ESP32'
```

Substitua o placeholder pelo IPv4 reservado. Informe o agent token e digite `SHUTDOWN` na pergunta sobre desligamento. O installer registra/inicia a tarefa `RemoteBootAgent` como SYSTEM. A política de execução do Windows precisa permitir esses scripts, conforme a política do administrador.

Instale em **ambos** os sistemas. Clientes/configurações antigos continuam com shutdown desabilitado. A configuração local usa `allow_shutdown: true`; para revogar, troque para `false` e reinicie o serviço/tarefa. Linux: `/etc/remote-boot/agent.json`; Windows: `%ProgramData%\RemoteBoot\agent.json`.

### Usar na dashboard

1. Com o PC ligado e o agent iniciado, abra a dashboard da ESP32.
2. Aguarde o heartbeat. O botão **Desligar PC** fica habilitado quando o agent anuncia permissão de shutdown.
3. Salve os arquivos abertos, clique no botão e confirme. A solicitação aguarda o próximo heartbeat; esse intervalo é de 12 segundos, além de eventuais atrasos de rede/sistema.
4. O Linux executa `systemctl poweroff`; o Windows executa `Stop-Computer` sem `-Force`. O pedido não altera BootNext nem seleciona outro sistema. Aplicações/inhibitors podem impedir o desligamento; a aceitação pela ESP32 não comprova que o sistema desligou.

### Usar pelo Sinric Pro

1. Preserve os dois Switches de boot já configurados.
2. Crie um Switch adicional no Sinric Pro chamado **Desligar PC**, conforme a disponibilidade do seu plano.
3. Na dashboard, abra **Sinric • até 8 dispositivos Switch**, clique em **Adicionar dispositivo**, cole o novo Device ID e selecione **Desligar PC (agent)**. Salve e aguarde `Sinric online`.
4. Envie **ON** para esse Switch para desligar o sistema atualmente rodando. Ele volta automaticamente para OFF após o pedido ser aceito. O evento OFF continua sem ação.
5. Para uma frase natural como “Alexa, desligar computador”, configure uma rotina no assistente cuja ação seja **ligar o Switch Desligar PC**. O Switch representa um comando momentâneo, não o estado elétrico do computador.

Não existe confirmação adicional no comando Sinric: habilitar `allow_shutdown` e mapear o Switch autoriza o seu acionamento remoto. Um único Switch de shutdown atende Windows e Linux, conforme o agent que estiver online.

### Entrega e diagnóstico

O firmware só aceita shutdown com agent online e permissão explícita. O comando fica em RAM, expira em 30 segundos e está vinculado à sessão atual do processo agent. Troca de sistema, reinício do agent ou da ESP32 invalida o comando pendente. O agent registra o ID recebido e exige confirmação da ESP32 antes de executar. Se essa confirmação se perder, o shutdown é cancelado; faça um novo pedido.

| Sintoma | Verificação |
|---|---|
| Botão desabilitado | Agent atualizado, `allow_shutdown: true`, token/IP corretos e heartbeat recente. |
| Sinric liga o PC em vez de desligar | O novo slot deve apontar para **Desligar PC (agent)**, não para um Boot ID. |
| Pedido aceito, PC continua ligado | Veja os logs do agent; confirmação pode ter expirado/falhado ou o OS pode ter recusado shutdown. |
| Funciona em um OS somente | Instale e habilite shutdown também no outro OS. |

No Linux: `systemctl status remote-boot` e `journalctl -u remote-boot -n 50`. No Windows: confira `RemoteBootAgent` no Task Scheduler; para diagnóstico interativo, pare a tarefa e execute `agent.ps1` da pasta instalada em um PowerShell elevado. Reinicie a tarefa ao terminar. A dashboard pode mostrar offline apenas após 45 segundos sem heartbeat; isso não prova o estado elétrico do PC.

Referências dos comandos locais: [Microsoft Stop-Computer](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.management/stop-computer?view=powershell-5.1) e [systemctl](https://www.freedesktop.org/software/systemd/man/latest/systemctl.html).

Validação desta alteração: testes de fila/sessão/expiração e dos handlers com comandos de energia simulados. Shutdown real, cloud Sinric e consumo dos agents ainda precisam ser testados na máquina.
