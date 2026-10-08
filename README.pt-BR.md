# ESP32 Remote Boot 3

[English](README.md) | [Português (Brasil)](README.pt-BR.md)

Escolha qual sistema instalado o PC vai iniciar, ligue-o pela Ethernet e solicite reinício ou desligamento pela dashboard da ESP32. Versão **3.0.0-alpha.1**, configuração **v3**, API **v2**.

Suporta até **4 PCs**, **8 instalações de agent** e **24 entradas UEFI por PC**. Windows e Linux do mesmo computador usam um cadastro de PC e agents separados. O Sinric Pro controla um PC escolhido.

## Leia antes de começar

Siga as etapas 1–11 na ordem. Cada etapa termina com uma conferência: resolva qualquer falha antes de continuar.

O papel de cada parte:

- **ESP32:** hospeda a dashboard e guarda a escolha de sistema.
- **Agent:** roda dentro do Windows/Linux, informa as entradas de boot e executa reinício/desligamento quando autorizado.
- **iPXE + RemoteBoot.efi:** rodam antes do sistema operacional e aplicam a escolha da ESP32. São instalados uma vez por computador físico.
- **BIOS/UEFI:** inicia o iPXE e fornece a rede quando a placa precisa do driver do firmware.

Os sistemas já precisam estar instalados e funcionando. Este projeto não instala Windows/Linux nem hospeda suas imagens de instalação.

Nos comandos abaixo, **192.168.1.50 é um exemplo**. Substitua pelo IPv4 LAN reservado para sua ESP32. Execute os comandos do projeto na pasta que contém README.md, platformio.ini, agent/, installer/ e ipxe/. Não copie o símbolo do prompt do terminal.

## 1. Confira o hardware e prepare a BIOS

Você precisa de uma ESP32-C3 com **4 MB de flash**, cabo USB de dados, Wi-Fi **2,4 GHz** e um PC x86-64 com Ethernet, UEFI e Wake-on-LAN. O installer Linux exige systemd. A instalação EFI automatizada exige discos GPT e uma partição EFI em FAT.

Conecte o cabo Ethernet do PC ao roteador. Alimente a ESP32 por uma fonte que continue ligada quando o PC desligar.

Entre na BIOS/UEFI, normalmente apertando **Del** ou **F2** ao ligar. Os nomes dos menus variam:

1. Use o modo **UEFI**; desative **CSM/Legacy**. Antes de mudar, confirme que seus sistemas atuais já iniciam em UEFI.
2. Desative **Secure Boot** para usar os binários EFI não assinados do projeto. Se o Windows usa BitLocker/criptografia do dispositivo, tenha a chave de recuperação antes de alterar o boot. Não limpe o TPM.
3. Ative **Wake-on-LAN / Power On By PCI-E**. Desative **ErP/deep sleep** se retirar energia da placa Ethernet com o PC desligado.
4. Ative **Network Stack** e **IPv4 PXE Support**, normalmente em **Advanced → Network Stack Configuration**. Em placas ASUS, **F7** pode abrir o modo avançado. Isso é necessário para placas que usam o driver UEFI/SNP, incluindo RTL8125 na revisão iPXE utilizada.
5. Salve e inicie seu sistema atual. Mantenha a prioridade de boot dele até o teste do iPXE funcionar.

Habilitar o driver de rede da BIOS não exige um servidor TFTP. Mais adiante, escolha **Remote Boot iPXE**, e não a opção separada de boot PXE/IPv4 do firmware.

**Confira antes de continuar:** Windows/Linux ainda inicia normalmente e o cabo Ethernet está conectado. Veja [detalhes da BIOS](docs/bios.md).

## 2. Baixe o código e instale as ferramentas

