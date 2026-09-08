# Sinric Pro: Wake-on-LAN dual boot

Sinric Pro é a integração principal para ligar o PC e selecionar o sistema. Crie dois dispositivos do tipo **Switch** no portal: por exemplo, “PC Windows” e “PC Linux”. Copie App Key, App Secret e os dois Device IDs para a dashboard e mapeie cada um para a respectiva entrada `Boot####` descoberta. Os rótulos são livres; o mapeamento usa o ID UEFI, não o nome do sistema.

O fluxo completo é `Switch ON` → ESP32 salva o target em NVS → ESP32 envia Wake-on-LAN → UEFI inicia iPXE local → ESP32 entrega `/boot.ipxe` → `RemoteBoot.efi` carrega o loader EFI local correspondente. Portanto, iPXE e `RemoteBoot.efi` são parte necessária da seleção remota dual boot; agents são opcionais e servem para status/reboot remoto com o PC já ligado.

## Configuração completa

### 1. Prepare o PC e a ESP32

1. Conclua o onboarding Wi-Fi, MAC Ethernet, broadcast e reserva DHCP da ESP32 descritos no README.
2. Configure WoL no BIOS/UEFI e nos dois sistemas operacionais.
3. Instale a entrada Remote Boot e execute um teste único (`TEST`) antes de promovê-la.
4. Na dashboard, sincronize o catálogo e anote o `Boot####` que inicia Windows e o que inicia Linux. Teste ambos pela dashboard antes de envolver o Sinric.

### 2. Crie a aplicação no Sinric Pro

1. Acesse [portal.sinric.pro](https://portal.sinric.pro) e crie ou entre na sua conta.
2. Na área **Apps**, crie uma app para este projeto, por exemplo `ESP32 Remote Boot`.
3. Abra a app e, na área **Credentials**, copie **App Key** e **App Secret**. Eles identificam a app; não são Device IDs.
4. Não publique essas credenciais, os Device IDs, a senha Wi-Fi, o MAC real ou tokens em commits, issues, prints ou vídeos.

### 3. Crie os dois dispositivos

1. Na área **Devices** da mesma app, adicione um dispositivo do tipo **Switch** chamado `PC Windows`.
2. Adicione outro **Switch** chamado `PC Linux` (ou o nome da sua distribuição).
3. Abra cada dispositivo e copie seu **Device ID**. Cada ID é diferente e deve ter 24 caracteres no firmware.

O nome é o que aparecerá no Alexa/Google Home; ele não determina o sistema. A relação correta será definida na dashboard da ESP32.

### 4. Vincule voz (quando usar Alexa ou Google Home)

1. Vincule a conta Sinric Pro ao assistente de voz pelo fluxo de integração oferecido pelo próprio portal Sinric Pro.
2. No app Alexa ou Google Home, execute a descoberta/sincronização de dispositivos.
3. Confirme que aparecem exatamente os dois Switches criados: `PC Windows` e `PC Linux`.

Não continue enquanto os dois Switches não puderem ser acionados no app do assistente. O ESP32 ainda não deve ser mapeado nesta etapa.

### 5. Cadastre as credenciais na ESP32

1. Abra a dashboard da ESP32 pelo IP reservado e autentique com o token administrativo.
2. Em **Sinric**, marque **Ativar Sinric**.
3. Cole **App Key** e **App Secret** da etapa 2.
4. Em **Sinric • até 8 dispositivos Switch**, clique em **Adicionar dispositivo** duas vezes.
5. Cole o Device ID de `PC Windows` e selecione o `Boot####` que você validou para Windows.
6. Cole o Device ID de `PC Linux` e selecione o `Boot####` que você validou para Linux.
7. Salve. A ESP32 reinicia para registrar os callbacks. Reconecte à dashboard e confirme no status `Sinric online`.

### 6. Teste sem arriscar o boot padrão

1. Desligue completamente o PC. Ele deve continuar energizado pela tomada e conectado por Ethernet.
2. Acione somente `PC Windows` no app do assistente. O PC deve ligar e iniciar a entrada UEFI mapeada para Windows.
3. Desligue novamente e repita com `PC Linux`.
4. Só após os dois testes funcionarem, promova a entrada Remote Boot, se esse for o comportamento desejado para o botão físico do PC.

Se o status não ficar `Sinric online`, confirme Wi-Fi/DNS, App Key, App Secret e se os dois Device IDs pertencem à mesma app. Se o Switch ligar o PC mas entrar no sistema errado, corrija somente o mapeamento `Device ID → Boot####`; não recrie a app nem altere o BootOrder.

Não mapeie um Switch a `default` enquanto ainda estiver validando os dois sistemas. Use `default` somente depois de definir padrão/fallback e testar ambos.

A implementação usa `SinricProSwitch`, `onPowerState`, `sendPowerStateEvent` e `SinricPro.begin` do SDK 3.3.1. Não cria dispositivos no portal nem inventa enumeração de modes. O limite local de oito slots não promete oito dispositivos gratuitos: plano/limites da conta são separados.

ON enfileira boot; OFF não desliga o PC. Após o evento aceito, o Switch é devolvido para OFF. Pedido com PC online é rejeitado. Após edição de credenciais/slots, salvar reinicia o ESP32 para registrar os callbacks atualizados.

Fonte: [SDK SinricPro 3.3.1](https://github.com/sinricpro/esp8266-esp32-sdk/tree/3.3.1). Integração cloud não validada nesta execução.
