# Upstream

Vendorizado de `https://github.com/CamM2325/microlink` no commit
`216da3300f0493b0860247d43f7af5ce29df63a5`.

Alterações locais necessárias para o ESP32-C3:

- tarefas sem afinidade obrigatória em chips single-core;
- buffer temporário Noise limitado a 24 KiB;
- pacotes descriptografados entregues por `netif->input` ao thread correto do lwIP;
- MTU WireGuard conservador de 1280 bytes;
- `wireguard_lwip` movido para componente ESP-IDF irmão.

- resposta de registro reutiliza o buffer estático de 32 KiB do coord, em três
  fatias disjuntas (16 KiB para H2, 8 KiB para JSON e 4 KiB para frames), antes
  da leitura do mapa inicial na mesma tarefa; elimina 28 KiB de alocações
  temporárias sem mudar limites de parsing ou admissão TLS. Capacidade mínima
  de 32 KiB é verificada na compilação e pelo teste portátil do workspace.

- Long-poll map assembly also borrows the coordination scratch (one extra byte
  for the bounded JSON terminator). Incremental parsing ignores unused DERPMap
  metadata and materializes one peer array entry at a time, preserving removal
  and patch handling. Borrowed frame storage is never freed by frame reset.
  Failure logs identify Noise/H2/map bounds or allocation and stream closure.

- The hybrid build enables ESP-IDF dynamic mbedTLS record buffers. RX records
  retain the 16KB maximum and are allocated from the incoming record length;
  consumed RX and flushed TX buffers shrink to small idle state. Configuration
  and CA certificate freeing stay disabled to preserve reconnect ownership.
  The existing DERP TLS admission guard remains unchanged.

- After the loaded-device DERP handshake exhausted heap, the initial-map scratch
  became a dynamic coordination-task lease: registration retains it for the
  initial map, which always releases it on return. Disconnect, reconnect and
  task exit also release it. Streaming maps allocate only their declared bounded
  length. Noise decrypts in place after authentication; HTTP/2 DATA fragments
  feed map framing directly, without a second retained H2 DATA body. Padded DATA
  and flow-control accounting retain the original frame semantics and limits.

- A inicialização da aplicação dá prioridade ao registro da tailnet antes de
  admitir novas sessões TLS Sinric e DERP. A espera adicional é limitada a20s;
  sessões existentes/upgrades e a política normal de admissão são preservados.

- A lease do workspace de registro/mapa inicial é adquirida antes do STUN/DNS
  para evitar fragmentação anterior à reserva; os caminhos de liberação existentes
  permanecem responsáveis por mapa concluído, falha, desconexão e encerramento.

- Preferência regional DERP e hostname de fallback configuráveis via Kconfig;
  o padrão do componente é compatível com Dallas e o projeto seleciona São Paulo.

- O retorno WireGuard preserva por peer o último caminho autenticado (DERP ou
  UDP), inclusive durante entrega IP assíncrona. Probes diretos continuam
  explícitos por pacote, sem sobrescrever a preferência das respostas. A
  promoção de next_keypair reata o ponteiro antes de marcar a janela anti-replay.

- Logs por pacote WireGuard e por sondagem DISCO/CallMeMaybe/PeerGone passam de
  INFO para DEBUG. O console do ESP-IDF escreve de forma síncrona (UART a
  115200 baud e USB sem leitor); no núcleo único do C3, esse volume (~5 linhas/s
  em repouso, mais durante tráfego) atrasava o loop do servidor web.
