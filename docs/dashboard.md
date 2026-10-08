# Dashboard multi-PC

A dashboard usa API v2 e mantém a seleção de PC em cada aba. Cadastre até quatro PCs em **Adicionar PC**, informando nome e MAC da interface Ethernet. O seletor e os cards indicam o destino das ações. Boot, Wake-on-LAN, reinício, desligamento e descoberta usam o identificador desse PC; nomes e Boot IDs repetidos em PCs diferentes não alteram o destino.

Em **Configuração**, **Salvar ajustes do PC** grava identificação, WoL, comportamento de boot e preferências de exibição do PC selecionado. **Salvar alterações** grava somente configuração global de rede, senha administrativa e Tailscale. Senhas vazias mantêm o valor salvo. Respostas de um PC anterior são canceladas ou descartadas ao trocar a seleção; respostas do catálogo não substituem um nome que esteja sendo editado.

Para vincular uma instalação Windows/Linux, abra **Parear agent**, execute o instalador e digite o código de oito caracteres mostrado por ele. Confira hostname e OS no computador, escolha um PC existente ou **Criar novo PC**, informe um nome para a instalação e marque a confirmação explícita. O código expira em cinco minutos. Cancelar fecha a janela e cancela a solicitação consultada. A dashboard recebe somente identificadores e metadados: nenhuma credencial longa do agent é copiada ou armazenada no navegador.

**Sistema → Agents vinculados** mostra cada instalação, PC, OS, permissões e presença reportada. Aprovação mostra **Aguardando conexão do agent** até o hello autenticado. Se a ESP reiniciar após a aprovação e antes de o instalador receber a credencial, remova o vínculo que não conectou e repita o pareamento. **Remover vínculo** revoga apenas essa instalação; **Remover PC** revoga suas instalações e limpa as referências Sinric.

Sinric controla um único PC escolhido no próprio painel. Mudar esse PC exige confirmação e limpa todos os slots antes de salvar; escolha novamente os sistemas no catálogo do novo PC. Remover o PC escolhido desativa a integração e não escolhe outro PC automaticamente. Sem catálogo, conecte um agent e solicite sincronização antes de mapear sistemas.

## Verificação automatizada

`npm ci` e `npx playwright install chromium firefox webkit` preparam os browsers. `npm run test:ui` executa interações reais em Chromium, Firefox e WebKit nas larguras 360, 390, 768 e 1280 px com uma API determinística interceptada. Os testes cobrem cadastro, seleção, resposta atrasada, salvamento por PC, aprovação explícita, cancelamento, expiração, falha NVS, isolamento de abas, Sinric, foco e ausência de overflow. Traces de falha ficam em `test-results/`, ignorado pelo Git. As fixtures nunca enviam comandos a hardware real.

`node tests/test_dashboard_model.js`, os testes JS de validação/polling/Tailscale e `python tests/test_web_asset.py` verificam políticas e o documento gzip único e determinístico. Os módulos locais são incorporados na ordem `pc-model.js`, `pairing-ui.js`, `app.js`, sem dependências de CDN.

Validação de software em 2026-10-08: **204/204 interações Playwright passaram** nos três browsers e quatro larguras (1,5 min), incluindo criação de PC durante o pareamento, troca de credencial administrativa lembrada e reinício de paginação quando a geração muda. Os testes JS de modelo, validação, polling e Tailscale, estrutura Python e asset determinístico também passaram. A inspeção visual preservou a apresentação do painel. Esses resultados usam fixtures; presença e energia em PCs reais dependem da validação de hardware.
