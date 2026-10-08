# Matriz de validação: múltiplos PCs e pareamento

Estado inicial: **PLANEJADO** para todas as novas funcionalidades. Esta matriz não é um relatório de testes executados. A implementação deve produzir testes automatizados correspondentes e registrar resultados em `docs/testing/results/`, incluindo commit, firmware/variante, ambiente, duração e evidência.

## Ambiente e dados

- PC A e PC B possuem uma entrada `0001` com nomes diferentes. Adicionar C/D para carga. Usar MACs sintéticos distintos em testes de software.
- A possui agents Windows e Linux; B outro agent. Quatro sessões simultâneas usam PCs diferentes. Oito vínculos não significam oito sessões.
- FakeHost registra chamadas sem energia/UEFI real. RNG/clock/storage de testes são injetados; produção usa RNG real.
- Senhas e credenciais sintéticas nos traces. Teste real recebe senha admin por entrada segura; não gravar em Git, screenshots, argv ou logs.
- CI Linux executa política C++ com ASan/UBSan; CI Windows/Linux executa NativeAOT; Playwright Chromium/Firefox/WebKit verifica UI. Firmware real usa a variante MicroLink.

## Lógica e isolamento

| ID | Cenário | Resultado obrigatório | Camada |
|---|---|---|---|
| IS-01 | A/B têm Boot ID 0001, filas e catálogos diferentes | Seleção, nome, TTL, fallback e dispatch não se misturam | C++ + hardware |
| IS-02 | Agent A envia pc_id B/token A | Não acessa B nem rotas admin; nenhuma mutação | API + integração |
| IS-03 | A e B pedem WoL simultâneo | Pacotes contêm MAC certo; cooldown é independente | C++ + captura UDP |
| IS-04 | Windows e Linux de A disputam sessão | Segunda sessão recebe conflito; não substitui a primeira silenciosamente | C++ + agent |
| IS-05 | ACK de B, ID repetido, sessão antiga ou ACK >15 s | Agent/ESP rejeitam; zero execução no FakeHost | C++ + C# |
| IS-06 | Quinto PC, MAC duplicado, nome/ID inválido | Erro determinístico; snapshot anterior intacto | C++ + API/UI |
| IS-07 | Revogar/remover agent com comando pendente | Desconecta, cancela fila; token antigo nunca reconecta | Integração/hardware |
| IS-08 | Sync de A com sessão antiga ou catálogo repetido | Rejeita sessão antiga; preferências de B intactas | API + C++ |
| API-01 | JSON inválido, campos inesperados, body/frame >12.000 | 400/413; sem crash nem alteração parcial | API + WS |
| API-02 | Bootstrap/status com Tailscale inativo/ativo | JSON UTF-8 válido em respostas repetidas; strings copiadas | Hardware + regressão |
| API-03 | Paginação e catálogo mudando durante leitura | Generation detecta mudança; sem catálogo combinado | API + UI |
| API-04 | Config/listas/logs em limites máximos | Nenhuma senha/token/segredo aninhado aparece | C++ + API |
| API-05 | Cliente antigo ou PC desconhecido | Versão/404 explícitos; nenhum fallback para PC implícito | API + agent |

## Persistência

| ID | Cenário | Resultado obrigatório | Camada |
|---|---|---|---|
| NV-01 | Falha antes/durante/depois do blob e seletor | Carrega snapshot anterior ou novo completo, nunca parcial | FakeStore + hardware controlado |
| NV-02 | CRC/comprimento/seletor corrompido | Recupera banco válido ou bloqueia com motivo; não abre cadastro indevido | C++ |
| NV-03 | Snapshot máximo com UTF-8 e 8 vínculos | Cabe em 20.000 bytes e NVS64KiB; excesso falha antes de escrever | C++ + ESP |
| NV-04 | NVS cheia ao aprovar pareamento | Não entrega token nem exibe aprovação; repetição não duplica agent | C++ + API |
| NV-05 | Reboot/reset/schema antigo | Reboot preserva vínculos; reset remove todos; schema antigo exige instalação limpa | ESP |

## Pareamento e instaladores

| ID | Cenário | Resultado obrigatório | Camada |
|---|---|---|---|
| PA-01 | Janela fechada/expirada | Start recusado; nada alocado persistentemente | C++ + API |
| PA-02 | Código minúsculo/com hífen/espaços externos | Normaliza só formato permitido; não aceita caracteres ambíguos arbitrários | C++ + UI |
| PA-03 | Expiração em 300 s e wrap millis | Expira corretamente nos dois casos | C++ com clock falso |
| PA-04 | Mais de 3 pendentes ou 6 starts/min | 429/limite sem negar acesso admin normal | API + carga |
| PA-05 | Cinco códigos errados/min; tentativa durante bloqueio | Bloqueio 60 s; erro não revela candidatos | C++ + API |
| PA-06 | Segredo de outro pedido/token admin no poll | Não entrega credencial; código curto sozinho insuficiente | API |
| PA-07 | Aprovação/entrega perde resposta | Mesmo segredo recupera mesmo token; só um vínculo persistido | C++ + C# |
| PA-08 | Duas abas aprovam o mesmo código | Uma aprovação efetiva; segunda idempotente/conflito claro | API + UI |
| PA-09 | Cancelamento/reboot antes de confirmar | Fecha pendência; vínculo já persistido não vira duplicata | C++ + hardware |
| PA-10 | Colisão RNG de código/ID e agentes no limite | Regenera com tentativas limitadas ou falha clara; não sobrescreve | C++ |
| AG-01 | Pair completo Windows e Linux | Código → aprovação → arquivo protegido → hello → serviço ativo | NativeAOT + instaladores |
| AG-02 | Timeout, cancel, offline e expiração | Motivo correto, saída previsível, nenhum serviço falso | Loopback + installers |
| AG-03 | Redirect/proxy e URL inválida | Não envia segredo para outro servidor | C# |
| AG-04 | Falha/gravação interrompida do agent.json | Arquivo anterior válido ou novo completo; sem arquivo aberto ao usuário comum | C# + OS |
| AG-05 | Instância duplicada e reconnect | Uma instância; sessão nova; comando antigo não repetido | C# |
| AG-06 | Permissões locais reboot/shutdown negadas | Painel reflete permissão e FakeHost não executa | C# + UI |
| AG-07 | Hostname enganoso/MAC VPN/duplicado | Admin vê metadados para conferir; associação não automática | Installer + UI |

