# Primeiro teste em hardware

1. Preserve a ordem UEFI original e confirme que o loader atual inicia diretamente pelo menu UEFI.
2. Grave a ESP32 pelo PlatformIO; configure AP/Wi-Fi/MAC/tokens e mantenha alimentação USB independente do PC desligado.
3. Execute o installer no host, sincronize catálogo e configure padrão + fallback válidos na dashboard. Não promova a entrada.
4. Marque TEST no installer para BootNext. Reinicie manualmente, acompanhe iPXE e confira o sistema padrão. Se falhar, use menu UEFI para o loader anterior.
5. Instale agent nesse OS e confira heartbeat/OS na dashboard. Repita nos demais OS desejados.
6. Desligue o PC; aguarde status offline (45 s). Selecione outro sistema na dashboard e confirme WoL + boot escolhido.
7. Repita para Windows, GRUB/shim e UKI somente se existentes. Nome do loader não comprova compatibilidade sem boot real.
8. Teste ESP32 offline/cabo desconectado: confirme timeout e recuperação pelo firmware. Teste ID obsoleto controladamente e fallback sem remover seu loader funcional.
9. Com PC ligado, boot comum deve retornar 409. Reboot exige habilitação do agent e confirmação explícita; salve trabalho antes.
10. Só após os testes bem-sucedidos execute promote. Guarde resultado, versão do firmware, placa, UEFI e NIC num relatório local privado.

Estes testes **não foram executados pelo ambiente de desenvolvimento**. Não considere a V2 validada em hardware pelo sucesso do setup V1.
