# Segurança

Use somente numa LAN confiável. Dashboard/API usam Bearer token em HTTP, sem TLS: participantes com acesso ao tráfego podem capturar credenciais. Não exponha portas do ESP32 à internet. Sinric é a integração remota opcional.

- Token administrativo: leitura/configuração, WoL, reboot do PC, manutenção ESP32.
- Token do agent: somente catálogo, sincronização e heartbeat/comandos. Proteja o arquivo local com ACL/root; quem obtiver esse token poderá falsificar status e catálogo.
- `/boot.ipxe` e os assets da dashboard são públicos; não contêm credenciais. O script HTTP não é autenticado e sua substituição na rede é um risco de execução pré-boot. Secure Boot não é implementado por este projeto.
- Setup AP usa senha aleatória mostrada no serial. Após perda de Wi-Fi, a configuração continua exigindo o token administrativo existente. Reiniciar não apaga credenciais.
- NVS não é criptografada por este profile. Acesso físico à flash pode revelar credenciais.
- Logs são 32 mensagens em RAM; não incluem tokens, nomes de SSID, MAC ou payloads HTTP.
- Respostas de configuração omitem senhas, tokens, App Key e App Secret; retornam apenas flags de presença.
- A API não libera CORS. Tokens ficam somente em memória na página, sem localStorage.
- Restaurar exige token e `confirm=FACTORY_RESET`; schema desconhecido/corrompido é preservado e bloqueado.
- Reboot do PC exige confirmação explícita na UI e habilitação local do agent. Não há download/execução arbitrária de comandos pelo agent.
- O bloqueio de recursão UEFI usa nome, caminho iPXE/RemoteBoot e protocolo marcador na sessão. Um loader que chama código externo malicioso está fora desse limite de confiança.

O scanner simples de secrets é uma verificação complementar, não certificação de segurança. Antes de publicar, exclua configs locais, backups EFI, relatórios pessoais e binários gerados para sua máquina. Veja `.gitignore`.
