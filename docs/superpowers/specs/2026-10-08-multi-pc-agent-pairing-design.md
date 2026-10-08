# Multiplos PCs e pareamento de agents

## Objetivo e base

Branch: `feat/multi-pc-agent-pairing`, criada de `01c9cce` em `feat/no-ap-reliable-wifi`. Essa base foi validada na ESP32-C3 com MicroLink/Tailscale, cadastro de senha administrativa e acesso HTTP pela tailnet. O usuario confirmou o funcionamento. Esta entrega e planejamento; nao muda o firmware em execucao.

O usuario quer adicionar PCs na dashboard, instalar agents Windows/Linux sem copiar uma credencial longa e sincronizar por um codigo curto confirmado no painel. A instalacao sera do zero, com reset; nao desenvolver migracao do schema antigo. A senha administrativa continua sendo cadastrada na primeira abertura.

## Escolhas e limites

- Meta de capacidade: 4 PCs, 8 instalacoes de agent no total, ate 24 entradas UEFI por PC e 8 slots Sinric globais. Validar a meta no hardware antes de anunciar suporte.
- Um PC fisico pode ter agents Windows e Linux distintos, associados ao mesmo `pc_id`; apenas uma sessao ativa por PC. VMs ou maquinas simultaneas devem ser PCs separados.
- `pc_id` e `agent_id`: identificadores aleatorios de 128 bits, 32 caracteres hexadecimais minusculos. Nomes e hostname nao definem identidade.
- Schema 3 e protocolo `/api/v2`; clientes antigos recebem erro claro de versao incompativel. Sem compatibilidade silenciosa ou rotas com PC implicito.
- Firmware com Tailscale exige `esp32c3_4mb_microlink`, PlatformIO Core 6.1.19. A variante padrao continua existindo, explicitamente sem Tailscale.
- Mantem confirmacao e opt-in local para reboot/shutdown, TTL da fila 30 s, validade de ACK no agent 15 s, presenca WebSocket 90 s e keepalive 60 s.
- Nenhum comando de teste automatico pode desligar, reiniciar ou alterar UEFI do computador que executa os testes; usar FakeHost e fixtures. Testes fisicos de energia exigem acao deliberada em PCs de teste.

## Modelo e isolamento

Configuracao global: Wi-Fi embutido, senha admin, Tailscale e credenciais Sinric. O schema 3 aceita zero PCs: cadastro inicial salva globals com senha e listas vazias pelo ConfigStore; nao depende de MAC ou token global de agent, nem da chave intermediaria initial_admin do schema anterior. Cada PC guarda nome, MAC Ethernet, parametros WoL, politica de boot, catalogo UEFI, padrao e fallback. Agents guardam `agent_id`, `pc_id`, nome da instalacao/OS, credencial individual e estado revogado. Segredos ficam fora das respostas de configuracao e logs.

Estado RAM por PC: presenca, OS ativo, sessao, fila de energia, selecao pendente, cooldown WoL e guard de dispatch. Nao compartilhar pending, ACK, catalogo ou descoberta entre PCs. Um token de agent resolve sua identidade no servidor; `pc_id` enviado pelo cliente nunca concede acesso. Agent nao possui permissao administrativa.

Catalogo pertence ao PC fisico. Windows e Linux no mesmo PC sincronizam o mesmo conjunto de entradas UEFI locais. Sync exige agent autenticado e sua sessao ativa; preservar nomes personalizados/visibilidade e reconciliar padrao/fallback apenas nesse PC. Boot ID pode se repetir entre PCs sem colisao.

MAC e nome sao editaveis, mas MAC duplicado entre PCs e invalido. Instalador lista interfaces Ethernet para selecao; nao escolhe automaticamente VPN, Tailscale, Wi-Fi, adaptador virtual ou MAC aleatorio. Cadastro manual e fallback quando deteccao nao for confiavel.

## Pareamento

1. Admin abre area Agents, clica Sincronizar agent e abre janela de 5 minutos.
2. Instalador recebe IP LAN ou Tailscale da ESP; inicia `--pair --url URL`, informa hostname/OS e candidatos de interface Ethernet.
3. ESP, apenas com janela aberta, cria `pairing_id`, segredo de dispositivo de 256 bits e codigo de 8 caracteres Crockford, exibido `ABCD-EFGH`. Nao usar senha admin no instalador.
4. Instalador mostra codigo e aguarda. Poll autenticado pelo segredo de dispositivo a cada 2 s, sem redirects/proxy; TTL maximo 5 minutos.
5. Admin digita codigo, confere hostname/OS/MAC nao confiaveis e escolhe PC existente ou cria um PC. Confirmacao explicita e obrigatoria; nao aprovar apenas por hostname.
6. ESP gera credencial individual de 256 bits, grava vinculo em NVS e autoriza a entrega somente ao segredo da solicitacao correspondente. Dashboard recebe apenas identificadores e estado.
7. Entrega tolera perda de resposta: retornar a mesma credencial ao mesmo segredo ate confirmacao ou TTL. Agent grava arquivo atomicamente, testa hello, envia confirmacao; entao ESP descarta a entrega temporaria. Token permanente continua valido.
8. Installer habilita servico/tarefa apenas depois de salvar configuracao com protecoes e validar conexao. Se falhar, exibe motivo e oferece repetir; nao deixa um servico falsamente instalado.

