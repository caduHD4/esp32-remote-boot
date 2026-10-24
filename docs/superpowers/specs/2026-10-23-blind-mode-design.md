# Modo cego: descoberta da ESP com IP dinâmico

## Objetivo e escolhas do usuário

Permitir usar Remote Boot sem acesso administrativo ao roteador e sem reservar
IPs. O usuário consegue conectar seus dispositivos ao Wi-Fi. PCs e ESP podem
receber outros IPv4 depois de quedas de energia ou reinicializações.

- Modo opcional por PC, chamado **modo cego**, escolhido por checkbox no cadastro.
- Permitir ativar e desativar também em PCs existentes.
- Conversão **assistida pelo instalador**, sem dar ao agent permissão nova para
  escrever arquivos EFI ou alterar a ordem de boot remotamente.
- Preservar credenciais, identidade do PC, agentes, catálogo, mapeamentos Sinric
  e funcionamento do modo atual.
- Implementação futura na branch `feat/dynamic-ip-discovery`, criada da main
  `c3261ba`. Este documento não autoriza gravações na placa ou EFI durante o
  planejamento.

## Situação atual e limites

WoL já usa o MAC Ethernet e broadcast calculado com a sub-rede atual da ESP;
o IPv4 do PC não é necessário para esse envio. O agent inicia a conexão com a
ESP, mas sua configuração e os instaladores aceitam apenas URL HTTP IPv4.
O iPXE é construído com o IPv4 da ESP incorporado no script de seleção.

O endereço da ESP é global: a opção por PC muda como esse PC encontra a placa.
Ela não muda DHCP/endereço da ESP e não garante continuidade de PCs no modo
antigo se o endereço compartilhado mudar. O modo cego funciona com endereço
estático ou DHCP; recomenda-se DHCP para a instalação do amigo. Nunca alterar
a configuração global de rede implicitamente ao marcar a checkbox.

Escopo inicial: IPv4, uma LAN com comunicação permitida entre os dispositivos,
agent Windows/Linux e boot UEFI x86_64 Ethernet, conforme o produto atual.
Não depende de DNS do roteador, reserva DHCP, internet, Tailscale ou porta aberta
no modem. Não atravessa VLANs, Wi-Fi convidado isolado ou isolamento entre
clientes. Não acrescenta boot por Wi-Fi nem exige varredura de todos os IPs.

## Abordagem escolhida

Usar descoberta UDP local como mecanismo comum a agent, localizador desktop e
iPXE. O último IP conhecido é apenas uma pista; uma identidade persistente da
ESP é a referência. mDNS oferece um nome de dashboard, mas não é pré-requisito
para o boot ou para a reconexão do agent.

Alternativas consideradas:

1. **Somente MAC/ARP:** não fornece um endereço HTTP confiável quando a cache
   ARP está vazia; MAC não autentica a placa. Não atende sozinho ao objetivo.
2. **Somente mDNS:** simplifica a dashboard, mas exigiria depender de resolução
   `.local` em todos os clientes, inclusive no ambiente pré-boot. Não usar como
   único mecanismo.
3. **UDP com identidade validada + mDNS auxiliar:** cobre a cadeia completa,
   com uma extensão pequena e mantida no iPXE. Abordagem proposta.

## ESP e protocolo de descoberta

- Manter `device_id` aleatório persistente e um par de chaves ECDSA P-256 em
  namespace NVS separado. A chave privada nunca é publicada pela API ou por UDP.
  MAC pode aparecer como informação auxiliar, nunca como prova de autenticidade.
- Gerar a identidade uma vez ao preparar o recurso; não recriar a identidade
  silenciosamente quando houver erro de leitura/gravação. Falha nesse preparo
  deve impedir apenas a ativação do modo cego, preservando o modo atual.
- Publicar ID/chave pública por fluxo de pareamento aprovado e consulta
  administrativa autenticada. Na conversão, usar o vínculo já existente; não
  pedir outro pareamento nem trocar seus tokens. A dashboard autenticada exporta
  um perfil público por PC com modo, PC ID, ID/chave pública da ESP e versão;
  o instalador exige esse arquivo para a conversão inicial e confere o PC ID
  com a configuração local. Esse perfil não contém tokens. Não obter uma nova
  chave automaticamente de um respondente desconhecido. O instalador mostra
  ID/impressão digital da chave e exige confirmação manual inicial. Se não
  houver uma origem anterior confiável para o perfil, conferir a impressão
  pública pela USB física da ESP; o firmware expõe apenas ID/fingerprint nesse
  diagnóstico, nunca tokens/chave privada. Não tratar a posse de um bearer
  token enviado por HTTP como autenticação do servidor. A confiança inicial continua
  exigindo escolher/aprovar a ESP correta, como no pareamento atual.
