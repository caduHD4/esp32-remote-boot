# Baseline da branch de planejamento

Branch: `feat/multi-pc-agent-pairing`, derivada de `01c9cce`.
Data: 2026-10-08. Ambiente: Windows, Python 3.12, Node e .NET SDK 10.0.401 local.

## Executado

| Comando | Resultado |
|---|---|
| `py -3.12 tests/test_dashboard_ui.py` | PASS: estrutura estática responsiva/acessível |
| `py -3.12 tests/test_web_asset.py` | PASS: asset determinístico |
| `py -3.12 tests/test_no_ap_policy.py` | PASS: política Wi-Fi station-only |
| `py -3.12 tests/test_microlink_build_config.py` | Exit 0: configuração de build MicroLink |
| `node tests/test_dashboard_validation.js` | PASS: validação browser |
| `node tests/test_dashboard_polling.js` | PASS: polling/visibilidade |
| `node tests/test_microlink_dashboard.js` | PASS: estados Tailscale |
| `.dotnet10/dotnet.exe run --project agent/native/RemoteBoot.Agent.csproj -- --self-test` | Exit 0: WebSocket loopback, descoberta, reconnect/sessão nova, rejeição de sessão antiga, ACK e energia simulada |

O self-test inclui encerramentos de conexão esperados antes das mensagens PASS. Não executa shutdown/reboot real.

## Ainda não executado

Todos os casos novos da matriz multi-PC/pareamento permanecem PLANEJADOS: o código dessas funcionalidades ainda não foi implementado. Não houve teste novo Playwright, carga de quatro PCs, alteração de UEFI, reset ou upload da ESP nesta entrega. Os testes estáticos da dashboard não equivalem a teste de navegação em browser.

A versão MicroLink da base foi validada na sessão anterior por upload com hash verificado, peer ESP32C3 online em Tailscale, ping e cinco acessos HTTP pela tailnet; o usuário confirmou funcionamento. Isso é evidência histórica da base, não execução novamente nesta etapa.