## Interface

| ID | Interação | Resultado obrigatório |
|---|---|---|
| UI-01 | Primeiro acesso/reset e zero PCs | Cadastrar senha, entrar, estado vazio com Adicionar PC; sem token longo |
| UI-02 | Adicionar/editar até 4 PCs | Nome/MAC corretos, validação no campo, limite claro, nenhum duplicado por duplo clique |
| UI-03 | Selecionar A/B e agir | Cards, Boot, catálogo, permissões e confirmação mostram PC correto |
| UI-04 | Resposta lenta de A chega depois da seleção B | UI permanece B; comando originalmente confirmado A segue identificado A |
| UI-05 | Parear novo PC/OS adicional | Código, revisão e destino corretos; online somente após hello |
| UI-06 | Código errado/expirado/cancel/offline/NVS falha | Erro específico, retry seguro, foco útil; sem sucesso falso |
| UI-07 | Remover PC/agent, cancelar confirmação | Cancelar não muda nada; confirmar afeta só alvo e limpa estados locais |
| UI-08 | Sinric escolhe A, troca B, remove B | Confirma troca, limpa slots; remoção desativa; nenhuma ação em A |
| UI-09 | 360/390/768/1280px, teclado e leitores | Sem overflow horizontal; foco modal/restauração; labels/erros acessíveis |
| UI-10 | Duas abas, reload durante poll e erro JSON | Conflitos tratados, estado recarrega sem repetir ação, diagnóstico não chama tudo de rede |

Executar asserções de comportamento e rede, incluindo PC ID em cada request e proibição de segredos no DOM/localStorage. Screenshots em larguras definidas complementam asserções. Incluir checagem automática de acessibilidade e inspeção manual de teclado; teste automático não substitui revisão visual.

## Boot e Sinric

| ID | Cenário | Resultado obrigatório |
|---|---|---|
| BT-01 | iPXE A/B com ID0001 e pending simultâneo | Cada URL retorna somente loader/target do PC correspondente |
| BT-02 | PC ID ausente/removido/adulterado | Saída segura; nunca consome seleção de PC default |
| BT-03 | Manifesto de outro PC/hash errado | Installer recusa antes de tocar EFI |
| BT-04 | Boot/fallback/guard nos dois PCs | Política local e guard independentes; sem recursão |
| BT-05 | Falha instalação/reexecução | Backup e BootOrder preservados; pareamento não altera EFI |
| SI-01 | Evento de switch e PC escolhido A | Somente A recebe; B com mesmo Boot ID permanece intacto |
| SI-02 | Trocar escolhido A→B | Confirmação e limpeza dos mapeamentos; nenhum reaproveitamento silencioso |
| SI-03 | Remover PC escolhido | Sinric desativado e slots limpos atomicamente |
| SI-04 | PC sem catálogo/offline/agent sem permissão | Rejeição clara; nunca redireciona ação a outro PC |

## Gates reais e carga

| ID | Teste | Critério |
|---|---|---|
| HW-01 | Dois PCs reais; Windows e Linux no mesmo PC | Pareamento, identidade, catálogo e reconexão isolados |
| HW-02 | LAN e Tailscale | ESP online na tailnet; HTTP autenticado e WS por IP100.x; cinco acessos consecutivos válidos |
| HW-03 | 4 sessões/8 vínculos/96 entradas/Tailscale/Sinric | Sem OOM/watchdog; requests cabem no limite; registrar heap/largest block e picos TLS |
| HW-04 | Soak2h, reconexões Wi-Fi/agents e polling | Sem reset inesperado, queda monótona de memória ou HTTP>8s sob carga controlada |
| HW-05 | WoL/reboot/shutdown/boot/fallback deliberados | Dois PCs de teste, MAC e alvo comprovados; nenhum efeito no PC errado |
| HW-06 | Reset e reinstalação completa | Cadastro e pareamento do zero; credenciais anteriores rejeitadas |

Além de registro/ping no Tailscale, verificar tráfego HTTP/WS autenticado. Teste pela mesma LAN não prova acesso de uma rede externa; incluir cliente fora da LAN quando disponível e marcar BLOCKED se não houver.

## Registro de resultados

Para cada caso: ID, PASS/FAIL/BLOCKED, commit, comando/passos, ambiente, saída sanitizada, responsável e limitação. Gate bloqueado não é PASS. Traces e screenshots nunca contêm Auth Key/senha/token reais. Hardware ausente é limitação documentada, não cobertura por mock.