- Usar UDP IPv4 na porta privada 49192, com requisição contendo versão, nonce
  aleatório e ID desejado; o localizador inicial pode listar IDs sem selecioná-los.
  A resposta assinada vincula nonce, ID, IPv4 atual e portas HTTP/WebSocket.
- Clientes vinculados verificam a assinatura com a chave pública fixada no
  pareamento/conversão antes de usar um endereço e enviar credenciais. Não
  aceitar automaticamente outra chave, outro ID ou apenas um MAC igual.
- Fixar a codificação binária canônica, limites e vetores de teste compartilhados
  antes de implementar os três clientes. Datagrama máximo de 512 bytes;
  rejeitar versões, tamanhos, assinaturas e nonces inválidos. Limitar frequência
  de respostas e processar um orçamento limitado por passagem do loop.
- Descoberta não carrega tokens, senha Wi-Fi, auth key ou catálogo. Não é uma
  substituição do transporte HTTP atual por TLS; não prometer proteção nova
  contra interceptação ativa de todo o tráfego da LAN.
- Oferecer hostname único `remote-boot-<sufixo-do-id>.local` e serviço mDNS.
  Quando o navegador não resolver `.local`, o localizador do instalador lista
  as ESPs encontradas e abre a dashboard no endereço atual. JavaScript da
  dashboard não tentará fazer broadcast UDP pelo navegador.

## Agent e boot

### Agent

Acrescentar aos metadados vinculados o modo, o ID e a chave pública da ESP,
sem substituir PC ID, agent ID ou token. Arquivos antigos permanecem válidos
no modo atual. A opção ausente equivale a desativada.

No modo cego, validar o endereço em cache; se indisponível, descobrir por UDP
nas interfaces IPv4 LAN ativas. Usar broadcast limitado e broadcast por
sub-rede; não divulgar o token em VPNs ou para qualquer respondente encontrado.
Aguardar rede/DHCP sem loop acelerado. Após validar a placa, abrir a conexão
normal e persistir o novo endereço de forma atômica. Não perder a identidade
nem exigir novo pareamento se a ESP estiver temporariamente ausente.

Preservar o limite de oito agents/quatro PCs. Em dual boot, cada instalação do
agent precisa ser preparada; atualizar um sistema não prepara o outro.

### iPXE

Manter o commit upstream fixado e acrescentar uma extensão `rbdiscover` ao
build do modo cego. A revisão usada pelo projeto dispõe de UDP e verificação
ECDSA; a extensão integra essas APIs, com testes antes do uso físico.

Depois do DHCP do PC, procurar somente o ID/chave pública incorporados no
artefato daquele PC. Tentar o endereço conhecido como pista, depois broadcast;
respeitar um orçamento total de descoberta de dez segundos, dentro do limite
do usuário já suportado. Não exigir IP fixo, DNS do roteador ou token secreto
embutido na imagem EFI.

Com a resposta válida, obter a seleção no endereço atual e continuar o fluxo
iPXE/RemoteBoot.efi existente. Em indisponibilidade, assinatura inválida ou
timeout, não escolher outra ESP: sair pelo caminho de falha/fallback existente,
sem laço de reinicialização. A assinatura de descoberta não deve ser anunciada
como assinatura de todo o script HTTP de seleção, que continua no modelo atual.

Manter nesta versão o fluxo de build por PC já existente: Linux para compilar,
ou Linux/WSL para gerar os artefatos que serão instalados pelo Windows nativo.
Não prometer compilação EFI nativa no Windows nem criar um serviço externo de
build. O perfil público pode ser levado ao ambiente de build; credenciais do
agent não precisam sair do PC. A instalação continua no sistema real do PC,
nunca nas variáveis UEFI do WSL.

O modo atual continua gerando e usando seu script de IPv4 conhecido. Manifestos
passam a identificar também modo, identidade/chave fixada e hashes dos artefatos;
o instalador rejeita artefatos para outro PC/ESP ou outro modo.

## Dashboard e conversão assistida

Checkbox desmarcada por padrão:

> Ativar modo cego — encontrar a ESP automaticamente quando o IP mudar.

