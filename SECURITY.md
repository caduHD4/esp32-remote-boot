# Segurança

Use em uma LAN confiável. A dashboard e a API usam Bearer token sobre HTTP sem TLS; quem puder observar o tráfego pode capturar credenciais ou alterar respostas. Não encaminhe portas da ESP32 para a internet. Sinric é uma integração cloud opcional.

## Identidade e autorização

- O token admin autoriza configuração global, gerenciamento de PCs/pairing, ações de boot e reset. Mantenha-o fora de código e logs.
- Não há token global de agent. Cada agent recebe um token aleatório próprio ao ser aprovado para um PC. O token permite sincronizar o catálogo vinculado e autenticar WebSocket; o servidor deriva o PC do vínculo e verifica `agent_id`, `pc_id` e `session_id`. Reutilizar credencial de outro PC não concede acesso ao seu estado.
- O pareamento requer janela temporária aberta por admin e aprovação explícita associada a um PC. O código de usuário é curto e deve ser compartilhado apenas com quem está pareando; o segredo do dispositivo e token são credenciais. A entrega aprovada expira e o agent confirma a sessão após validar a configuração.
- `/boot/{pc_id}.ipxe` é público para permitir boot pré-OS. O script iPXE e os assets da dashboard também são públicos. O endpoint pode despachar o target pendente desse PC, então uma máquina na LAN pode interferir com o fluxo de boot. O script é entregue por HTTP e pode ser substituído em trânsito; Secure Boot e verificação criptográfica do iPXE não são implementados.

## Armazenamento e dados

- Schema 3 guarda snapshots em `bank0`/`bank1` no namespace NVS `remote-boot-v3`; cada snapshot tem geração, tamanho e CRC32, e `commit` seleciona o banco válido mais recente. O limite serializado é 20.000 bytes. O CRC detecta corrupção, não autentica conteúdo.
- NVS não é criptografada neste profile. Acesso físico à flash pode revelar Wi-Fi, credenciais Sinric, configuração de PCs e tokens dos agents. Proteja fisicamente o dispositivo e não compartilhe dumps de flash.
- Respostas de configuração omitem senhas e tokens, App Key e App Secret, retornando apenas indicadores de presença. A UI mantém o token admin em memória e não usa `localStorage`. Logs são limitados a 32 mensagens em RAM e não devem incluir credenciais.
- A API não libera CORS. Senhas de agent, token admin, tokens de agent e segredos Sinric não devem ser incluídos em commits, relatórios ou capturas.
- Reset de fábrica exige autenticação admin e confirmação `FACTORY_RESET`; apaga a configuração gerenciada e reinicia a ESP32. Uma atualização comum de firmware não apaga NVS.

## Boot e execução remota

Instaladores Windows/Linux exigem configuração protocol 2 do agent pareado. Antes de escrever na ESP, verificam que PC ID e URL do agent correspondem ao manifesto, e que SHA-256 de iPXE e loader coincide. O manifesto protege contra seleção acidental de artefatos de outro PC, mas não substitui uma assinatura confiável do firmware/loader. Proteja a origem dos artefatos.

Reboot e shutdown são comandos limitados do agent, sujeitos às permissões configuradas no PC. Não há execução arbitrária de shell/comandos fornecida pela API. `RemoteBoot.efi` bloqueia recursão conhecida por nome, caminho iPXE/RemoteBoot e protocolo marcador; isso não protege contra loader EFI malicioso ou comprometido.

Reporte vulnerabilidades sem publicar credenciais, tokens ou dados pessoais. O scanner de secrets do repositório é uma verificação complementar, não uma certificação de segurança. Antes de publicar, exclua `config.local.json`, backups EFI e binários gerados para sua máquina; consulte `.gitignore`.
