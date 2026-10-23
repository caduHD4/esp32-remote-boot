# Acesso à dashboard — 23/10/2026

## Encontrado e corrigido neste PC

A Ethernet tinha IP e rota padrão, mas faltava sua rota conectada para a LAN.
O kernel enviava tráfego para a ESP pela interface Wi-Fi, mesmo com o cabo
ativo. Nesse caminho, as medições mostraram perdas e respostas HTML incompletas.

Reaplicar a conexão ativa pelo NetworkManager restaurou a rota automática da
LAN pela Ethernet. A rota específica para a ESP usada no teste foi removida:
não ficou dependência de IP fixo nem alteração no perfil DHCP persistente.

Depois disso, oito de oito downloads completos do HTML gzip passaram e vinte
pings tiveram zero perdas, com média de aproximadamente2,3ms. O usuário também
confirmou que a dashboard abriu e permitiu login. Após reiniciar a ESP para
conferir o backup final, outros oito downloads e vinte pings passaram.

## Controles e preservação

Reiniciar a ESP, testar outro modo do rádio e testar o firmware padrão não
resolveram o acesso pelo Wi-Fi deste PC. Não ficou nenhuma dessas imagens de
diagnóstico: a aplicação MicroLink completa original foi restaurada em0x20000.

O backup final confirmou aplicação normal exata, configuração carregada
integralmente idêntica à anterior, identidade de descoberta idêntica e regiões
de boot/partições/PHY inalteradas. O leitor de backup respeitou a recuperação
pelo banco alternativo que o ConfigStore já implementa.

A causa das perdas no caminho sem fio não foi estabelecida. Esta correção
comprova o acesso pelo cabo neste PC; não comprova recuperação do celular ou
de todas as conexões Wi-Fi. Não houve alteração do roteador, credenciais,
EFI, serviço/agente do PC ou configuração persistente de rede do host.
