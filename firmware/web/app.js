'use strict';
function validateSinric(input){
  const errors={},warnings={};
  const enabled=!!input.enabled;
  const appKey=(input.appKey||'').trim();
  const appSecret=(input.appSecret||'').trim();
  const keyReady=appKey?appKey.length>=10:!!input.appKeySet;
  const secretReady=appSecret?appSecret.length>=10:!!input.appSecretSet;
  let clean=(input.slots||[]).map(slot=>({device_id:(slot.device_id||'').trim(),boot_id:slot.boot_id||''}));
  if(!enabled)clean=clean.filter(slot=>slot.device_id.length>0);
  if(enabled&&!keyReady)errors.sinric_app_key='Informe uma App Key válida com pelo menos 10 caracteres.';
  if(enabled&&!secretReady)errors.sinric_app_secret='Informe um App Secret válido com pelo menos 10 caracteres.';
  const validBootIds=new Set(input.validBootIds||[]),seen=new Set();
  clean.forEach((slot,index)=>{
    const normalized=slot.device_id.toLowerCase();
    if(!/^[0-9a-f]{24}$/i.test(slot.device_id))errors['slot_'+index]='O Device ID deve conter exatamente 24 caracteres hexadecimais.';
    else if(seen.has(normalized))errors['slot_'+index]='Device ID duplicado.';
    else seen.add(normalized);
    if(slot.boot_id!=='default'&&slot.boot_id!=='shutdown'&&!validBootIds.has(slot.boot_id))errors['slot_'+index]='Selecione uma ação ou sistema válido.';
  });
  if(enabled&&clean.length===0)warnings.no_slots=true;
  return {valid:Object.keys(errors).length===0,errors,warnings,slots:clean};
}
function formatTailscaleStatus(tailscale={}){
  if(!tailscale.built)return {label:'DESATIVADO',detail:'Firmware padrão',tone:'neutral'};
  if(!tailscale.configured)return {label:'NÃO CONFIGURADO',detail:'Adicione a chave no build',tone:'warning'};
  if(tailscale.state==='wifi_offline')return {label:'AGUARDANDO WI-FI',detail:'Acesso local preservado',tone:'warning'};
  if(tailscale.state==='peer_wait')return {label:'CONTROLE ONLINE',detail:(tailscale.derp_online?'Relay online':'Relay pendente')+' • Sem tráfego autenticado recente',tone:'warning'};
  if(tailscale.connected){
    const peerCount=Number(tailscale.peers)||0;
    return {label:'CONECTADO',detail:(tailscale.ip||'IP pendente')+' • '+peerCount+' peers',tone:'online'};
  }
  if(tailscale.state==='error')return {label:'ERRO',detail:'Acesso local preservado',tone:'error'};
  const states={config_locked:['BLOQUEADO','Revise a configuração'],setup_mode:['AGUARDANDO','Conclua a configuração local'],wifi_offline:['AGUARDANDO WI-FI','Acesso local preservado'],starting:['INICIANDO','Preparando conexão'],connecting:['CONECTANDO','Negociando acesso remoto'],registering:['REGISTRANDO','Aguardando a tailnet'],reconnecting:['RECONECTANDO','Acesso local preservado']};
  const value=states[tailscale.state]||['AGUARDANDO','Acesso local preservado'];
  return {label:value[0],detail:value[1],tone:'warning'};
}
function createStatusPoller({poll,isHidden}){
  let pending=false;
  const tick=()=>{
    if(pending||isHidden())return;
    pending=true;
    let request;try{request=poll()}catch(_){pending=false;return}
    Promise.resolve(request).catch(()=>{}).finally(()=>{pending=false})
  };
  return {tick,visibilityChanged:()=>{if(!isHidden())tick()}}
}
function statusControls(state){return {shutdownDisabled:!state.shutdown_enabled}}
globalThis.RemoteBootValidation={validateSinric,formatTailscaleStatus,createStatusPoller,statusControls};
if(typeof document!=='undefined'){
const $=id=>document.getElementById(id);
let token='',cfg={},systems=[],slots=[],refreshTimer,statusPoller,statusRequest;
const pageNames={overview:'Visão geral',boot:'Boot',settings:'Configuração',sinric:'Sinric Pro',system:'Sistema'};
const fields=[
  ['Rede','Conexão do ESP32 com a rede local',[
    ['ssid','SSID'],['wifi_password','Senha Wi-Fi','password'],['dhcp','Usar DHCP','checkbox'],
    ['ip','IP estático'],['subnet','Máscara de sub-rede'],['gateway','Gateway'],['dns','DNS']]],
  ['PC','Wake-on-LAN e identificação do computador',[
    ['pc_name','Nome do PC'],['mac','MAC Ethernet'],['wol_port','Porta WoL','number'],
    ['wol_repeat','Repetições WoL','number'],['wol_interval_ms','Intervalo WoL (ms)','number']]],
  ['Boot','Comportamento padrão de inicialização',[
    ['default_target','Sistema padrão','target'],['fallback_boot_id','Fallback','target'],
    ['pending_ttl_s','Validade da seleção (segundos)','number'],['physical_boot_behavior','Botão físico do PC','behavior']]],
  ['Acesso','Tokens locais de administração e agent',[
    ['admin_token','Novo token administrativo','password'],['agent_token','Novo token do agent','password']]]
];

function icon(name){const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('aria-hidden','true');const use=document.createElementNS('http://www.w3.org/2000/svg','use');use.setAttribute('href','#icon-'+name);svg.append(use);return svg}
function message(text){$('message').textContent=text}
function showToast(text,error=false){const host=$(error?'toastAlert':'toastStatus');const item=document.createElement('div');item.className='toast'+(error?' error':'');item.textContent=text;host.append(item);setTimeout(()=>item.remove(),4600);message(text)}
function apiError(code){const known={AUTH_REQUIRED:'Informe o token administrativo.',FORBIDDEN:'Token inválido ou sem permissão.',INVALID_CONFIG:'Revise os campos destacados.',SCHEMA_LOCKED:'A configuração está bloqueada por incompatibilidade.',PC_ALREADY_ON:'O computador já está online.',SINRIC_CREDENTIALS_REQUIRED:'Informe App Key e App Secret antes de ativar o Sinric.'};return known[code]?known[code]+' ('+code+')':code}
async function api(path,method='GET',data){
  let response;const controller=new AbortController();const timeout=setTimeout(()=>controller.abort(),8000);
  try{
    try{response=await fetch('/api/v1/'+path,{method,signal:controller.signal,headers:{Authorization:'Bearer '+token,'Content-Type':'application/json'},...(data?{body:JSON.stringify(data)}:{})})}
    catch(_){throw Error('Não foi possível acessar o ESP32. Verifique a rede.')}
    let result={};try{result=await response.json()}catch(_){if(!response.ok)throw Error('Resposta inválida do ESP32 ('+response.status+').');throw Error('Não foi possível acessar o ESP32. Verifique a rede.')}
    if(!response.ok)throw Error(apiError(result.error||String(response.status)));return result
  }finally{clearTimeout(timeout)}
}
function setBusy(button,busy,label){
  if(!button)return;
  const text=button.querySelector('span');
  if(busy){button.dataset.label=text?text.textContent:'';button.disabled=true;button.classList.add('busy');if(text&&label)text.textContent=label}
  else{button.disabled=false;button.classList.remove('busy');if(text&&button.dataset.label)text.textContent=button.dataset.label}
}
async function action(fn,button,success='Solicitação aceita.'){
  setBusy(button,true,'Processando');
  try{await fn();showToast(success);await status()}
  catch(error){showToast(error.message,true)}
  finally{setBusy(button,false)}
}
function setActiveView(name){
  if(!pageNames[name])return;
  document.querySelectorAll('.view').forEach(view=>{const active=view.id==='view-'+name;view.hidden=!active;view.classList.toggle('active',active)});
  document.querySelectorAll('[data-view]').forEach(button=>button.classList.toggle('active',button.dataset.view===name));
  $('pageTitle').textContent=pageNames[name];window.scrollTo({top:0,behavior:'smooth'})
}
function choices(input,value,kind){
  const add=(v,t)=>{const option=document.createElement('option');option.value=v;option.textContent=t;input.append(option)};
  add('','Nenhum');
  if(kind==='behavior'){input.replaceChildren();add('default_target','Usar sistema padrão');add('last_selected','Usar última seleção');add('exit_to_firmware','Retornar ao firmware')}
  else{if(kind==='slot'){add('default','Padrão / seleção pendente');add('shutdown','Desligar PC (agent)')}for(const entry of systems)if(!entry.blocked)add(entry.id,entry.name+' • '+entry.id)}
  input.value=value||''
}
function fieldInput(key,label,type='text'){
  if(type==='checkbox'){
    const wrap=document.createElement('label');wrap.className='check-field';const input=document.createElement('input');input.type='checkbox';input.id='f_'+key;input.checked=!!cfg[key];wrap.append(input,document.createTextNode(label));return wrap
  }
  const wrap=document.createElement('label');wrap.className='field';wrap.dataset.field=key;const title=document.createElement('span');title.textContent=label;
  const input=document.createElement(type==='target'||type==='behavior'?'select':'input');input.id='f_'+key;
  if(input.tagName==='SELECT')choices(input,cfg[key],type);else{input.type=type;if(type==='number')input.inputMode='numeric';input.value=type==='password'?'':cfg[key]??''}
  if(type==='password'){input.autocomplete='new-password';input.placeholder=cfg[key+'_set']?'Já configurado; vazio mantém':'Informe um novo valor'}
  const error=document.createElement('small');error.className='field-error';error.dataset.errorFor=key;wrap.append(title,input,error);return wrap
}
function renderFields(){
  $('fields').replaceChildren();
  for(const [title,description,list] of fields){
    const group=document.createElement('section');group.className='settings-group';const heading=document.createElement('h2');heading.textContent=title;const help=document.createElement('p');help.textContent=description;const grid=document.createElement('div');grid.className='form-grid';
    for(const definition of list)grid.append(fieldInput(...definition));group.append(heading,help,grid);$('fields').append(group)
  }
  const network=$('f_ip')?.closest('.settings-group');if(network){const staticKeys=['ip','subnet','gateway','dns'];for(const key of staticKeys)$('f_'+key).closest('.field').classList.add('static-field')}
  $('f_dhcp').addEventListener('change',setDhcpVisibility);setDhcpVisibility()
}
function setDhcpVisibility(){
  const hidden=!!$('f_dhcp')?.checked;
  document.querySelectorAll('.static-field').forEach(field=>{field.hidden=hidden;field.classList.toggle('static-fields',true)})
}
function renderEntries(){
  $('entries').replaceChildren();
  systems.forEach((entry,index)=>{
    const row=document.createElement('div');row.className='entry-row';const main=document.createElement('div');main.className='entry-main';const text=document.createElement('div');const name=document.createElement('strong');name.textContent=entry.name;const meta=document.createElement('small');meta.textContent='Boot'+entry.id+(entry.blocked?' • bloqueada':'');text.append(name,meta);
    const controls=document.createElement('div');controls.className='entry-controls';const hide=document.createElement('label');hide.className='switch-row';const check=document.createElement('input');check.type='checkbox';check.checked=!!entry.hidden;check.disabled=!!entry.blocked;check.onchange=()=>{entry.hidden=check.checked;renderButtons()};hide.append(check,document.createTextNode('Ocultar'));
    const up=document.createElement('button');up.type='button';up.className='icon-button';up.setAttribute('aria-label','Mover '+entry.name+' para cima');up.append(icon('up'));up.disabled=index===0;up.onclick=()=>{[systems[index-1],systems[index]]=[systems[index],systems[index-1]];renderEntries();renderButtons()};
    controls.append(hide,up);main.append(text,controls);row.append(main);$('entries').append(row)
  })
}
function bootButton(label,kind,handler){
  const button=document.createElement('button');button.type='button';button.className='button '+kind;button.append(icon(kind==='primary'?'power':'refresh'));const span=document.createElement('span');span.textContent=label;button.append(span);button.onclick=()=>handler(button);return button
}
function renderButtons(){
  $('buttons').replaceChildren();$('overviewBoot').replaceChildren();
  for(const entry of systems){
    if(entry.blocked||(entry.hidden&&!$('showHidden').checked))continue;
    const card=document.createElement('article');card.className='boot-card';const head=document.createElement('div');head.className='boot-card-head';const badge=document.createElement('div');badge.className='status-icon green';badge.append(icon('boot'));const title=document.createElement('div');const name=document.createElement('strong');name.textContent=entry.name;const id=document.createElement('small');id.textContent='Boot'+entry.id;title.append(name,id);head.append(badge,title);
    const actions=document.createElement('div');actions.className='boot-actions';
    actions.append(
      bootButton('Ligar','primary',button=>action(()=>api('boot','POST',{boot_id:entry.id}),button,'Boot solicitado para '+entry.name+'.')),
      bootButton('Forçar WoL','secondary',button=>{if(confirm('Enviar WoL para '+entry.name+' mesmo com o PC online? Isso não reinicia o PC.'))action(()=>api('boot','POST',{boot_id:entry.id,force:true,confirm:'FORCE_BOOT'}),button,'Pacote WoL enviado.')} ),
      bootButton('Reiniciar aqui','ghost',button=>{if(confirm('Reiniciar o PC agora em '+entry.name+'? Salve seu trabalho antes.'))action(()=>api('reboot','POST',{boot_id:entry.id,confirm:'REBOOT'}),button,'Reinicialização solicitada.')} )
    );
    card.append(head,actions);$('buttons').append(card);
    if($('overviewBoot').children.length<3){
      const quick=document.createElement('div');quick.className='quick-item';const info=document.createElement('div');const quickName=document.createElement('strong');quickName.textContent=entry.name;const quickId=document.createElement('small');quickId.textContent='Boot'+entry.id;info.append(quickName,quickId);quick.append(info,bootButton('Ligar','primary',button=>action(()=>api('boot','POST',{boot_id:entry.id}),button,'Boot solicitado para '+entry.name+'.')));$('overviewBoot').append(quick)
    }
  }
  if(!$('buttons').children.length){const empty=document.createElement('div');empty.className='panel';empty.textContent='Nenhuma entrada de boot disponível.';$('buttons').append(empty)}
}
function renderSlots(){
  $('slots').replaceChildren();
  slots.forEach((slot,index)=>{
    const row=document.createElement('div');row.className='slot-row';
    const idWrap=document.createElement('label');idWrap.className='field';const idTitle=document.createElement('span');idTitle.textContent='Device ID';const id=document.createElement('input');id.placeholder='24 caracteres hexadecimais';id.value=slot.device_id||'';id.autocomplete='off';id.oninput=()=>slot.device_id=id.value.trim();const idError=document.createElement('small');idError.className='field-error';idError.dataset.errorFor='slot_'+index;idWrap.append(idTitle,id,idError);
    const targetWrap=document.createElement('label');targetWrap.className='field';const targetTitle=document.createElement('span');targetTitle.textContent='Ação';const target=document.createElement('select');choices(target,slot.boot_id,'slot');target.onchange=()=>slot.boot_id=target.value;targetWrap.append(targetTitle,target);
    const remove=document.createElement('button');remove.type='button';remove.className='button danger-soft slot-remove';remove.append(icon('trash'));const label=document.createElement('span');label.textContent='Remover';remove.append(label);remove.onclick=()=>{slots.splice(index,1);renderSlots();updateSlotWarning()};
    row.append(idWrap,targetWrap,remove);$('slots').append(row)
  });
  updateSlotWarning()
}
function updateSlotWarning(){$('slotWarning').hidden=!($('f_sinric_enabled').checked&&slots.length===0)}
function renderSinric(){
  $('f_sinric_enabled').checked=!!cfg.sinric_enabled;$('f_sinric_app_key').value='';$('f_sinric_app_secret').value='';
  $('f_sinric_app_key').placeholder=cfg.sinric_app_key_set?'Já configurada; vazio mantém':'Informe a App Key';
  $('f_sinric_app_secret').placeholder=cfg.sinric_app_secret_set?'Já configurado; vazio mantém':'Informe o App Secret';
  $('hint_sinric_app_key').textContent=cfg.sinric_app_key_set?'Credencial armazenada no ESP32.':'';
  $('hint_sinric_app_secret').textContent=cfg.sinric_app_secret_set?'Credencial armazenada no ESP32.':'';
  $('f_sinric_enabled').onchange=updateSlotWarning;renderSlots()
}
function clearSkeletons(){document.querySelectorAll('.skeleton').forEach(item=>item.classList.remove('skeleton'))}
function updateStatusCards(state){
  clearSkeletons();$('cardPc').textContent=state.online?'ONLINE':'OFFLINE';$('cardOs').textContent=state.os||'Sistema não identificado';
  $('cardEsp').textContent=state.ip||'Sem IP';$('cardVersion').textContent=state.version||'Versão desconhecida';
  $('cardWifi').textContent=state.rssi>-67?'Sinal ótimo':state.rssi>-75?'Sinal bom':'Sinal fraco';$('cardRssi').textContent=state.rssi+' dBm';
  $('cardSinric').textContent=state.sinric_online?'ONLINE':cfg.sinric_enabled?'OFFLINE':'DESATIVADO';$('cardPending').textContent=state.pending_target?'Pendente: '+state.pending_target:'Nenhum boot pendente';
  const tailscale=formatTailscaleStatus(state.tailscale);$('cardTailscale').textContent=tailscale.label;$('cardTailscale').className='status-value '+tailscale.tone;$('cardTailscaleDetail').textContent=tailscale.detail;
  $('topBadge').textContent=state.online?'PC online':'PC offline';$('topBadge').className='badge '+(state.online?'online':'offline');$('sideDot').className='status-dot '+(state.online?'online':'');$('sideStatus').textContent=state.online?'PC online':'PC offline'
}
function applyStatus(state){
  $('shutdown').disabled=statusControls(state).shutdownDisabled;updateStatusCards(state);
  $('setupInfo').textContent=state.config_locked?'Configuração preservada, porém incompatível ou corrompida.':state.setup_mode?'Configure Wi-Fi, MAC e dois tokens diferentes para concluir.':'Campos de senha vazios mantêm os valores existentes.'
}
function status(){
  if(statusRequest)return statusRequest;
  statusRequest=(async()=>{const state=await api('status');applyStatus(state);return state})().finally(()=>{statusRequest=null});
  return statusRequest
}
async function connect(){
  token=$('token').value.trim();$('loginError').hidden=true;setBusy($('connect'),true,'Conectando');
  try{
    const initial=await api('bootstrap');cfg=initial.config;systems=cfg.systems||[];slots=cfg.sinric_slots||[];$('login').hidden=true;$('app').hidden=false;renderFields();renderEntries();renderSinric();renderButtons();applyStatus(initial.status);clearInterval(refreshTimer);statusPoller=createStatusPoller({poll:()=>status().catch(error=>showToast(error.message,true)),isHidden:()=>document.hidden});refreshTimer=setInterval(statusPoller.tick,10000);showToast('Dashboard conectada.')
  }catch(error){$('loginError').textContent=error.message;$('loginError').hidden=false}
  finally{setBusy($('connect'),false)}
}
function clearFieldErrors(){document.querySelectorAll('.field-error').forEach(error=>error.textContent='');document.querySelectorAll('[aria-invalid=true]').forEach(input=>input.removeAttribute('aria-invalid'))}
function showFieldErrors(errors){
  clearFieldErrors();let first=null;
  for(const [key,text] of Object.entries(errors)){
    const error=document.querySelector('[data-error-for="'+key+'"]');if(error)error.textContent=text;
    const input=key.startsWith('slot_')?$('slots').children[Number(key.slice(5))]?.querySelector('input'):$('f_'+key);
    if(input){input.setAttribute('aria-invalid','true');if(!first)first=input}
  }
  if(first)first.focus()
}
function collectPatch(validatedSlots=slots){
  const patch={};
  for(const [,,list] of fields)for(const [key,,type='text'] of list){const element=$('f_'+key);if(type==='password'&&!element.value)continue;patch[key]=type==='checkbox'?element.checked:type==='number'?Number(element.value):element.value}
  patch.sinric_enabled=$('f_sinric_enabled').checked;if($('f_sinric_app_key').value)patch.sinric_app_key=$('f_sinric_app_key').value;if($('f_sinric_app_secret').value)patch.sinric_app_secret=$('f_sinric_app_secret').value;
  patch.systems=systems;patch.sinric_slots=validatedSlots;return patch
}
$('connect').onclick=connect;$('token').addEventListener('keydown',event=>{if(event.key==='Enter')connect()});
document.addEventListener('visibilitychange',()=>statusPoller?.visibilityChanged());
document.querySelectorAll('[data-view]').forEach(button=>button.addEventListener('click',()=>setActiveView(button.dataset.view)));
$('refreshStatus').onclick=button=>action(()=>status(),button.currentTarget,'Status atualizado.');
$('showHidden').onchange=renderButtons;
$('addSlot').onclick=()=>{if(slots.length<8){slots.push({device_id:'',boot_id:'default'});renderSlots()}else showToast('O limite é de 8 dispositivos.',true)};
$('settings').onsubmit=async event=>{
  event.preventDefault();
  const validation=validateSinric({enabled:$('f_sinric_enabled').checked,appKey:$('f_sinric_app_key').value,appKeySet:!!cfg.sinric_app_key_set,appSecret:$('f_sinric_app_secret').value,appSecretSet:!!cfg.sinric_app_secret_set,slots,validBootIds:systems.filter(entry=>!entry.blocked).map(entry=>entry.id)});
  if(!validation.valid){showFieldErrors(validation.errors);showToast('Revise a configuração do Sinric antes de salvar.',true);setActiveView('sinric');return}
  clearFieldErrors();slots=validation.slots;renderSlots();
  const buttons=[...document.querySelectorAll('.save-button')];buttons.forEach(button=>setBusy(button,true,'Salvando'));
  try{await api('config','PUT',collectPatch(validation.slots));clearInterval(refreshTimer);showToast('Configuração salva. O ESP32 está reiniciando.')}
  catch(error){showToast(error.message,true)}finally{buttons.forEach(button=>setBusy(button,false))}
};
$('shutdown').onclick=event=>{if(confirm('Desligar o PC? Salve seu trabalho antes.'))action(()=>api('shutdown','POST',{confirm:'SHUTDOWN'}),event.currentTarget,'Desligamento solicitado.')};
$('rescan').onclick=event=>action(()=>api('discovery/request','POST',{}),event.currentTarget,'Atualização do catálogo solicitada.');
$('readLogs').onclick=async event=>{const button=event.currentTarget;setBusy(button,true,'Carregando');try{$('logs').textContent=(await api('logs')).logs.join('\n')||'Nenhum evento registrado.'}catch(error){showToast(error.message,true)}finally{setBusy(button,false)}};
$('restart').onclick=event=>{if(confirm('Reiniciar ESP32?'))action(()=>api('system/reboot','POST',{}),event.currentTarget,'ESP32 reiniciando.')};
$('reset').onclick=event=>{if(prompt('Digite FACTORY_RESET para apagar a configuração')==='FACTORY_RESET')action(()=>api('system/reset','POST',{confirm:'FACTORY_RESET'}),event.currentTarget,'Configuração apagada.')};
}
