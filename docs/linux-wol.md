# WoL Linux

Em NetworkManager, identifique a conexão Ethernet com `nmcli connection show` e configure:

```bash
sudo nmcli connection modify 'NOME_DA_CONEXAO' 802-3-ethernet.wake-on-lan magic
sudo ethtool NOME_DA_INTERFACE
```

Reative a conexão quando puder interromper a rede. Confirme `Wake-on: g` no ethtool. Não copie nomes de interfaces de outro PC. Sem NetworkManager, configure o equivalente no gerenciador usado pela distro.

Teste desligamento real e link da NIC com ESP32 em fonte independente. Compartilhar sub-rede IP não garante que o AP encaminha broadcast: isolamento de clientes pode impedir WoL.

Fonte: [NetworkManager Ethernet settings](https://networkmanager.dev/docs/api/latest/settings-802-3-ethernet.html).
