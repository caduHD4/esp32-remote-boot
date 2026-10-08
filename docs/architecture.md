# Arquitetura

O firmware mantém até quatro PCs independentes. Cada PC tem ID de 32 caracteres hexadecimais minúsculos, MAC, catálogo de até 24 entradas UEFI e estado de boot próprio. Até oito agents podem estar vinculados, no máximo dois por PC. O agent sincroniza somente o catálogo do PC ao qual foi pareado. Um PC aceita uma sessão WebSocket ativa por vez.

## Estado e persistência

A configuração persistida usa schema 3 no namespace NVS `remote-boot-v3`. Cada banco tem um cabeçalho (`bank0`, `bank1`) com magic, geração, tamanho e CRC32; o corpo JSON fica em blocos de até 512 bytes (`b0_00` etc.). A serialização, a leitura e a verificação usam streams para evitar uma alocação contínua do snapshot inteiro. A gravação escreve e verifica o banco inativo antes de atualizar o marcador uint64 `commit`. O carregamento aceita também os blobs anteriores, convertidos na próxima gravação. O limite permanece 20.000 bytes. A partição NVS começa em `0x9000` e tem tamanho `0x10000`; o app começa em `0x20000`.

O loop HTTP tem um único proprietário da configuração em RAM. Uma alteração transfere essa configuração para uma transação, sem cloná-la. Após a gravação atômica, a transação devolve o documento ao estado ativo; em caso de falha, recarrega o último snapshot confirmado. Os catálogos só aparecem nas rotas paginadas, sem cópias temporárias durante a montagem do bootstrap. No MicroLink, o mesmo buffer de 32 KiB é reutilizado entre registro, mapa inicial e fragmentos do long-poll; o parser retém um peer por vez. Os buffers TLS são dinâmicos, mantendo os limites de registros de 16/4 KiB e os critérios de admissão TLS.

O último PC selecionado para boot é persistido separadamente do catálogo. Pending, timestamps monotônicos, presença, sessão ativa e comandos aguardando resposta são estado em RAM; reiniciar o ESP32 descarta esses dados transitórios. Cada PC mantém padrão, fallback, ordem e visibilidade de entradas. Uma sincronização conserva ordem/visibilidade de IDs ainda presentes e remove referências a entradas removidas.

## API, pareamento e isolamento

A interface de rede é API v2; respostas e configuração identificam schema 3. `/api/v1/*` retorna 410. Rotas administrativas usam o Bearer admin. Operações de PC recebem o ID do PC na rota e consultam apenas seu catálogo/estado. A identidade de um agent é resolvida pelo token vinculado e pela sessão ativa, nunca por um PC escolhido no payload. Mensagens autenticadas carregam `pc_id`, `agent_id` e `session_id`; os três precisam corresponder ao vínculo e à sessão.

O pareamento começa apenas durante uma janela aberta pelo administrador e exige aprovação explícita associada a um PC. A credencial do agent só é entregue depois da aprovação. O agent salva a configuração protocol 2 e confirma que recebeu e validou a sessão. Cada agent sincroniza o próprio catálogo; não existe token global de agent nem sincronização HTTP legada.

## Despacho UEFI

A ESP32 serve um script iPXE por PC em `/boot/{pc_id}.ipxe`. O instalador gera iPXE com esse URL específico e um manifesto liga o PC ID e IP da ESP32 aos hashes do iPXE e do loader. O script busca o loader final no disco local; a ESP32 não hospeda os sistemas operacionais.

O `RemoteBoot.efi` valida a entrada solicitada, grava seu ID de 16 bits em `BootNext` e chama `ResetSystem(EfiResetCold)`. No boot seguinte, o firmware consome `BootNext` e executa a entrada usando sua política nativa. O endpoint de boot evita despachar novamente o mesmo alvo durante 60 segundos; nova solicitação aceita pela API rearma o despacho. Se a entrada indicada falhar e o firmware voltar ao iPXE, o script sai para a ordem normal da UEFI.

O loader iPXE embute o URL do PC; portanto, reutilizar a mesma imagem EFI em máquinas distintas viola o vínculo do manifesto. Os installers preservam backup e BootOrder, mas não fazem rollback transacional completo após falha no meio da instalação.

## Limites UEFI

`RemoteBoot.efi` interpreta `EFI_LOAD_OPTION`, valida Device Path, bloqueia recursão, expande `HD()` por assinatura, chama `LoadImage`, configura `LoadOptions`/`OptionalData` e chama `StartImage`. Fallback é tentado no máximo uma vez e somente se a chamada retornar erro. `OptionalData` permanece alocado durante `StartImage`. A atualização de `BootCurrent` é temporária e sua falha não impede o boot.

Suporte inicial: paths completos e short form `HD()` com assinatura única. Paths multi-instance, URI/USB short forms e `File()` sem expansão de boot manager completa podem falhar e retornar ao firmware. Rede/USB ficam ocultos por padrão. “Genérico” significa ausência de nomes de OS hardcoded, não implementação integral do Boot Manager UEFI.

O timeout de DHCP e progresso HTTP do script inicial é 10 s (o builder aceita 1–60 s), mas não é deadline absoluto: iPXE pode estender DHCP para link/switch e reinicia o timeout durante progresso HTTP. Não há timeout seguro para interromper loader/kernel após transferência de controle. Somente `net0` é selecionada inicialmente; múltiplas NICs exigem ajuste e teste. `exit` retorna ao chamador/firmware, cuja continuação do BootOrder depende da implementação UEFI.

Fontes: [UEFI Boot Manager](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html), [Loaded Image](https://uefi.org/specs/UEFI/2.10/09_Protocols_EFI_Loaded_Image.html), [iPXE ifconf](https://ipxe.org/cmd/ifconf), [iPXE imgfetch](https://ipxe.org/cmd/imgfetch), [iPXE embed](https://ipxe.org/embed).
