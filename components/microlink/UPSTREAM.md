# Upstream

Vendorizado de `https://github.com/CamM2325/microlink` no commit
`216da3300f0493b0860247d43f7af5ce29df63a5`.

Alterações locais necessárias para o ESP32-C3:

- tarefas sem afinidade obrigatória em chips single-core;
- buffer temporário Noise limitado a 24 KiB;
- pacotes descriptografados entregues por `netif->input` ao thread correto do lwIP;
- MTU WireGuard conservador de 1280 bytes;
- `wireguard_lwip` movido para componente ESP-IDF irmão.
