# UKI opcional CachyOS

O setup antigo validou uma UKI específica. A V2 não a exige e não redistribui a UKI privada.

`scripts/create-cachyos-uki.sh` é helper experimental específico para CachyOS com mkinitcpio. Ele requer `/etc/kernel/cmdline` correto para a instalação, sem BOOT_IMAGE; não deduz opções de criptografia, root, subvolume ou resume. Confirme a linha usada pelo sistema antes de gerar. Não reutilize UUID de outra máquina.

Informe a ESP montada, confirme UKI e verifique o arquivo gerado em EFI/Linux/cachyos-remote.efi. Ele não altera BootOrder, não substitui o preset padrão e não instala hook automático de atualização. Registre uma entry de teste com efibootmgr e use BootNext. Regenere após cada atualização de kernel/initramfs; para uso duradouro configure um preset mkinitcpio compatível com sua distro.

O helper foi derivado do procedimento de referência, mas não foi executado nesta máquina de trabalho nem validado em hardware V2.

Sintaxe consultada: [mkinitcpio upstream manual](https://man.archlinux.org/man/mkinitcpio.8.en).
