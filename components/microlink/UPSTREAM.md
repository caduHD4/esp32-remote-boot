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