Baixe o código atualizado da **main** no [GitHub](https://github.com/caduHD4/esp32-remote-boot): **Code → Download ZIP** e extraia. Se já tem Git:

~~~bash
git -c core.autocrlf=false clone https://github.com/caduHD4/esp32-remote-boot.git
cd esp32-remote-boot
~~~

Se o repositório for privado, entre com uma conta autorizada. Evite uma pasta antiga com artefatos de build desconhecidos.

### Linux: instale os pré-requisitos

Ubuntu/Debian:

~~~bash
sudo apt update
sudo apt install python3 python3-venv git build-essential binutils gnu-efi perl liblzma-dev jq curl efibootmgr ethtool
~~~

Arch/CachyOS:

~~~bash
sudo pacman -S --needed python git base-devel binutils gnu-efi perl xz jq curl efibootmgr ethtool
~~~

Crie um ambiente Python **dentro da pasta do projeto**:

~~~bash
python3 -m venv .venv
.venv/bin/python -m pip install platformio==6.1.19
.venv/bin/pio --version
~~~

### Windows: instale os pré-requisitos

Instale [Python 3](https://www.python.org/downloads/windows/) com o Python launcher. Abra o PowerShell na pasta do projeto:

~~~powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.1.19
.\.venv\Scripts\pio.exe --version
~~~

Use os caminhos dos executáveis mostrados aqui; não precisa ativar o ambiente Python. Se já usa pipx, a instalação equivalente é: **pipx install --force platformio==6.1.19**.

**Confira antes de continuar:** o comando de versão mostra **PlatformIO Core 6.1.19**. Use exatamente essa versão: a 6.2.0 tem incompatibilidade de SCons com a toolchain MicroLink.

## 3. Configure e grave a ESP32

Copie config.local.example.json para **config.local.json** na pasta do projeto. Edite apenas os valores, mantendo um JSON válido:

~~~json
{
  "ssid": "YOUR_2_4_GHZ_WIFI",
  "wifi_password": "YOUR_WIFI_PASSWORD"
}
~~~

Troque os textos pelo nome e senha do seu Wi-Fi de 2,4 GHz. Esse arquivo é privado. As credenciais são embutidas na compilação; trocar o Wi-Fi exige outro upload.

Escolha **uma** variante:

| O que você precisa | Variante |
|---|---|
| Dashboard, agents, boot e Sinric na rede local | esp32c3_4mb |
| A própria ESP32 deve entrar no Tailscale | esp32c3_4mb_microlink |

Conecte a ESP32 por USB. Para a variante padrão:

~~~bash
# Linux
.venv/bin/pio run -e esp32c3_4mb -t upload
.venv/bin/pio device monitor -b 115200
~~~

~~~powershell
# PowerShell no Windows
.\.venv\Scripts\pio.exe run -e esp32c3_4mb -t upload
.\.venv\Scripts\pio.exe device monitor -b 115200
~~~

Para Tailscale, troque esp32c3_4mb no comando de upload por **esp32c3_4mb_microlink**. Se a porta USB não for detectada automaticamente, use a porta correta com **--upload-port PORTA** no upload e **-p PORTA** no monitor. Encontre-a com **pio device list**, usando o caminho do executável do seu sistema mostrado acima. Saia do monitor com **Ctrl+C**.

Anote o IPv4 mostrado pela ESP32. Nas configurações DHCP do roteador, reserve esse IP para a ESP32 para que ele não mude.

**Confira antes de continuar:** o upload terminou sem erro e a ESP32 recebeu um IPv4 da rede local. Upload comum preserva as configurações salvas. Migrar da versão 2 exige backup e um apagamento separado porque o mapa de partições mudou; siga [migração/recuperação](docs/setup.md) antes de gravar sobre uma versão 2 existente.

## 4. Abra a dashboard e cadastre o PC

1. Abra **http://IP_DA_SUA_ESP32** no navegador de um dispositivo na mesma rede.
2. No primeiro acesso, preencha **Senha** e **Repetir senha** e clique **Salvar e reiniciar ESP**. A senha deve ter 8–128 caracteres. Depois do reinício da ESP32, faça login.
3. Descubra o **MAC Ethernet** do PC. No Windows, execute **getmac /v** ou **ipconfig /all**. No Linux, execute **ip -br link**. Use o adaptador com fio, não Wi-Fi, Bluetooth ou Tailscale.
4. Clique **Adicionar PC** e informe nome e MAC.

No dual boot, crie **um cadastro para o computador físico** e pareie os dois agents a ele. Crie outro cadastro apenas para outro computador.

**Confira antes de continuar:** o card do PC apareceu. Ele ficar offline é esperado até o agent conectar. O PC ID interno é gerado automaticamente; você não precisa procurá-lo na dashboard.

## 5. Baixe os agents e confira os ZIPs

Na mesma [release 3.x](https://github.com/caduHD4/esp32-remote-boot/releases), baixe **agent-SHA256SUMS** e o pacote de cada sistema usado:

- Windows: **remote-boot-agent-win-x64.zip**
- Linux: **remote-boot-agent-linux-x64.zip**

Confira cada ZIP antes de extrair:

~~~powershell
Get-FileHash .\remote-boot-agent-win-x64.zip -Algorithm SHA256
~~~

~~~bash
sha256sum remote-boot-agent-linux-x64.zip
~~~

Compare com a linha desse mesmo ZIP no arquivo agent-SHA256SUMS. Os valores precisam ser iguais.

Extraia o pacote em outra pasta. Copie o executável para a pasta do código atualizado, no caminho correspondente abaixo; crie as pastas se necessário. Use os installers do código atualizado.

~~~text
projeto/
  README.md
  platformio.ini
  installer/
  agent/
  ipxe/
  build/
    agent/
      win-x64/remote-boot-agent.exe
      linux-x64/remote-boot-agent
~~~

Você precisa apenas do executável do sistema atual. Não precisa instalar o runtime .NET; os agents da release são NativeAOT.

**Confira antes de continuar:** o executável está no caminho esperado. Não continue se o hash do ZIP for diferente ou o installer disser que falta o binário.

## 6. Instale e pareie o agent em cada sistema

Na dashboard, clique primeiro em **Parear agent** e deixe a janela aberta. Ela permite pareamento durante cinco minutos.

### Windows

Inicie o Windows no PC de destino. Abra o **PowerShell como administrador**, entre na pasta do projeto e execute:

~~~powershell
.\installer\windows\install-agent.ps1 -EspAddress '192.168.1.50'
~~~

Se o PowerShell bloquear o script baixado, você pode permitir a execução apenas nesse terminal com **Set-ExecutionPolicy -Scope Process Bypass** e repetir o comando. Políticas da organização ainda podem se aplicar.

### Linux

Inicie o Linux real do PC de destino, **não o WSL**. Confirme o modo UEFI:

~~~bash
test -d /sys/firmware/efi && echo "UEFI OK"
~~~

Depois, na pasta do projeto:

~~~bash
chmod +x build/agent/linux-x64/remote-boot-agent
sudo bash installer/linux/install-agent.sh 192.168.1.50
~~~

Digite a senha de administrador do Linux quando o sudo pedir.

### Conclua o pareamento

Em qualquer um dos sistemas:

1. Na pergunta sobre reinício remoto, digite **REBOOT** para habilitar ou apenas Enter para deixar desativado.
2. Na pergunta sobre desligamento remoto, digite **SHUTDOWN** para habilitar ou apenas Enter para deixar desativado.
3. O installer mostrará um código como **ABCD-EFGH**. Mantenha o terminal aberto.
4. Digite o código na dashboard e clique **Consultar código**.
5. Confira hostname/sistema, escolha o **PC de destino** correto e dê um nome à instalação, como Windows ou Linux.
6. Marque **Conferi o computador e autorizo este vínculo** e clique **Confirmar vínculo**.
7. Espere o installer terminar. Aprovar sozinho não conclui: ele precisa salvar a credencial, conectar e sincronizar o catálogo.

Se o código expirar, abra outra janela de pareamento e execute o installer novamente. No dual boot, repita esta etapa após iniciar o outro sistema e escolha o **mesmo cadastro de PC**. Instale o agent em cada sistema no qual deseja status, reinício ou desligamento.

**Confira antes de continuar:** **Sistema → Agents vinculados** mostra o agent do sistema atual conectado e as permissões solicitadas habilitadas. No Linux, o comando abaixo também deve mostrar **active**:

~~~bash
systemctl is-active remote-boot
~~~

No Windows, confira a tarefa **RemoteBootAgent** no Agendador de Tarefas. O agent é separado do iPXE; esta etapa não altera as entradas de boot.

## 7. Escolha o sistema padrão e habilite Wake-on-LAN

Selecione o PC na dashboard e espere seu catálogo de boot carregar. Use **Sincronizar agent** em **Agents vinculados** se necessário.

Em **Configuração → Ajustes do PC selecionado**:

1. Escolha em **Sistema padrão** o loader que deve iniciar normalmente.
2. Escolha em **Fallback** outro loader local válido, se houver.
3. Em **Botão físico do PC**, escolha **Usar sistema padrão** para o teste inicial.
4. Clique **Salvar ajustes do PC**.

Não escolha Remote Boot/iPXE como destino. Essas entradas são bloqueadas para evitar recursão. Uma entrada GRUB inicia o GRUB, incluindo o menu dele; não escolhe um item interno do menu.

Habilite WoL em **cada sistema instalado**:

- **Windows:** nas propriedades do adaptador Ethernet no Gerenciador de Dispositivos, habilite Wake on Magic Packet e Shutdown Wake-On-Lan quando existirem, e permita que o adaptador acorde o PC. Desative a Inicialização Rápida do Windows. Veja [WoL Windows](docs/windows-wol.md).
- **Linux com NetworkManager:** encontre a conexão com fio com **nmcli connection show** e use seu nome exato abaixo. Encontre a interface Ethernet com **ip -br link**.

~~~bash
sudo nmcli connection modify 'NOME_DA_CONEXAO_ETHERNET' 802-3-ethernet.wake-on-lan magic
sudo ethtool NOME_DA_INTERFACE_ETHERNET
~~~

Reative a conexão quando puder interromper a rede e confirme **Wake-on: g** no ethtool. Outros gerenciadores de rede exigem a configuração equivalente; veja [WoL Linux](docs/linux-wol.md).

**Confira antes de continuar:** a dashboard tem padrão/fallback válidos e o agent está conectado. Mais adiante, teste acordar após desligamento real; WoL depende da placa, BIOS e último sistema desligado.

## 8. Compile e instale iPXE uma vez por PC

Use o caminho **A** se esse computador tem Linux. Use **B** se tem apenas Windows. No dual boot, A basta: não instale outro iPXE pelo Windows apenas porque instalou um segundo agent.

### A. Instalação pelo Linux real do PC

Inicie o Linux e confirme que o agent está pareado. As dependências da etapa 2 precisam estar instaladas. Na pasta atualizada do projeto:

~~~bash
sudo bash installer/linux/install.sh
~~~

O installer **lê o IP da ESP32 e o PC ID interno de /etc/remote-boot/agent.json**, compila a imagem específica e confere o manifesto. Você não precisa informar o PC ID manualmente.

Responda nesta ordem:

1. **EFI System Partition mount:** confira a partição detectada, normalmente /boot/efi, /efi ou /boot. Dê Enter somente se estiver correta. Ela precisa ser a partição EFI GPT/FAT montada, não a raiz do Linux nem uma partição de dados do Windows.
2. **INSTALL:** digite INSTALL para gravar a imagem e criar a entrada UEFI. Anote o caminho do backup.
3. **REUSE:** se encontrar uma entrada Remote Boot existente, digite REUSE somente depois de conferir que aponta para a partição EFI escolhida. Entradas duplicadas exigem investigação.
4. **TEST:** digite TEST para pedir um teste único do iPXE na próxima reinicialização.

O installer não reinicia o PC. Ele informa o **BootXXXX** real criado ou reutilizado; anote. Esse número varia de computador para computador.

### B. Compilação com Linux/WSL e instalação pelo Windows

O build EFI exige ferramentas Linux. Se o PC só tem Windows, instale Ubuntu/WSL, se necessário, com **wsl --install -d Ubuntu** no PowerShell administrador, reinicie se solicitado e conclua a configuração inicial do Ubuntu.

No **PowerShell administrador do Windows do PC de destino**, leia apenas o PC ID público salvo pelo agent pareado:

~~~powershell
$pcId = (Get-Content "$env:ProgramData\RemoteBoot\agent.json" -Raw | ConvertFrom-Json).pc_id
$pcId
~~~

Copie esse valor de 32 caracteres. Não copie nem compartilhe o agent.json inteiro; ele contém um token secreto.

Abra Ubuntu/WSL, instale as dependências Ubuntu da etapa 2 e entre na pasta do código. O caminho Windows C:\Users\SEU_USUARIO\Downloads\esp32-remote-boot normalmente vira /mnt/c/Users/SEU_USUARIO/Downloads/esp32-remote-boot no WSL. Coloque caminhos com espaços entre aspas.

Compile usando o IP real da ESP32 e o ID que acabou de ler:

~~~bash
bash ipxe/build.sh 192.168.1.50 COLE_O_PC_ID_AQUI
~~~

O build gera **build/PC_ID/ipxe.efi**, **RemoteBoot.efi**, **manifest.json** e **SHA256SUMS**. Mantenha-os juntos. Se compilou em outra máquina Linux, copie toda essa pasta de saída para o Windows de destino.

Volte ao **PowerShell administrador na pasta Windows do código**:

~~~powershell
.\installer\windows\install.ps1 -EspAddress '192.168.1.50' -IpxeFile ".\build\$pcId\ipxe.efi" -FullScan
~~~

Confira a saída e responda **VERIFIED** após verificar o build/hash. Escolha os **DiskNumber** e **PartitionNumber** corretos na lista de partições EFI. Se houver várias e você não souber identificar a correta, pare e consulte [descoberta EFI](docs/uefi-discovery.md). Digite **INSTALL** para instalar e **TEST** para o teste único. Guarde o caminho do backup e o BootXXXX.

**Não execute installer/linux/install.sh nem install-agent.sh no WSL para alterar o PC Windows.** Nesse caminho, o WSL serve apenas para compilar; os installers Windows cuidam do Windows.

**Confira antes de continuar em qualquer caminho:** o installer terminou, a imagem pertence a esse PC/ESP32 e você guardou o backup e o ID da entrada. No Linux, **efibootmgr -v** lista **Remote Boot iPXE** apontando para **\\EFI\\iPXE\\ipxe.efi**. Não reutilize a imagem de um PC em outro.

## 9. Teste iPXE antes de torná-lo padrão

Salve seu trabalho e reinicie. TEST usa **BootNext**, portanto a primeira tentativa é temporária. Observe:

1. **Remote Boot**
2. **Configuring [dhcp] ... ok**
3. **http://IP_DA_ESP32/boot/PC_ID.ipxe ... ok**
4. O sistema escolhido/padrão iniciando. RemoteBoot.efi pode gravar BootNext e causar um segundo reset; isso é esperado.

O agent deve reconectar depois que o sistema iniciar. Teste todos os loaders que pretende usar, não apenas um.

Se voltar ao menu comum dos sistemas sem mostrar iPXE, abra o **menu de boot do firmware**, não o menu GRUB. Tente **Remote Boot iPXE**. Algumas BIOS omitem entradas fora do BootOrder mesmo quando existem; a opção **Boot from EFI file**, quando disponível, permite abrir **EFI/iPXE/ipxe.efi** diretamente. Consulte [diagnóstico](docs/troubleshooting.md) antes de mudar a prioridade padrão.

**Confira antes de continuar:** iPXE recebeu endereço DHCP, baixou o script desse PC e o sistema desejado realmente iniciou. Mantenha a prioridade normal dos sistemas se esse teste falhar.

## 10. Coloque Remote Boot em primeiro e teste o controle remoto

Depois do sucesso na etapa 9, entre nas prioridades de boot da BIOS/UEFI, coloque **Remote Boot iPXE em primeiro**, mantenha as entradas Windows/Linux seguintes e salve. Os installers preservam o BootOrder antigo; o TEST temporário sozinho não torna o iPXE permanente.

Inicie novamente e confirme que o sistema padrão abre. Depois teste pela dashboard:

1. Desligue o PC normalmente; mantenha a ESP32 alimentada e a Ethernet conectada.
2. Espere a dashboard informar que o PC está offline.
3. No card do sistema desejado, clique **Ligar**. Isso guarda a escolha e envia Wake-on-LAN.
4. Confirme que o PC acordou, iniciou o iPXE e entrou nesse sistema.
5. Repita após desligar cada sistema instalado, porque cada um pode alterar o WoL da placa.

Com o PC ligado, use **Reiniciar aqui** para trocar de sistema e confirme; o agent atual deve permitir REBOOT. **Ligar** não reinicia um PC já ligado. **Forçar WoL** apenas envia WoL; também não reinicia. **Desligar** exige SHUTDOWN habilitado no agent atual.

Se falhar, escolha o loader Windows/Linux existente pelo menu UEFI e faça o diagnóstico. Os backups ficam em **/var/backups/remote-boot** no Linux ou **%ProgramData%\RemoteBoot\backup-*** no Windows. Veja [recuperação](docs/setup.md).

**Confira ao terminar:** boot padrão, escolha remota de sistema, WoL e os comandos de energia habilitados funcionam nesse PC real.

## 11. Opcional: Tailscale e Sinric Pro

**Tailscale:** a ESP32 deve usar **esp32c3_4mb_microlink**. Na dashboard, preencha a Auth Key em **Configuração → Tailscale**, salve e espere reconectar. A variante padrão não entra no Tailscale; salvar uma chave não adiciona o recurso. Valide a instalação LAN primeiro. Veja [MicroLink/Tailscale](docs/microlink-tailscale.md).

**Sinric Pro:** crie uma app e dispositivos Switch no [portal Sinric](https://portal.sinric.pro). Escolha **um PC** nas configurações Sinric da dashboard, salve a seleção e então preencha App Key, App Secret, Device IDs e mapeamentos. ON solicita a ação; OFF não desliga o PC. Trocar o PC limpa os mapeamentos. Desligamento exige agent conectado e autorizado. Veja [configuração Sinric](docs/sinric.md).

## Se algo der errado

| Mensagem ou sintoma | Primeira verificação |
|---|---|
| Falta o binário do agent | Etapa 5: localização do executável e conteúdo do ZIP. |
| Código de pareamento expirou | Reabra Parear agent e execute o installer novamente; conclua em cinco minutos. |
| sudo pede senha | Digite a senha de administrador Linux no terminal; autorização na dashboard/chat não autentica o sudo. |
| Bash reclama de pipefail com caractere extra | O script tem finais de linha Windows CRLF. Use um ZIP novo do código ou clone com core.autocrlf=false. |
| net0 no such network device | Habilite Network Stack/IPv4 PXE na BIOS. RTL8125 precisa do driver UEFI/SNP nesta revisão iPXE. |
| file:autoexec.ipxe not found | Se aparece Remote Boot depois, o script embutido está rodando; investigue o erro seguinte. |
| DHCP falha | Cabo Ethernet, DHCP do roteador, driver UEFI, VLAN e interface escolhida pelo iPXE. |
| Não baixa a URL do script | IP reservado da ESP32, acesso pela rede e cadastro do PC. Recompile se o IP/identidade mudar. |
| Exec format error / RemoteBoot failed | Use o builder atualizado e recompile/reinstale. RemoteBoot.efi precisa ser uma aplicação EFI x86-64 real, não um fixture ou artefato antigo. |
| iPXE não aparece no GRUB | Ele é uma entrada UEFI separada do menu GRUB. Confira o menu do firmware e a entrada EFI. |
| PC inicia normalmente depois de TEST | TEST é temporário; valide a etapa 9 e então configure prioridade permanente na etapa 10. |
| PC não acorda | WoL na BIOS e no último sistema desligado, MAC Ethernet correto, energia da NIC/ESP32 e isolamento de broadcast na LAN. |
| Sistema mudou, mas aparece offline | Instale/pareie o agent nesse sistema ao mesmo cadastro de PC. |

Mais: [diagnóstico](docs/troubleshooting.md), [detalhes do agent](docs/native-agent.md), [API](docs/api.md), [arquitetura](docs/architecture.md), [segurança](SECURITY.md).

## Atualizações e verificações de desenvolvimento

Uploads comuns preservam as configurações NVS. Trocar IP da ESP32, identidade do PC ou código EFI exige recompilar e reinstalar a imagem iPXE desse PC. Atualizar apenas o firmware ESP32 não substitui os arquivos EFI do computador. Não há atualização OTA.

Na pasta do código, com as dependências de desenvolvimento instaladas:

~~~bash
bash tests/run.sh
pio run -e esp32c3_4mb
pio run -e esp32c3_4mb_microlink
npm ci
npx playwright install
npm run test:ui
~~~

O size gate rejeita firmware acima de 90% da partição real de aplicação. A configuração usa dois bancos NVS com CRC e geração. Testes de software não comprovam WoL/boot físico em todas as placas; valide seu hardware como descrito acima e na [matriz de testes](docs/testing/multi-pc-agent-pairing.md).

Mantenha credenciais Wi-Fi, tokens dos agents, backups de firmware, snapshots privados e UKIs específicos da máquina fora do Git.
