# UEFI e NIC

Alvo: x86_64 UEFI, CSM/Legacy desabilitado, Ethernet com WoL. Os nomes de menus variam. Procure Wake-on-LAN / Power On By PCI-E; opções ErP/deep sleep podem remover energia da NIC quando desligada. Verifique manual da placa.

Os binários produzidos não são assinados por chave confiável do firmware. Secure Boot precisa estar desativado para este fluxo experimental, ou requer um processo de assinatura/enrollment externo que este projeto não implementa. Não altere Secure Boot ou TPM às cegas em máquina com BitLocker: tenha os meios de recuperação antes de modificar boot.

O iPXE roda como aplicação EFI no disco; não há TFTP obrigatório. Mantenha os loaders existentes e teste via BootNext antes de promover a entrada.

## Rede disponível no iPXE

O driver de rede usado pelo Linux/Windows não fica disponível durante o boot EFI. O iPXE precisa de um driver nativo compatível ou de uma interface de rede exposta pelo firmware UEFI via SNP (Simple Network Protocol). O build `ipxe.efi` usado pelo projeto inclui suporte a SNP.

A Realtek RTL8125 (`10ec:8125`), por exemplo, não tem driver nativo na revisão iPXE fixada em `ipxe/build.sh`. Nesse caso, habilite a rede UEFI na BIOS para que o iPXE possa usar o driver do firmware:

1. Entre na BIOS e abra o modo avançado (normalmente **F7** em placas ASUS).
2. Procure **Advanced → Network Stack Configuration**; os nomes e o caminho variam conforme a placa e a versão da BIOS.
3. Habilite **Network Stack** e **IPv4 PXE Support** (ou opções equivalentes de rede UEFI).
4. Salve as alterações e teste novamente **Remote Boot iPXE**, com o cabo Ethernet conectado.

Mantenha a entrada **Remote Boot iPXE** como destino do teste; habilitar a rede UEFI não exige selecionar o boot PXE da placa nem configurar um servidor TFTP. Essa configuração depende de o firmware fornecer um driver compatível; valide no hardware antes de promover a entrada.

`net0 no such network device` indica que a interface esperada não foi encontrada pelo iPXE. No shell iPXE, `ifstat` permite conferir as interfaces disponíveis. Se nenhuma aparecer mesmo com a rede UEFI habilitada, investigue o suporte do firmware/driver; trocar apenas o timeout DHCP não resolve a ausência da interface. Em PCs com várias NICs, o script atual usa `net0` e pode precisar de ajuste.

Mensagens `file:autoexec.ipxe not found` e `file:/autoexec.ipxe not found` não são a causa principal quando o script embutido continua e mostra **Remote Boot**: nesse caso, investigue o erro posterior de rede ou loader.

Referências: [drivers e alvos de build do iPXE](https://ipxe.org/appnote/buildtargets) e [orientação ASUS para Network Stack/IPv4 PXE](https://www.asus.com/br/support/faq/1049115/).