Janela e solicitacoes residem em RAM, somem ao reboot. Se a ESP reiniciar depois da aprovacao persistida mas antes de entregar o token, mostrar vinculo nao conectado na dashboard; o admin remove esse vinculo e repete o pareamento. Nao recriar vinculo nem reenviar segredo automaticamente por hostname. Esse caso deve ser um erro recuperavel explicito no instalador. Maximo 3 pendentes; iniciar pareamento limitado globalmente a 6 pedidos/minuto. Consulta por codigo na area admin: maximo 5 tentativas/minuto por janela, bloqueio 60 s ao exceder. Erros nao revelam se codigo parcial existe. Expiracao por diferenca unsigned de millis para suportar wrap. Codigos e IDs sem colisao entre ativos; segredo nunca no URL, argv, logs ou armazenamento do navegador. Nao persistir codigos.

Se confirmacao chegou mas NVS falhou, nao aprovar nem entregar credencial. Remover/revogar agent invalida token e desconecta sessao imediatamente; nao apagar PC ou outro OS. Remover PC requer confirmacao, revoga agents e limpa filas/referencias Sinric desse PC, persistindo tudo antes de atualizar UI. Credencial de agent nunca substitui senha admin. A rede HTTP/WS permanece com o modelo atual LAN/Tailscale; codigo curto e aprovacao administrativa nao tornam HTTP publico seguro.

## Interface

Visao geral com cards por PC, botao Adicionar PC e selecao de PC visivel. Boot, configuracoes de PC e estado usam identificador capturado no inicio da acao. Troca de PC cancela requests antigos ou descarta respostas pela identidade/geracao. Confirmacoes mostram nome e acao; nao executar comando no PC selecionado posteriormente.

Area Agents mostra PC, OS, conectado/desconectado/revogado, ultimo contato, permissoes reportadas, Sincronizar agent e Remover vinculo. Dialog de pareamento com codigo, detalhes detectados, PC destino, timer e cancelar. Tratar vazio, limite, erro, expiracao, offline e sucesso; foco e erros acessiveis; nenhum token longo para copiar.

Sinric controla somente um PC escolhido globalmente em `sinric_pc_id`. Todos os slots referenciam acoes/sistemas desse PC. Trocar o PC limpa os mapeamentos anteriores e exige confirmacao e novo mapeamento; remover o PC escolhido desativa Sinric e limpa os slots. Os demais PCs continuam controlaveis pela dashboard. Nao existe fallback silencioso para outro PC. Wi-Fi/Tailscale/admin ficam numa area global distinta das configuracoes do PC.

## Boot e instalacao EFI

Cada binario iPXE incorpora `pc_id` e chama `/boot/<pc_id>.ipxe`; sem ID ou PC removido retorna script de saida segura, sem consumir pending alheio. PC ID e publico e nao concede privilegios admin/agent. A rota mantem a politica publica de boot na LAN; nao tratar IP/MAC como autenticacao criptografica.

Build iPXE exige PC ID; produz manifesto com ESP, PC ID e SHA256. Installer verifica o manifesto e recusa binario de outro PC. Instalar agent continua sem alterar EFI. Instalacao EFI e passo separado que usa o vinculo salvo, valida o PC e preserva BootOrder/backups. Sem scripts genericos que possam selecionar pending global.

## Memoria, persistencia e observabilidade

Colecoes limitadas e validacao antes de alocar; maximo 12.000 bytes por request/frame e resposta paginada para catalogos grandes, sem bootstrap completo com 96 entradas. Sessao WebSocket limitada a 4 PCs autenticados + 1 hello pendente. Sincronizacao de catalogo por PC; NVS ampliada para 64 KiB na nova instalacao: nvs em 0x9000 tamanho 0x10000, phy_init em 0x19000 tamanho 0x1000 e factory em 0x20000 tamanho 0x3E0000. Persistir snapshot schema 3 em blobs putBytes, nunca putString (limite de string NVS); maximo serializado 20.000 bytes. Dois bancos com geracao, comprimento e CRC, mais seletor ativo atomico; escrever e reler banco inativo antes de atualizar seletor. Validar recuperacao apos perda de energia e falta de espaco. A mudanca de particoes exige instalacao limpa ja autorizada para a implementacao futura.

Medir heap livre, menor heap e maior bloco no baseline e com 4 PCs, Tailscale e Sinric. Em regime estavel preservar pelo menos 60.000 bytes livres e bloco de 24.000 bytes quando admitir TLS, coerente com `ml_tls_admit`; nao diminuir guardas para fazer teste passar. Se meta nao couber, otimizar buffers/paginacao e repetir; documentar limite real antes de entregar. Soak de 2 h sem reset/watchdog e sem queda monotona de memoria em estado equivalente.

Logs usam pc_id/agent_id, codigo de evento e motivo; sem tokens, senha ou Auth Key. Status Tailscale global distingue built/configured/control online/trafego autenticado. Strings de snapshots JSON devem ser copiadas antes de sair do escopo.

## Conclusao e escopo

Implementacao futura deve passar os testes descritos no plano e na matriz de validacao. Nao promete ausencia absoluta de erros. Registro de resultados separa testes de simulacao, hardware LAN, Tailscale e energia real. Plano nao inclui executar alteracoes de UEFI/energia automaticamente, migracao de dados antigos ou novos recursos de acesso publico.
