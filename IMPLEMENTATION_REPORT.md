# IMPLEMENTATION REPORT — Remote Boot V2

Data: 2026-09-08. Estado: **implementação experimental compilada e testada em software**. Não há validação em ESP32 física, UEFI real, Windows real ou conta Sinric nesta execução. Os critérios de aceitação que dependem desses ambientes permanecem abertos; a V2 não é certificada para produção.

## Entrega

`esp32-remote-boot-repo.zip`: repositório de código completo, com README, firmware, UI, app EFI, builder iPXE, installers, agents, testes, workflows e documentação. `IMPLEMENTATION_REPORT.md`: este relatório, também incluído no ZIP.

O ZIP não contém toolchains, caches, binários pré-compilados nem o snapshot privado. O build gera os binários. Não houve push, criação de repositório remoto ou publicação no GitHub. A UKI privada, IP/MAC/UUIDs e logs pessoais não foram copiados.

## Implementado

- ESP32-C3 4 MB: setup AP com senha aleatória via serial, configuração sem editar código, NVS com schema versionado e recuperação que preserva dados.
- Dashboard HTML/CSS/Vanilla JS, gzip/PROGMEM; rede DHCP/estática, MAC/WoL, catálogo, visibilidade/ordem, padrão/fallback, TTL, comportamento de boot físico, Sinric, logs e manutenção.
- Catálogo de até 24 Boot IDs, default/last/pending separados, pending reutilizável em retries e confirmação por heartbeat correspondente.
- API com tokens admin/agent separados; redaction de credenciais; confirmação de reset, force WoL e reboot; PC online resulta em 409 no boot comum.
- WoL broadcast calculado pela subnet, repetições/intervalo e cooldown; heartbeat e comando de reboot com ack e proteção contra repetição.
- Sinric opcional com até 8 Switch slots; IDs/credenciais configuráveis. Sem criação inventada de dispositivos no portal e sem promessa de gratuidade de slots.
- UEFI Boot#### genérico: limites/offsets do EFI_LOAD_OPTION, Device Path, OptionalData preservado, expansão de HD() por assinatura única, fallback único e bloqueio de recursão.
- iPXE local com script e RemoteBoot.efi embutidos. Conferido que os bytes exatos do novo EFI e do script inicial estão no binário iPXE compilado.
- Installer Linux com descoberta, build, detecção/validação de ESP, backups, create-only, BootNext, instalação opcional de agent e promoção separada confirmada.
- Installer Windows com API firmware documentada, serialização GPT Device Path, backup BCD/variáveis, validação de PE, seleção da ESP, BootNext e promoção confirmada. Agent via Scheduled Task.
- Helper UKI CachyOS opcional, documentação de recuperação, workflows CI/release e scanner simples de secrets.

## Builds executados

| Componente | Resultado | Evidência |
|---|---|---|
| ESP32-C3 | PASS | PlatformIO 6.1.18, espressif32 6.10.0, Arduino-ESP32 2.0.17 |
| firmware.bin | PASS | **1,027,760 bytes**, 24.89% da APP |
| Partição APP | PASS | **4.128.768 bytes** (`0x3F0000`), offset `0x10000`, flash total 4 MiB, sem OTA |
| RAM estática | PASS | 44.108 de 327.680 bytes; isso não mede pico de heap em runtime |
| RemoteBoot.efi | PASS | 58,781 bytes, PE32+ EFI application x86_64 |
| iPXE EFI | PASS | 1,233,920 bytes; build de teste com IPv4 de documentação |
| Gate de tamanho | PASS | Limite de 90% APP, resultado 24.89% |

GNU-EFI construído localmente do commit `f56c54402de23349bd658dc91eb508d7732a9970`. iPXE fixado no commit de referência `7ada3e0f04d0526747df5e0e46c05c94cbf7a7d1`. Sinric 3.3.1 fixado em `2ad7bffd59271148503dac7f22f53b33aad499f4`; WebSockets 2.6.1 em `7a4c416082b80dfc2c0da10731effc8d3a69abc6`; ArduinoJson 7.3.1. Hashes e tamanhos em `docs/build-validation.json`.

Houve erros/timeouts em mirrors durante a instalação de dependências. A compilação final passou usando versões/commits fixados. Warnings de depreciação de containsKey vêm do SDK Sinric; não impediram o build. A instalação de pacotes de sistema estava indisponível no ambiente, então GNU-EFI/ShellCheck/PowerShell foram preparados localmente.

## Testes executados

