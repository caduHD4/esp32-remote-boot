# Validação de Sinric, dashboard e Tailscale na placa

Esta sequência testa o funcionamento real. Testes nativos e CI validam código e
compilação, mas não substituem o teste de rádio, memória e tráfego na ESP32-C3.

## Atualização no CachyOS

Dentro do repositório e da branch `feat/microlink-tailscale-poc`:

```sh
git status --short
git pull --ff-only
pio run -e esp32c3_4mb_microlink -t upload
pio device monitor -b 115200
```

Se o Git informar conflito com alterações locais, preserve-as antes de continuar;
não use reset forçado. Mantenha seu arquivo local de configuração do MicroLink e
sua auth key fora do Git. Não é necessário apagar NVS ou gerar outra chave para
testar estas correções. Feche o monitor serial antes de fazer outro upload.

## 1. Verificar o caminho local

Abra `http://10.0.1.10/` (ou o IP LAN atual exibido pela placa). Confira a versão,
faça login e confirme que a lista de sistemas e o estado do computador aparecem.
O botão de desligar só deve habilitar com agent online e desligamento autorizado.

No navegador, a entrada autenticada usa `/api/v1/bootstrap`. Atualizações usam
`/api/v1/status`; a aba oculta pausa as consultas periódicas. O HTML público pode
responder `304` quando não mudou; configuração e status não devem ser armazenados
em cache. Uma transferência interrompida deve liberar a próxima tentativa.

## 2. Verificar os switches do Sinric

Confira em Sinric Pro se cada Device ID corresponde ao switch correto, e na
dashboard se ele aponta para um sistema existente. Um switch configurado como
"Padrão / seleção pendente" precisa de uma seleção pendente válida ou de um
sistema padrão configurado. Mostrar ONLINE não confirma que o comando foi aceito.

Com o computador desligado, teste um ON no site/app do Sinric. Observe os eventos
da dashboard e confirme o envio de WoL e o boot escolhido. O OFF é apenas o retorno
do botão momentâneo; não desliga o PC. Aguarde o retorno a OFF antes de repetir.
Depois teste o mesmo switch pelo Google Home.

Teste também uma rejeição esperada com o PC já online: ela não deve disparar um
segundo boot. Não use force como forma de esconder erro de configuração.

## 3. Verificar o caminho remoto de verdade

No Android, habilite Tailscale na mesma tailnet e desligue o Wi-Fi, usando dados
móveis. Acesse por HTTP o **IP VPN atual da ESP32**, não o IP LAN. No teste anterior
o IP VPN era `100.67.28.127`; confirme se continua sendo esse na dashboard.

Abra a página, faça login, navegue e faça algumas atualizações. Compare o estado
de coordenação/relay com a evidência de tráfego WireGuard: um cadastro online na
tailnet, isoladamente, não comprova acesso HTTP pelo túnel.

O estado "CONTROLE ONLINE" indica coordenação ativa, mas nenhum pacote IP
autenticado recente observado pelo firmware. Não é prova de que o túnel esteja
quebrado: faça uma tentativa de acesso. A API autenticada de status também expõe
`control_online`, `derp_online`, `derp_server_info`, `wg_encrypted_rx`,
`wg_authenticated_rx`, `authenticated_age_ms`, `reconnects` e `tls_deferred`.
Pacotes recebidos ainda criptografados não confirmam autenticação bem-sucedida.
Esses contadores são agregados da ESP32, não uma prova específica de acesso pelo
Android; por isso, o teste com dados móveis continua necessário.

Depois, mantenha a dashboard aberta e teste um comando Sinric com o PC desligado.
Verifique se ambos continuam funcionando e se a memória disponível se recupera
depois do carregamento. Repita uma vez após reiniciar a ESP32, sem limpar NVS.

## Se houver falha

Registre versão/commit, modo de acesso (LAN ou VPN, Wi-Fi ou dados móveis), horário
da tentativa, mensagem da dashboard, memória livre/maior bloco e trecho do serial
desde antes da tentativa até a falha. Informe se houve reset da placa ou somente
reconexão. Não compartilhe senha do setup, token administrativo ou auth key.

Não altere buffer, MTU, região DERP, credenciais e roteador ao mesmo tempo: isso
impede identificar qual mudança explica o resultado.
