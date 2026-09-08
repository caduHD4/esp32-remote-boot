# Arquitetura

A ESP32 só serve o pequeno script de seleção. O installer embute script inicial e RemoteBoot.efi no iPXE; o loader final permanece no disco local.

O host descobre Boot####, envia ID/descrição/flags e mantém heartbeat. O catálogo NVS suporta 24 entradas; nomes não identificam o target. A ordem e a visibilidade escolhidas pelo administrador sobrevivem à sincronização. Targets removidos invalidam padrão, fallback e pending; slots inválidos são retirados.

Estado separado: padrão persistido na configuração; última escolha em NVS; pending e timestamp monotônico em RAM. Pending permanece em retries HTTP e só é consumido por heartbeat com ID correspondente ou expira. Reiniciar a ESP32 descarta pending por segurança, preservando padrão/última escolha. Um heartbeat sem ID confiável deixa a expiração a cargo do TTL.

Configuração JSON em uma única chave NVS com schema 2. Migração de schema 1 ajusta a versão e TTL ausente; os demais campos devem validar. Conteúdo inválido ou versão futura fica bloqueado e preservado. Não se importam credenciais/targets da V1 antiga com sistemas hardcoded.

UEFI: parse limitado de EFI_LOAD_OPTION → validação de Device Path → bloqueio de recursão → expansão HD() por assinatura → LoadImage → LoadOptions/OptionalData → StartImage. Fallback só é tentado uma vez e somente se a chamada retornar erro. OptionalData permanece alocado durante StartImage. BootCurrent é atualizado temporariamente se o firmware aceitar; a falha nessa atualização não impede o boot.

Suporte inicial: paths completos e short form HD() com assinatura única. Paths multi-instance, URI/USB short forms e File()-only não têm expansão de boot manager completa. Entradas dessas classes podem falhar; retorne ao firmware. Rede/USB são ocultos por padrão. “Genérico” significa ausência de nomes de OS hardcoded, não implementação integral de todos os comportamentos de um boot manager UEFI.

Timeout de DHCP e progresso HTTP usa 10 s no script inicial (builder aceita 1–60 s). Isso **não é deadline absoluto**: iPXE pode estender DHCP para link/switch e progresso HTTP reinicia seu timeout. Não há timeout seguro para interromper loader/kernel após transferência de controle. V2 não promete fallback se o OS falhar depois.

Somente `net0` é selecionada no profile inicial, para evitar multiplicar timeouts; múltiplas NICs exigem ajuste do template e teste. `exit` retorna ao chamador/firmware; a continuação exata do BootOrder depende da implementação UEFI.

Fontes primárias: [UEFI Boot Manager](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html), [Loaded Image](https://uefi.org/specs/UEFI/2.10/09_Protocols_EFI_Loaded_Image.html), [iPXE ifconf](https://ipxe.org/cmd/ifconf), [iPXE imgfetch](https://ipxe.org/cmd/imgfetch), [iPXE embed](https://ipxe.org/embed).