| Teste | Resultado / alcance |
|---|---|
| Estado de firmware | PASS: catálogo, IDs, pending TTL/retries, fallback inválido após rescan, heartbeat, online/409, force, wraparound de millis |
| Autenticação | PASS: comparação de tokens e comprimentos; HTTP real não exercitado |
| Configuração | PASS: migração schema 1→2, TTL existente preservado, schema futuro recusado sem apagamento, redaction sem alterar original |
| EFI parser | PASS: fixtures sintéticas de Windows, shim/GRUB, Limine, systemd-boot e UKI; limites, OptionalData binário, paths truncados e bloqueio iPXE/RemoteBoot |
| Buffers malformados | PASS: 20.000 buffers sintéticos sob AddressSanitizer/UBSan |
| Linux discovery | PASS: função real com efibootmgr mockado, ordenação BootOrder, IDs duplicados, ocultação network/USB e bloqueio de recursão |
| Bash | PASS: sintaxe e ShellCheck 0.10.0 sem achados |
| PowerShell/C# | PASS: parser de todos os PS1 e compilação/teste de serialization/parser C# usando PowerShell 7.4.7 no Linux |
| JavaScript | PASS: syntax check via Node; sem simulação completa do navegador/ESP32 |
| Documentação | PASS: links relativos existentes; revisão das instruções de instalação/recuperação |
| Secrets/privacidade | PASS: scanner simples + comparação de 27 identificadores privados distintos do snapshot, zero correspondências nos arquivos públicos revisados |

LeakSanitizer não pôde acessar `/proc` neste ambiente; o teste nativo foi executado com `ASAN_OPTIONS=detect_leaks=0`, mantendo AddressSanitizer e UndefinedBehaviorSanitizer. Isso não certifica ausência de leaks.

## Limitações e requisitos ainda não validados

1. **Hardware e integração real:** onboarding, Wi-Fi, NVS física, WoL, API HTTP, cloud Sinric, BootNext, execução de loaders e reboot de OS não foram testados fisicamente. As fixtures de nomes de loaders não comprovam boot desses sistemas.
2. **Installers:** sintaxe, ShellCheck e C# foram testados; backup/ESP/UEFI, rerun e rollback não foram exercitados em hosts reais ou VMs. Não há rollback transacional completo de interrupção no meio da instalação. BootOrder anterior é preservado e backups são fornecidos.
3. **Windows build:** o installer é nativo PowerShell; o binário iPXE/EFI é construído em Linux/WSL2. Não há toolchain GNU-EFI nativo Windows empacotado.
4. **Descoberta Windows:** ciclo padrão enumera BootOrder; `-FullScan` no installer pesquisa também entradas fora da ordem. O agent periódico não executa essa varredura completa. Entradas fora de BootOrder podem precisar ser incluídas na ordem pelo administrador ou ressincronizadas após scan completo.
5. **Device Paths:** paths completos e HD() com assinatura única são tratados; expansão completa de URI/USB/File-only/multi-instance não está implementada. Sem suporte universal a toda classe de entry UEFI. Instaladores automatizados exigem ESP GPT.
6. **Timeouts:** DHCP e progresso HTTP têm timeout finito configurável no builder, mas não há deadline absoluto contra tráfego que continue avançando. A seleção inicial usa net0. Timeout não aborta loader/kernel já iniciado. Fallback cobre somente retorno de erro de LoadImage/StartImage.
7. **Pending:** RAM monotônica; reboot da ESP32 descarta pending, preservando padrão/última escolha. Não há persistência do deadline entre reinicializações. BootCurrent só é publicado quando confiável; sem ID de heartbeat, TTL encerra pending.
8. **UKI:** helper específico, opcional e experimental. Requer cmdline local correta e regeneração após kernel; não instala preset/hook automático. O sucesso da UKI V1 não valida o helper V2.
9. **Segurança:** HTTP LAN sem TLS; boot.ipxe não autenticado; NVS sem criptografia; profile sem Secure Boot. Leia SECURITY.md. Testes não são auditoria completa.
10. **CI/release:** workflows incluídos; não executados no GitHub, portanto não afirmo “CI verde”. O workflow de tag gera artifacts, não publica automaticamente um GitHub Release. Os binários gerados dependem das licenças dos componentes; veja THIRD_PARTY.md.
11. **UI:** configuração existe, mas não houve teste de interação em navegador conectado à ESP32. Após rescan, reconecte na dashboard para recarregar o catálogo. O botão físico configurado é o do PC, não GPIO da ESP32.

## Primeiro teste exato

Na raiz extraída, execute `pio run -e esp32c3_4mb -t upload` com a ESP32 conectada; depois `pio device monitor -b 115200`. Use a senha exibida para entrar no AP e como token do primeiro acesso em `http://192.168.4.1`. Configure Wi-Fi/MAC e tokens diferentes. Execute o installer do OS, configure padrão/fallback na dashboard e escolha **TEST**, mantendo BootOrder original. Reinicie manualmente e confirme o loader padrão antes de promover a entrada. Procedimento completo em `docs/hardware-test.md`.

## Fontes técnicas consultadas

- [UEFI Boot Manager / EFI_LOAD_OPTION](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html)
- [UEFI Loaded Image](https://uefi.org/specs/UEFI/2.10/09_Protocols_EFI_Loaded_Image.html)
- [iPXE ifconf](https://ipxe.org/cmd/ifconf), [imgfetch](https://ipxe.org/cmd/imgfetch), [embed](https://ipxe.org/embed)
- [Microsoft firmware read](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfirmwareenvironmentvariableexw), [firmware write](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfirmwareenvironmentvariableexw)
- [Sinric SDK](https://github.com/sinricpro/esp8266-esp32-sdk/tree/3.3.1), [efibootmgr](https://github.com/rhboot/efibootmgr)
