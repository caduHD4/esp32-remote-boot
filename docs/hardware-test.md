# Primeiro teste em hardware

1. Preserve a ordem UEFI original e confirme que o loader atual inicia diretamente pelo menu UEFI.
2. Grave a ESP32 pelo PlatformIO; configure AP/Wi-Fi/MAC/tokens e mantenha alimentação USB independente do PC desligado.
3. Execute o installer no host, sincronize catálogo e configure padrão + fallback válidos na dashboard. Não promova a entrada.
4. Marque TEST no installer para BootNext. Reinicie manualmente, acompanhe iPXE e confira o sistema padrão. Se falhar, use menu UEFI para o loader anterior.
5. Instale agent nesse OS e confira heartbeat/OS na dashboard. Repita nos demais OS desejados.
6. Desligue o PC; aguarde status offline (45 s). Selecione outro sistema na dashboard e confirme WoL + boot escolhido.
7. Repita para Windows, GRUB/shim e UKI somente se existentes. Nome do loader não comprova compatibilidade sem boot real.
8. Teste ESP32 offline/cabo desconectado: confirme timeout e recuperação pelo firmware. Teste ID obsoleto controladamente e fallback sem remover seu loader funcional.
9. Com PC ligado, boot comum deve retornar 409. Reboot exige habilitação do agent e confirmação explícita; salve trabalho antes.
10. Só após os testes bem-sucedidos execute promote. Guarde resultado, versão do firmware, placa, UEFI e NIC num relatório local privado.

Estes testes **não foram executados pelo ambiente de desenvolvimento**. Não considere a V2 validada em hardware pelo sucesso do setup V1.


## Test 1 — First-run SoftAP

Status: **pendente de execução física**. Preencha resultado e log por placa:
ESP32-C3 SuperMini, ESP32-C3 DevKitM-1 e outros ESP32-C3 de 4 MB disponíveis.

1. Use placa sem configuração ou execute o factory reset existente. Se optar por apagar flash/NVS, preserve antes as configurações que precisar: a operação apaga credenciais.
2. Grave `pio run -e esp32c3_4mb -t upload` sem mudar board, framework ou versões.
3. Abra `pio device monitor` em 115200; pressione RESET se perdeu o início do log.
4. Confirme `SETUP_AP_STARTED`, `SoftAP start: OK`, IP `192.168.4.1` ou equivalente válido, SSID correto, modo AP, canal 1 e MAC válido.
5. Em outro dispositivo, faça scan Wi-Fi 2,4 GHz.
6. Confirme presença de `RemoteBoot-XXXX`; anote se outro scanner também vê a rede.
7. Conecte com o token exibido neste boot; confirme aumento de `Connected stations` no diagnóstico de 10 s.
8. Abra `http://192.168.4.1` (ou o IP exibido).
9. Confirme carregamento da dashboard e autenticação com o token inicial.

## Falhas e limite de tentativas

A política pode ser exercitada sem rádio com `bash tests/run.sh` (test_setup_ap.cpp).
Para injeção em hardware, use uma cópia local de teste do adapter: force retorno false
em `startAp`, IP zero em `hasValidIp`, ou false em `startDns`, um cenário por vez.
Não publique nem use essas alterações no firmware de operação; restaure e regrave após testar.

| Caso | Resultado esperado |
| --- | --- |
| SoftAP start failure nas 3 tentativas | 3 tentativas totais (inicial + 2 retries), `SETUP_AP_FAILED`, sem STARTED/DNS/Setup ready/READY. |
| Invalid IP nas 3 tentativas | `SETUP_AP_INVALID_IP` por tentativa, depois FAILED; sem DNS ou servidor de setup. |
| Falha inicial e sucesso no retry 2 ou 3 | STARTED somente no sucesso com IP válido; DNS chamado uma vez. |
| DNS falha com AP válido | `SETUP_DNS_FAILED`; dashboard pelo IP, mensagem de disponibilidade limitada. |
| Restart during setup / RESET | Novo boot, token novo, AP reaparece sem configuração salva. |
| Power cycle | Desligar/religar alimentação; repetir scan, conexão e dashboard. |
| USB reconnect | Desconectar/reconectar USB; reabrir porta serial se necessário; repetir scan e conexão. |
| Software restart | Usar reinício da ESP32 na dashboard/API existente; repetir scan e autenticação com token novo. |

Repita reset, power cycle e software restart pelo menos 3 vezes cada sem configuração salva.
Após falha terminal, confirme diagnósticos periódicos e ausência de retries infinitos ou reboot automático.
Teste também fallback após perda da LAN por mais de 60 s: sucesso oferece dashboard;
falha não anuncia READY nem atende setup; configuração NVS deve permanecer preservada.

## Persistência e regressão

1. Salve Wi-Fi e demais campos obrigatórios/tokens pela dashboard.
2. Reinicie; com a LAN disponível, confirme que não entra em Setup AP.
3. Confirme associação à LAN e acesso à dashboard no IP LAN.
4. Confirme configurações e tokens preservados após power cycle.
5. Execute os testes anteriores de WoL, Sinric Pro, WebSocket agent, BootNext,
   UEFI discovery, shutdown e reboot em Windows/Linux. Registre cada resultado;
   testes host não substituem essa verificação integrada.

Registro: commit, placa, chip/revision/flash, alimentação, ação, tentativa,
SSID/IP/MAC/canal, scan em cada cliente, dashboard, resultado e log **sem token**.