Disponível no cadastro e nos ajustes de um PC existente. Mostrar instruções da
conversão e avisar que a checkbox, isoladamente, não atualiza agents ou EFI.
Mostrar separadamente a escolha desejada, compatibilidade de cada agent e
preparo do carregador; não alegar conversão completa enquanto faltar preparo.
Não mostrar um agent offline como comprovadamente preparado apenas pelo modo
solicitado na configuração da ESP.

O instalador Windows/Linux executa a conversão local:

1. Consulta a escolha do PC e confirma a identidade da ESP pelo vínculo existente.
2. Faz backup de configuração do agent, manifesto e carregador EFI existente.
3. Prepara o agent local e gera/verifica a imagem no modo escolhido. Exibe
   exatamente o destino antes da confirmação local de escrita EFI.
4. Reutiliza a entrada EFI existente; não promove a entrada nem altera BootOrder.
   Mantém o teste único por BootNext como escolha explícita do usuário.
5. Reporta o preparo ao servidor usando a autenticação existente, incluindo modo,
   versão do protocolo de descoberta e hashes. O relatório do instalador não é
   prova de boot físico; a dashboard mantém essa distinção.
6. Em falha parcial, mostra qual etapa falhou, conserva os backups e permite
   retomar/restaurar. Não revoga pareamentos nem exclui sistemas para converter.

Desativar segue o mesmo fluxo assistido. Capturar e confirmar o IPv4 que o modo
atual passará a usar; avisar que ele volta a depender desse endereço. Alterar a
checkbox não dispara escrita EFI remota, reboot, shutdown ou troca global de IP.

## Validação e critérios de aceite

- Configuração antiga continua válida com modo desativado; credenciais e IDs
  sobrevivem a habilitação, conversão, reinicialização, retomada e rollback.
- Firmware, C# e iPXE passam pelos mesmos vetores de protocolo/assinatura.
  Respostas falsas, outra ESP, chave diferente, replay com outro nonce,
  datagramas truncados e excesso de respostas não desviam a conexão.
- Testar troca de IP/sub-rede do PC e da ESP, cache de endereço obsoleta,
  reinício de ambos, DHCP tardio, interfaces múltiplas e ESP ausente. Não exigir
  acesso ao roteador nos testes do usuário; em bancada, controlar DHCP isolado.
- Testar PCs em ambos os modos na mesma ESP, múltiplas ESPs e dual boot com
  um agent antigo, exibindo claramente preparo parcial.
- Testar instaladores em fixtures de Windows/Linux: confirmação, manifestos,
  preservação de BootOrder, backup, falha entre etapas, retomada e rollback.
- Executar testes nativos, suite de interface e build das variantes da ESP,
  agents Windows/Linux e imagens EFI. Usar QEMU/OVMF com rede controlada para
  descoberta e continuidade do boot antes de qualquer alteração física.
- Aceite físico separado: ESP recebe outro IP, agent reencontra sem novo
  pareamento, dashboard é localizada, Sinric envia WoL e iPXE inicia o sistema
  escolhido. Repetir com PC/ESP reiniciados. Reportar validação sintética e
  física separadamente; não declarar sucesso de hardware por fixtures.

## Ordem de implementação proposta

1. Congelar protocolo/identidade e vetores; implementar ESP e prova de conceito
   de `rbdiscover` em QEMU, antes de depender disso na interface do usuário.
2. Integrar descoberta ao agent/pareamento e localizador de dashboard.
3. Acrescentar opção por PC, compatibilidade e instruções de conversão.
4. Integrar instaladores, manifestos, backups e rollback nos dois sistemas.
5. Validar falhas/trocas DHCP em bancada e só então oferecer teste físico
   assistido, com preservação das configurações e do boot funcional.

## Referências técnicas verificadas

- iPXE no commit usado pelo projeto:
  [API UDP](https://github.com/ipxe/ipxe/blob/7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1/src/include/ipxe/udp.h)
  e [verificação ECDSA](https://github.com/ipxe/ipxe/blob/7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1/src/crypto/ecdsa.c).
- Pontos locais centrais: configuração/validação do agent em
  `agent/native/AgentIdentity.cs`, script `ipxe/remote-boot.ipxe.in` e cadastro
  de PC em `firmware/web/app.js`. Os instaladores atualmente conferem IPv4,
  PC ID e hashes e já oferecem backup e teste explícito por BootNext.
