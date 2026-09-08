# UEFI e NIC

Alvo: x86_64 UEFI, CSM/Legacy desabilitado, Ethernet com WoL. Os nomes de menus variam. Procure Wake-on-LAN / Power On By PCI-E; opções ErP/deep sleep podem remover energia da NIC quando desligada. Verifique manual da placa.

Os binários produzidos não são assinados por chave confiável do firmware. Secure Boot precisa estar desativado para este fluxo experimental, ou requer um processo de assinatura/enrollment externo que este projeto não implementa. Não altere Secure Boot ou TPM às cegas em máquina com BitLocker: tenha os meios de recuperação antes de modificar boot.

Não precisa ativar PXE tradicional da BIOS. O iPXE roda como aplicação EFI no disco; não há TFTP obrigatório. Mantenha os loaders existentes e teste via BootNext antes de promover a entrada.
