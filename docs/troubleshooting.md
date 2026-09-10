# Troubleshooting

| Sintoma | Verificação/ação |
|---|---|
| ESP32 sem aparecer no USB | Cabo de dados, porta, driver USB e modo de download da placa; o profile usa USB CDC C3. Não aguarda serial conectado para operar. |
| Setup não abre | Veja senha do AP no monitor serial; acesse diretamente 192.168.4.1. Não depende do popup de captive portal. |
| Wi-Fi não conecta | SSID 2,4 GHz/credenciais, sinal e DHCP. Após timeout aparece AP recuperável; configuração antiga exige token existente. |
| SCHEMA_LOCKED | Flash preservada; volte para versão compatível ou restaure explicitamente. Não há apagamento automático. |
| 401/403 | Token admin/agent correto, 24–128 caracteres permitidos; confira qual permissão a rota aceita. |
| INVALID_CONFIG | Confira MAC unicast, limites WoL/TTL, IDs existentes e tokens diferentes. Nomes até 63 bytes UTF-8. |
| CATALOG_LIMIT | Mais de 24 entradas; remova entradas obsoletas com cuidado no host. Não ocorre truncamento silencioso. |
| PC parece online incorretamente | Status representa heartbeat recebido. Confira que só o PC correto usa o token do agent. |
| PC não liga | Fonte independente ESP32, NIC Ethernet com energia, WoL no último OS desligado, ErP, Fast Startup, broadcast/isolamento do AP. |
| 409 PC_ALREADY_ON | Boot normal não reinicia. Use reboot explícito com agent ou force somente para reenviar WoL. |
| iPXE não obtém rede | Confirme cabo, DHCP, driver iPXE, net0 e VLAN. Builder usa primeira NIC; setups multi-NIC precisam ajuste. |
| iPXE não encontra ESP32 | IP embutido deve ser o IP reservado correto. Rebuild após mudança. Teste GET /boot.ipxe na LAN. |
| EFI NOT_FOUND | Entry/ESP/assinatura/path válidos? Short forms fora de HD() não têm expansão integral. Escolha entry completa/compatível. |
| EFI ACCESS_DENIED | Entrada inativa, bloqueada, path inválido ou Secure Boot. Não desative bloqueios de recursão para contornar. |
| Seleção GRUB mostra menu | Boot#### seleciona o loader, não item interno. Configure o loader ou use UKI opcional. |
| Kernel travou após iniciar | Fallback do EFI só cobre retorno de erro de LoadImage/StartImage, não falha tardia do OS. |
| Firmware volta ao menu/shell | Continuação após exit depende da UEFI. Teste BootNext e ordem original; não promova até validar. |
| Agent Windows não roda | Veja Scheduled Task, histórico, política de execução e permissões. Não foi adicionada exceção automática de política. |
| Build iPXE falha | GCC/binutils/GNU-EFI/Perl/liblzma; commit fixado. Confira EFILIB e CRT0 no Makefile para sua distro. |
| Build firmware excede espaço | Use profile 4 MB fornecido, sem OTA. Não reduza o gate para ocultar overflow. |
| Build precisa de internet | Instale dependências PlatformIO/Git previamente para modo offline; o ZIP não inclui toolchains completos. |

Para diagnóstico, salve logs sem tokens. `tests/api_smoke.py` realiza apenas leituras quando executado com RB_URL/RB_ADMIN_TOKEN. Não publique backups pessoais de firmware.


### RemoteBoot-XXXX não aparece

Abra `pio device monitor` em 115200 e pressione RESET para capturar o boot completo.
O setup usa **WIFI_AP**, canal **1**, SSID visível e até **4 clientes**.
Não precisa de STA: Wi-Fi salvo é aplicado em modo STA no próximo boot.
O mesmo modo AP é usado na recuperação após perda prolongada da LAN; as configurações salvas não são apagadas.

Confira `SoftAP start`, `AP IP`, `AP MAC`, `WiFi mode`, `Channel` e `Connected stations`.
O log também informa chip, revision e tamanho da flash. Diagnósticos sem senha se repetem a cada 10 s durante setup.

| Resultado | Interpretação / ação |
| --- | --- |
| `SoftAP start: FAILED` | A chamada falhou, ou não foi possível selecionar modo AP. São no máximo 3 tentativas totais, com rádio desligado entre falhas. |
| `ERROR: SETUP_AP_INVALID_IP` | O AP retornou sucesso, mas IP é `0.0.0.0`; essa tentativa é descartada e não inicia DNS. |
| `ERROR: SETUP_AP_FAILED` | Todas as tentativas falharam; sem `SETUP_AP_STARTED`, servidor de setup ou `READY`. O loop permanece responsivo; pressione RESET para tentar novamente. |
| `SETUP_AP_STARTED`, IP válido, rede ausente | A API confirmou AP/IP, mas isso não comprova transmissão de beacons. Faça scan em outro dispositivo com Wi-Fi 2,4 GHz e colete os dois resultados. |
| `ERROR: SETUP_DNS_FAILED` | AP válido, DNS indisponível; conecte e abra diretamente `http://192.168.4.1` (ou o IP exibido). |
| Clientes = 0 | Nenhum cliente associado no instante da leitura; não comprova defeito de rádio. |

Se falhar: reinicie a placa; verifique alimentação; teste outra porta/cabo USB;
confirme a placa física e o ambiente `esp32c3_4mb` antes de alterar qualquer parâmetro.
Registre o log completo, modelo real da placa, fonte/cabo e resultado do scan.
Remova **Password/token** do log antes de compartilhar. Não publique credenciais.
Não conclua que o rádio está defeituoso somente pelo retorno da API.

Depois de um reset o token aleatório muda; use a senha do boot atual.
Não altere board, framework, versões ou parâmetros ao acaso antes de coletar o log.
Siga também [os testes de hardware](hardware-test.md).
