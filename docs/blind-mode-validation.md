# Modo cego: validação e ativação

O modo é opcional por PC. A checkbox grava a intenção; a conversão assistida
prepara o agente e o boot. PCs cadastrados mantêm IDs, token e permissões.
Veja [conversão assistida](../installer/README.md).

## Evidências de software

- Suíte local C/C++/Python/JS: passou, incluindo compatibilidade da configuração,
  identidade persistente, validação de assinatura e relatórios de preparação.
- Interface: 108 testes Chromium passaram, em quatro larguras, incluindo criar,
  editar e exportar perfil sem alterar DHCP ou iniciar pareamento.
- Agente Linux NativeAOT: self-test passou com o executável copiado sozinho para
  uma pasta vazia. O vetor público de interoperabilidade está embutido.
- DNS que nunca termina não bloqueia a busca. O prazo usa relógio monotônico e
  também é conferido depois de verificar a assinatura.
- Conversão: 29 cenários Linux, 21 PowerShell e aplicação com o binário Linux
  real passaram. Incluem falhas parciais, metadados privados, tarefa parada,
  backups corrompidos e recuperação sem pasta EFI preexistente.
- Boot: sete cenários em QEMU/OVMF passaram: cache, mudança de IP via broadcast,
  somente mDNS, chave errada, mDNS falso, silêncio e quadros malformados assinados.
  O loader não aceita uma prova cuja verificação termina depois do prazo.
- Revisões independentes: correções de boot, conversão e transportes aprovadas.

## Evidência na ESP e limites

A ESP real respondeu à descoberta assinada com a chave fixada. Um perfil com
IP antigo encontrou o endereço atual por mDNS e confirmou a assinatura antes
de aceitar o endereço. Não foi necessário alterar o roteador.

A investigação posterior identificou uma rota local ausente na interface
cabeada deste PC: mesmo com o cabo conectado, o acesso à ESP saía pelo Wi-Fi,
onde havia perdas. Reaplicar a conexão cabeada restaurou a rota automática da
LAN. O teste final teve oito downloads completos da dashboard e vinte pings
sem perdas; o usuário confirmou abertura e login. Não ficou rota fixa para o
IP da ESP. Veja [diagnóstico de acesso](network-access-2026-10-23.md).
A imagem completa foi restaurada após os diagnósticos; Tailscale não foi
desativado na configuração. O caminho sem fio ainda precisa de nova validação.

Foram corrigidos dois defeitos de transporte identificados no código: o envio
do HTML agora trata escritas parciais e contrapressão com prazos; cada cliente
TLS Sinric recebe limite de handshake de três segundos. Essas correções não
constituem a correção da rota local descrita acima. DNS do serviço
Sinric e a pressão de memória da coordenação MicroLink ainda exigem investigação
separada; não houve redução silenciosa de recursos ou remoção de credenciais.

Não houve alteração da EFI, BootOrder, serviço ou configuração do agente deste
PC. A ativação exige executar o instalador localmente, confirmar a impressão
digital e escolher os passos assistidos. Em dual boot, prepare cada agente.
Não foi feito boot físico, desligamento do PC nem mudança real da concessão
DHCP. Esses testes continuam sendo a aceitação final em hardware.

Os testes PowerShell locais usam fronteiras de sistema simuladas. O runtime
Windows nativo ainda precisa da execução na matriz Windows do CI ou num PC
Windows; a execução sob Wine não validou o provedor criptográfico e não é
contabilizada como aprovação Windows.
