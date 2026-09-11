# Troubleshooting

| Sintoma | Verificação/ação |
|---|---|
| ESP32 sem aparecer no USB | Cabo de dados, porta, driver USB e modo de download da placa; o profile usa USB CDC C3. Não aguarda serial conectado para operar. |
| Setup não abre | Veja senha do AP no monitor serial; acesse diretamente 192.168.4.1. Não depende do popup de captive portal. |
| Wi-Fi não conecta | SSID 2,4 GHz/credenciais, sinal e DHCP. Após timeout aparece AP recuperável; configuração antiga exige token existente. |
| `WiFiUdp parsePacket(): could not receive data: 9` contínuo | Atualize para 2.1.1 ou superior. Builds 2.1.0 processavam o DNS mesmo no setup direto pela LAN. |
| SCHEMA_LOCKED | Flash preservada; volte para versão compatível ou restaure explicitamente. Não há apagamento automático. |
| 401/403 | Token admin/agent correto, 24–128 caracteres permitidos; confira qual permissão a rota aceita. |
| INVALID_CONFIG | Confira MAC unicast, limites WoL/TTL, IDs existentes e tokens diferentes. Nomes até 63 bytes UTF-8. |
| CATALOG_LIMIT | Mais de 24 entradas; remova entradas obsoletas com cuidado no host. Não ocorre truncamento silencioso. |
| PC parece online incorretamente | Status representa heartbeat recebido. Confira que só o PC correto usa o token do agent. |
| PC não liga | Fonte independente ESP32, NIC Ethernet com energia, WoL no último OS desligado, ErP, Fast Startup, broadcast/isolamento do AP. |
| 409 PC_ALREADY_ON | Boot normal não reinicia. Use reboot explícito com agent ou force somente para reenviar WoL. |
| iPXE não obtém rede | Confirme cabo, DHCP, driver iPXE, net0 e VLAN. Builder usa primeira NIC; setups multi-NIC precisam ajuste. |
| iPXE não encontra ESP32 | IP embutido deve ser o IP reservado correto. Rebuild após mudança. Teste GET /boot.ipxe na LAN. |
| `Cannot identify created entry` | Atualize o repositório. Builds anteriores não reconheciam saídas de `efibootmgr` que exibiam `HD(...)` após o rótulo. Antes de repetir, remova somente a entrada duplicada após comparar seu caminho com `efibootmgr -v`. |
| EFI NOT_FOUND | Entry/ESP/assinatura/path válidos? Short forms fora de HD() não têm expansão integral. Escolha entry completa/compatível. |
| EFI ACCESS_DENIED | Entrada inativa, bloqueada, path inválido ou Secure Boot. Não desative bloqueios de recursão para contornar. |
| Após selecionar, ocorre um segundo POST/reset | É esperado: o primeiro boot executa o iPXE; `RemoteBoot.efi` grava `BootNext` e reinicia; o firmware então inicia o alvo. |
| Após `BootNext`, volta ao Windows | Confirme o alvo com `efibootmgr -v` e teste `efibootmgr --bootnext ID`. A proteção de 60 segundos impede repetição imediata do mesmo alvo e permite que a UEFI continue o `BootOrder`. |
| Seleção GRUB mostra menu | Boot#### seleciona o loader, não item interno. Configure o loader ou use UKI opcional. |
| Kernel travou após iniciar | Fallback do EFI só cobre retorno de erro de LoadImage/StartImage, não falha tardia do OS. |
| Firmware volta ao menu/shell | Continuação após exit depende da UEFI. Teste BootNext e ordem original; não promova até validar. |
| Agent Windows não roda | Veja Scheduled Task, histórico, política de execução e permissões. Não foi adicionada exceção automática de política. |
| Build iPXE falha | GCC/binutils/GNU-EFI/Perl/liblzma; commit fixado. Confira EFILIB e CRT0 no Makefile para sua distro. |
| Build firmware excede espaço | Use profile 4 MB fornecido, sem OTA. Não reduza o gate para ocultar overflow. |
| Build precisa de internet | Instale dependências PlatformIO/Git previamente para modo offline; o ZIP não inclui toolchains completos. |

Para diagnóstico, salve logs sem tokens. `tests/api_smoke.py` realiza apenas leituras quando executado com RB_URL/RB_ADMIN_TOKEN. Não publique backups pessoais de firmware.
