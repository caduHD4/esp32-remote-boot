'use strict';
function validateTailscaleAuthKey(value){
  return !value||(value.startsWith('tskey-auth-')&&value.length>=20&&value.length<=159&&!/[\x00-\x1f\x7f]/.test(value));
}
function validateCredential(value){
  return typeof value==='string'&&value.length>=8&&value.length<=128&&!/[\x00-\x1f\x7f]/.test(value);
}
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
  if(!tailscale.configured)return {label:'NÃO CONFIGURADO',detail:'Adicione a chave na dashboard',tone:'warning'};
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
const rememberedLoginKey='remote-boot-admin-token';
function rememberedLogin(storage){try{return storage.getItem(rememberedLoginKey)||''}catch(_){return ''}}
function storeRememberedLogin(storage,value,remember){try{if(remember&&value)storage.setItem(rememberedLoginKey,value);else storage.removeItem(rememberedLoginKey)}catch(_){}}
globalThis.RemoteBootValidation={validateCredential,validateTailscaleAuthKey,validateSinric,formatTailscaleStatus,createStatusPoller,statusControls,rememberedLogin,storeRememberedLogin};
if(typeof document!=='undefined'){
const $=id=>document.getElementById(id);
let initialSetup=false,token='',cfg={},systems=[],slots=[],refreshTimer,statusPoller,statusRequest,pcs=[],agents=[],globalStatus={},sinricSystems=[];
const pcModel=RemoteBootPcModel();let catalogReady=false;const pcStatusRequests=new Map();
const pcFields=[['name','Nome do PC'],['mac','MAC Ethernet'],['wol_port','Porta WoL','number'],['wol_repeat','Repetições WoL','number'],['wol_interval_ms','Intervalo WoL (ms)','number'],['default_target','Sistema padrão','target'],['fallback_boot_id','Fallback','target'],['pending_ttl_s','Validade da seleção (segundos)','number'],['physical_boot_behavior','Botão físico do PC','behavior']];
const pageNames={overview:'Visão geral',boot:'Boot',settings:'Configuração',sinric:'Sinric Pro',system:'Sistema'};
const fields=[
  ['Rede','Wi-Fi definido manualmente em config.local.json',[
    ['dhcp','Usar DHCP','checkbox'],
    ['ip','IP estático'],['subnet','Máscara de sub-rede'],['gateway','Gateway'],['dns','DNS']]],
  ['Acesso','Senha administrativa (8–128 caracteres)',[['admin_token','Nova senha administrativa','password']]],
  ['Tailscale','Acesso remoto opcional pelo firmware MicroLink',[
    ['tailscale_auth_key','Auth Key do Tailscale','password'],['tailscale_device_name','Nome do dispositivo Tailscale']]]
];

function icon(name){const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('aria-hidden','true');const use=document.createElementNS('http://www.w3.org/2000/svg','use');use.setAttribute('href','#icon-'+name);svg.append(use);return svg}
function message(text){$('message').textContent=text}
function showToast(text,error=false){const host=$(error?'toastAlert':'toastStatus');const item=document.createElement('div');item.className='toast'+(error?' error':'');item.textContent=text;host.append(item);setTimeout(()=>item.remove(),4600);message(text)}
function apiError(code){const known={PC_LIMIT:'O limite é de 4 PCs.',MAC_ALREADY_REGISTERED:'Este MAC já pertence a outro PC.',PAIRING_CLOSED:'Abra uma nova janela de pareamento.',PAIRING_EXPIRED:'Código expirado. Reinicie o pareamento no instalador.',PAIRING_RATE_LIMIT:'Muitas consultas. Aguarde 60 segundos.',NVS_WRITE_FAILED:'Não foi possível salvar. Nenhum vínculo foi aprovado.',AGENT_REVOKED:'Este vínculo foi revogado.',AUTH_REQUIRED:'Informe a senha administrativa.',FORBIDDEN:'Senha inválida ou sem permissão.',INVALID_PASSWORD:'Use de 8 a 128 caracteres.',PASSWORD_MISMATCH:'As senhas não coincidem.',SETUP_CLOSED:'A senha inicial já foi configurada.',INVALID_CONFIG:'Revise os campos destacados.',SCHEMA_LOCKED:'A configuração está bloqueada por incompatibilidade.',PC_ALREADY_ON:'O computador já está online.',SINRIC_CREDENTIALS_REQUIRED:'Informe App Key e App Secret antes de ativar o Sinric.'};return known[code]?known[code]+' ('+code+')':code}
async function api(path,method='GET',data,context){
  let response;const controller=new AbortController();const timeout=setTimeout(()=>controller.abort(),8000);const untrack=context?pcModel.track(controller):()=>{};
  try{
    try{response=await fetch('/api/v2/'+path,{method,signal:controller.signal,headers:{Authorization:'Bearer '+token,'Content-Type':'application/json'},...(data?{body:JSON.stringify(data)}:{})})}
    catch(e){if(controller.signal.aborted&&context&&!pcModel.current(context))throw Error('STALE_REQUEST');throw Error('Falha de rede ao acessar o ESP32.')}
    let result={};try{result=await response.json()}catch(_){if(!response.ok)throw Error('Resposta inválida do ESP32 ('+response.status+').');throw Error('Resposta JSON inválida do ESP32.')}
    if(!response.ok)throw Error(apiError(result.error||String(response.status)));return result
  }finally{clearTimeout(timeout);untrack()}
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
  catch(error){if(error.message!=='STALE_REQUEST')showToast(error.message,true)}
  finally{setBusy(button,false)}
}
function setActiveView(name){
  if(!pageNames[name])return;
  document.querySelectorAll('.view').forEach(view=>{const active=view.id==='view-'+name;view.hidden=!active;view.classList.toggle('active',active)});
  document.querySelectorAll('[data-view]').forEach(button=>button.classList.toggle('active',button.dataset.view===name));
  $('pageTitle').textContent=pageNames[name];window.scrollTo({top:0,behavior:'smooth'})
}
function choices(input,value,kind,entries=systems){
  const add=(v,t)=>{const option=document.createElement('option');option.value=v;option.textContent=t;input.append(option)};
  add('','Nenhum');
  if(kind==='behavior'){input.replaceChildren();add('default_target','Usar sistema padrão');add('last_selected','Usar última seleção');add('exit_to_firmware','Retornar ao firmware')}
  else{if(kind==='slot'){add('default','Padrão / seleção pendente');add('shutdown','Desligar PC (agent)')}for(const entry of entries)if(!entry.blocked)add(entry.id,entry.name+' • '+entry.id)}
  if(value&&![...input.options].some(option=>option.value===value))add(value,'Indisponível • '+value);input.value=value||''
}
function fieldInput(key,label,type='text',source=cfg,prefix='f_'){
  if(type==='checkbox'){
    const wrap=document.createElement('label');wrap.className='check-field';const input=document.createElement('input');input.type='checkbox';input.id=prefix+key;input.checked=!!source[key];wrap.append(input,document.createTextNode(label));return wrap
  }
  const wrap=document.createElement('label');wrap.className='field';wrap.dataset.field=key;const title=document.createElement('span');title.textContent=label;
  const input=document.createElement(type==='target'||type==='behavior'?'select':'input');input.id=prefix+key;
  if(input.tagName==='SELECT')choices(input,source[key],type);else{input.type=type;if(type==='number')input.inputMode='numeric';input.value=type==='password'?'':source[key]??''}
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
  const context=pcModel.capture();const pcName=pcs.find(pc=>pc.id===context.pcId)?.name||'';
  $('buttons').replaceChildren();$('overviewBoot').replaceChildren();
  for(const entry of systems){
    if(entry.blocked||(entry.hidden&&!$('showHidden').checked))continue;
    const card=document.createElement('article');card.className='boot-card';const head=document.createElement('div');head.className='boot-card-head';const badge=document.createElement('div');badge.className='status-icon green';badge.append(icon('boot'));const title=document.createElement('div');const name=document.createElement('strong');name.textContent=entry.name;const id=document.createElement('small');id.textContent='Boot'+entry.id;title.append(name,id);head.append(badge,title);
    const actions=document.createElement('div');actions.className='boot-actions';
    actions.append(
      bootButton('Ligar','primary',button=>action(()=>pcApi(context,'boot','POST',{boot_id:entry.id}),button,'Boot solicitado para '+entry.name+'.')),
      bootButton('Forçar WoL','secondary',button=>{if(confirm('Enviar WoL para '+pcName+' / '+entry.name+' mesmo com o PC online? Isso não reinicia o PC.'))action(()=>pcApi(context,'boot','POST',{boot_id:entry.id,force:true,confirm:'FORCE_BOOT'}),button,'Pacote WoL enviado.')} ),
      bootButton('Reiniciar aqui','ghost',button=>{if(confirm('Reiniciar '+pcName+' agora em '+entry.name+'? Salve seu trabalho antes.'))action(()=>pcApi(context,'reboot','POST',{boot_id:entry.id,confirm:'REBOOT'}),button,'Reinicialização solicitada.')} )
    );
    card.append(head,actions);$('buttons').append(card);
    if($('overviewBoot').children.length<3){
      const quick=document.createElement('div');quick.className='quick-item';const info=document.createElement('div');const quickName=document.createElement('strong');quickName.textContent=entry.name;const quickId=document.createElement('small');quickId.textContent='Boot'+entry.id;info.append(quickName,quickId);quick.append(info,bootButton('Ligar','primary',button=>action(()=>pcApi(context,'boot','POST',{boot_id:entry.id}),button,'Boot solicitado para '+entry.name+'.')));$('overviewBoot').append(quick)
    }
  }
  if(!$('buttons').children.length){const empty=document.createElement('div');empty.className='panel';empty.textContent='Nenhuma entrada de boot disponível.';$('buttons').append(empty)}
}
function renderSlots(){
  $('slots').replaceChildren();
  slots.forEach((slot,index)=>{
    const row=document.createElement('div');row.className='slot-row';
    const idWrap=document.createElement('label');idWrap.className='field';const idTitle=document.createElement('span');idTitle.textContent='Device ID';const id=document.createElement('input');id.placeholder='24 caracteres hexadecimais';id.value=slot.device_id||'';id.autocomplete='off';id.oninput=()=>slot.device_id=id.value.trim();const idError=document.createElement('small');idError.className='field-error';idError.dataset.errorFor='slot_'+index;idWrap.append(idTitle,id,idError);
    const targetWrap=document.createElement('label');targetWrap.className='field';const targetTitle=document.createElement('span');targetTitle.textContent='Ação';const target=document.createElement('select');choices(target,slot.boot_id,'slot',sinricSystems);target.onchange=()=>slot.boot_id=target.value;targetWrap.append(targetTitle,target);
    const remove=document.createElement('button');remove.type='button';remove.className='button danger-soft slot-remove';remove.append(icon('trash'));const label=document.createElement('span');label.textContent='Remover';remove.append(label);remove.onclick=()=>{slots.splice(index,1);renderSlots();updateSlotWarning()};
    row.append(idWrap,targetWrap,remove);$('slots').append(row)
  });
  updateSlotWarning()
}
function updateSlotWarning(){$('slotWarning').hidden=!($('f_sinric_enabled').checked&&slots.length===0)}
function renderSinric(){
  fillPcSelect($('sinricPc'),cfg.sinric_pc_id,true);$('sinricPc').dataset.previous=cfg.sinric_pc_id||'';loadSinricCatalog().catch(e=>showToast(e.message,true));
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
  $('setupInfo').textContent='Ajustes do PC e configuração global são salvos separadamente.'
}
async function pcApi(context,path,method='GET',data){
  if(!context.pcId)throw Error('Selecione um PC.');
  const result=await api('pcs/'+context.pcId+(path?'/'+path:''),method,data,method==='GET'?context:null);
  if(!pcModel.current(context))throw Error('STALE_REQUEST');return result;
}
async function catalog(id,context){
  for(let retry=0;retry<3;retry++){
    const entries=[];let generation=null,offset=0,changed=false;
    do{const page=await api('pcs/'+id+'/systems?offset='+offset+'&limit=8','GET',undefined,context);
      if(context&&!pcModel.current(context))throw Error('STALE_REQUEST');
      if(generation!==null&&generation!==page.generation){changed=true;break}generation=page.generation;
      entries.push(...page.systems);offset+=page.systems.length;if(!page.systems.length||offset>=page.total)return entries;
    }while(offset<24);
    if(!changed)return entries;
  }throw Error('Catálogo mudou durante a leitura. Atualize novamente.');
}
function readPcStatus(id){
  if(pcStatusRequests.has(id))return pcStatusRequests.get(id);
  const pc=pcs.find(pc=>pc.id===id);
  const request=api('pcs/'+id+'/status').then(state=>{if(pcs.includes(pc)){pc.status=state;renderPcCards()}return state}).finally(()=>{if(pcStatusRequests.get(id)===request)pcStatusRequests.delete(id)});
  pcStatusRequests.set(id,request);return request;
}
function status(){
  const context=pcModel.capture();if(!context.pcId)return Promise.resolve();
  return readPcStatus(context.pcId).then(state=>{if(pcModel.current(context))applyStatus({...globalStatus,...state});return state});
}
async function pollPcStatuses(){
  for(const pc of [...pcs]){if(!pcs.includes(pc))continue;const context=pcModel.capture();const state=await readPcStatus(pc.id);if(pc.id===context.pcId&&pcModel.current(context))applyStatus({...globalStatus,...state})}
}
function fillPcSelect(select,value,empty=false){
  select.replaceChildren();if(empty){const option=document.createElement('option');option.value='';option.textContent='Nenhum PC';select.append(option)}
  for(const pc of pcs){const option=document.createElement('option');option.value=pc.id;option.textContent=pc.name;select.append(option)}select.value=value||'';
}
function renderPcCards(){
  fillPcSelect($('pcSelect'),pcModel.selectedPcId);$('pcEmpty').hidden=pcs.length>0;$('addPc').disabled=pcs.length>=4;
  $('pcCards').replaceChildren();for(const pc of pcs){const card=document.createElement('button');card.type='button';card.className='pc-card';card.setAttribute('aria-pressed',String(pc.id===pcModel.selectedPcId));const name=document.createElement('strong');name.textContent=pc.name;const info=document.createElement('small');info.textContent=pc.mac+' • '+(pc.status?(pc.status.online?'Online':'Offline'):'Status pendente');card.append(name,info);card.onclick=()=>selectPc(pc.id);$('pcCards').append(card)}
}
function renderPcFields(){
  const pc=pcs.find(pc=>pc.id===pcModel.selectedPcId);$('pcSettingsName').textContent=pc?pc.name:'Selecione ou adicione um PC.';$('pcFields').replaceChildren();
  for(const [key,label,type='text'] of pcFields)if(pc)$('pcFields').append(fieldInput(key,label,type,pc,'pc_'));
  $('savePc').disabled=!pc||!catalogReady;$('removePc').disabled=!pc;$('rescan').disabled=!pc;
}
async function selectPc(id){
  pcModel.select(id);catalogReady=false;systems=[];$('shutdown').disabled=true;renderPcCards();renderPcFields();renderEntries();renderButtons();applyStatus({...globalStatus,online:false,os:id?'Carregando…':'Nenhum PC selecionado'});
  const context=pcModel.capture();if(!id)return;
  try{await Promise.all([status(),catalog(id,context).then(entries=>{if(pcModel.current(context)){systems=entries;catalogReady=true;$('savePc').disabled=false;for(const key of ['default_target','fallback_boot_id']){const input=$('pc_'+key);const value=input.value;input.replaceChildren();choices(input,value,'target')}renderEntries();renderButtons()}})])}catch(e){if(e.message!=='STALE_REQUEST')showToast(e.message,true)}
}
function renderAgents(){
  $('agentList').replaceChildren();if(!agents.length){$('agentList').textContent='Nenhum agent vinculado.';return}
  for(const agent of agents){const row=document.createElement('div');row.className='entry-row';const info=document.createElement('div');const pc=pcs.find(pc=>pc.id===agent.pc_id);const name=document.createElement('strong');name.textContent=(agent.installation_name||agent.hostname||'Agent')+' • '+(pc?.name||'PC removido');const detail=document.createElement('p');detail.textContent=(agent.os||'OS desconhecido')+' • '+(agent.connected?'Conectado':'Aguardando conexão do agent')+' • Último contato: '+(agent.last_seen||'não informado');const permissions=document.createElement('small');permissions.textContent='Permissões: reiniciar '+(agent.permissions?.reboot?'sim':'não')+', desligar '+(agent.permissions?.shutdown?'sim':'não');info.append(name,detail,permissions);
    const sync=document.createElement('button');sync.type='button';sync.className='button secondary';sync.textContent='Sincronizar agent';sync.disabled=!agent.connected;sync.onclick=()=>action(()=>api('pcs/'+agent.pc_id+'/discovery/request','POST',{}),sync,'Sincronização solicitada.');
    const remove=document.createElement('button');remove.type='button';remove.className='button danger-soft';remove.textContent='Remover vínculo';remove.onclick=async()=>{if(!confirm('Revogar '+(agent.installation_name||agent.hostname||'agent')+' de '+(pc?.name||agent.pc_id)+'?'))return;try{await api('agents/'+agent.id,'DELETE');await refreshAgents()}catch(e){showToast(e.message,true)}};row.append(info,sync,remove);$('agentList').append(row)
  }
}
async function refreshAgents(){agents=(await api('agents')).agents.map(agent=>({...agent,id:agent.agent_id||agent.id}));renderAgents()}
async function loadSinricCatalog(){const id=$('sinricPc').value;sinricSystems=[];renderSlots();$('sinricCatalogInfo').textContent=id?'Carregando catálogo do PC escolhido…':'Escolha o único PC controlado pela integração.';if(!id)return;const entries=await catalog(id);if($('sinricPc').value!==id)return;sinricSystems=entries;renderSlots();$('sinricCatalogInfo').textContent=entries.length?'Os dispositivos controlam somente este PC.':'PC sem catálogo. Conecte um agent e sincronize antes de mapear sistemas.'}
async function connect(){
  if(initialSetup){await saveInitialPassword();return}
  token=$('token').value;$('loginError').hidden=true;setBusy($('connect'),true,'Conectando');
  try{
    const initial=await api('bootstrap');storeRememberedLogin(localStorage,token,$('rememberLogin').checked);cfg=initial.config;pcs=(initial.pcs||[]).map(pc=>({...pc,id:pc.pc_id||pc.id}));agents=(initial.agents||[]).map(agent=>({...agent,id:agent.agent_id||agent.id}));globalStatus=initial.status||{};slots=(cfg.sinric_slots||[]).map(slot=>({...slot}));$('login').hidden=true;$('app').hidden=false;renderFields();renderSinric();renderAgents();selectPc(pcs[0]?.id||'');clearInterval(refreshTimer);statusPoller=createStatusPoller({poll:async()=>{try{globalStatus=await api('status');await pollPcStatuses();await refreshAgents();if(!pcModel.selectedPcId)applyStatus({...globalStatus,online:false})}catch(error){showToast(error.message,true)}},isHidden:()=>document.hidden});refreshTimer=setInterval(statusPoller.tick,15000);showToast('Dashboard conectada.')
  }catch(error){if(error.message.includes('FORBIDDEN')||error.message.includes('AUTH_REQUIRED')){storeRememberedLogin(localStorage,'',false);$('rememberLogin').checked=false}$('loginError').textContent=error.message;$('loginError').hidden=false}
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
function collectPatch(){
  const patch={};for(const [,,list] of fields)for(const [key,,type='text'] of list){const element=$('f_'+key);if(type==='password'&&!element.value)continue;patch[key]=type==='checkbox'?element.checked:type==='number'?Number(element.value):element.value}return patch;
}
$('connect').onclick=connect;$('token').addEventListener('keydown',event=>{if(event.key==='Enter')connect()});$('rememberLogin').addEventListener('change',()=>{if(!$('rememberLogin').checked)storeRememberedLogin(localStorage,'',false)});
async function saveInitialPassword(){
  $('loginError').hidden=true;
  const password=$('token').value;
  if(!validateCredential(password)||password!==$('repeatPassword').value){$('loginError').textContent=!validateCredential(password)?'Use de 8 a 128 caracteres sem caracteres de controle.':'As senhas não coincidem.';$('loginError').hidden=false;return}
  setBusy($('connect'),true,'Salvando');
  try{await api('setup','POST',{password,repeat_password:$('repeatPassword').value});storeRememberedLogin(localStorage,'',false);$('token').value='';$('repeatPassword').value='';$('loginHelp').textContent='Senha salva. A ESP está reiniciando…';setTimeout(()=>location.reload(),6500)}
  catch(error){$('loginError').textContent=error.message;$('loginError').hidden=false;setBusy($('connect'),false)}
}
async function initializeLogin(){
  setBusy($('connect'),true,'Carregando');
  try{
    const setup=await api('setup');if(setup.config_locked)throw Error('A configuração salva está incompatível ou corrompida. É necessário recuperar a ESP.');initialSetup=setup.required===true;
    $('repeatPasswordField').hidden=!initialSetup;$('rememberLoginField').hidden=initialSetup;
    $('passwordLabel').textContent=initialSetup?'Senha':'Senha administrativa';
    $('token').autocomplete=initialSetup?'new-password':'current-password';
    $('loginHelp').textContent=initialSetup?'Crie sua senha administrativa para começar.':'Entre com sua senha administrativa.';
    setBusy($('connect'),false);$('connect').querySelector('span').textContent=initialSetup?'Salvar e reiniciar ESP':'Conectar';
    if(!initialSetup){const savedLogin=rememberedLogin(localStorage);if(savedLogin){$('token').value=savedLogin;$('rememberLogin').checked=true;await connect()}}
  }catch(error){$('loginError').textContent=error.message;$('loginError').hidden=false;setBusy($('connect'),false)}
}
$('repeatPassword').addEventListener('keydown',event=>{if(event.key==='Enter')connect()});
initializeLogin();
document.addEventListener('visibilitychange',()=>statusPoller?.visibilityChanged());
document.querySelectorAll('[data-view]').forEach(button=>button.addEventListener('click',()=>setActiveView(button.dataset.view)));
$('refreshStatus').onclick=button=>action(()=>status(),button.currentTarget,'Status atualizado.');
$('showHidden').onchange=renderButtons;
$('addSlot').onclick=()=>{if(slots.length<8){slots.push({device_id:'',boot_id:'default'});renderSlots()}else showToast('O limite é de 8 dispositivos.',true)};
$('settings').onsubmit=async event=>{
  event.preventDefault();const sinric=!$('view-sinric').hidden;const buttons=[...document.querySelectorAll('.save-button')];
  try{
    let patch,path;if(sinric){
      const validation=validateSinric({enabled:$('f_sinric_enabled').checked,appKey:$('f_sinric_app_key').value,appKeySet:!!cfg.sinric_app_key_set,appSecret:$('f_sinric_app_secret').value,appSecretSet:!!cfg.sinric_app_secret_set,slots,validBootIds:sinricSystems.filter(entry=>!entry.blocked).map(entry=>entry.id)});
      if(!validation.valid){showFieldErrors(validation.errors);throw Error('Revise os campos do Sinric.')}if($('f_sinric_enabled').checked&&!$('sinricPc').value)throw Error('Selecione o PC controlado pelo Sinric.');
      patch={sinric_pc_id:$('sinricPc').value,sinric_enabled:$('f_sinric_enabled').checked,sinric_slots:validation.slots};for(const key of ['sinric_app_key','sinric_app_secret'])if($('f_'+key).value)patch[key]=$('f_'+key).value;path='integrations/sinric';
    }else{patch=collectPatch();if(patch.admin_token&&!validateCredential(patch.admin_token))throw Error('Senha administrativa deve conter 8–128 caracteres.');if(!validateTailscaleAuthKey(patch.tailscale_auth_key||''))throw Error('Use uma Auth Key tskey-auth- válida.');path='config'}
    buttons.forEach(button=>setBusy(button,true,'Salvando'));await api(path,'PUT',patch);for(const [key,value] of Object.entries(patch)){if(['admin_token','tailscale_auth_key','sinric_app_key','sinric_app_secret'].includes(key)){cfg[key+'_set']=true;$('f_'+key).value=''}else cfg[key]=value}if(patch.admin_token){token=patch.admin_token;storeRememberedLogin(localStorage,token,$('rememberLogin').checked)}showToast(sinric?'Sinric salvo.':'Configuração global salva.');
  }catch(e){showToast(e.message,true)}finally{buttons.forEach(button=>setBusy(button,false))}
};
$('shutdown').onclick=event=>{const context=pcModel.capture(),pc=pcs.find(pc=>pc.id===context.pcId);if(pc&&confirm('Desligar '+pc.name+'? Salve seu trabalho antes.'))action(()=>pcApi(context,'shutdown','POST',{confirm:'SHUTDOWN'}),event.currentTarget,'Desligamento solicitado.')};
$('rescan').onclick=event=>{const context=pcModel.capture();action(async()=>{await pcApi(context,'discovery/request','POST',{});const entries=await catalog(context.pcId,context);if(pcModel.current(context)){systems=entries;renderEntries();renderButtons()}},event.currentTarget,'Atualização do catálogo solicitada.')};
$('pcSelect').onchange=()=>selectPc($('pcSelect').value);
$('addPc').onclick=()=>{$('pcError').textContent='';$('newPcForm').reset();$('pcDialog').showModal();$('newPcName').focus()};$('cancelPc').onclick=()=>{$('pcDialog').close();$('addPc').focus()};
$('newPcForm').onsubmit=async event=>{event.preventDefault();const button=event.submitter;setBusy(button,true);try{const result=await api('pcs','POST',{name:$('newPcName').value.trim(),mac:$('newPcMac').value.trim(),wol_port:9,wol_repeat:3,wol_interval_ms:100,pending_ttl_s:30,physical_boot_behavior:'default_target',default_target:'',fallback_boot_id:''});result.pc.id=result.pc.pc_id||result.pc.id;pcs.push(result.pc);$('pcDialog').close();pairingUi.updatePcs(result.pc.id);await selectPc(result.pc.id);($('pairingDialog').open?$('pairPc'):$('pcSelect')).focus()}catch(e){$('pcError').textContent=e.message}finally{setBusy(button,false)}};
$('savePc').onclick=async event=>{if(!catalogReady)return;const button=event.currentTarget;const context=pcModel.capture(),patch={};for(const [key,,type] of pcFields){const field=$('pc_'+key);patch[key]=type==='number'?Number(field.value):field.value}const entries=systems.map(entry=>({...entry}));setBusy(button,true);try{const result=await pcApi(context,'','PUT',patch);if(!pcModel.current(context))return;Object.assign(pcs.find(pc=>pc.id===context.pcId),result.pc||patch);await pcApi(context,'systems','PUT',{systems:entries});renderPcCards();showToast('Ajustes do PC salvos.')}catch(e){if(e.message!=='STALE_REQUEST')showToast(e.message,true)}finally{setBusy(button,false);button.disabled=!catalogReady}};
$('removePc').onclick=async()=>{const context=pcModel.capture(),pc=pcs.find(pc=>pc.id===context.pcId);if(!pc||!confirm('Remover '+pc.name+' e revogar todos os seus agents?'))return;try{await api('pcs/'+context.pcId,'DELETE',{confirm:'DELETE_PC'});pcs=pcs.filter(pc=>pc.id!==context.pcId);agents=agents.filter(agent=>agent.pc_id!==context.pcId);if(cfg.sinric_pc_id===context.pcId){cfg.sinric_pc_id='';cfg.sinric_enabled=false;cfg.sinric_slots=[];slots=[];renderSinric()}renderAgents();pairingUi.updatePcs();if(pcModel.current(context))await selectPc(pcs[0]?.id||'');else renderPcCards()}catch(e){showToast(e.message,true)}};
$('sinricPc').onchange=()=>{const previous=$('sinricPc').dataset.previous||'',next=$('sinricPc').value;if(next!==previous&&!confirm('Trocar o PC do Sinric? Todos os mapeamentos anteriores serão apagados ao salvar.')){$('sinricPc').value=previous;return}$('sinricPc').dataset.previous=next;slots=[];renderSlots();loadSinricCatalog().catch(e=>showToast(e.message,true))};
const pairingUi=RemoteBootPairing({api,pcs:()=>pcs,refresh:async()=>{await refreshAgents();showToast('Vínculo aprovado. Aguardando conexão do agent.');setActiveView('system')},onError:text=>showToast(text,true)});
$('pairAgent').onclick=event=>pairingUi.open(event.currentTarget);
$('readLogs').onclick=async event=>{const button=event.currentTarget;setBusy(button,true,'Carregando');try{$('logs').textContent=(await api('logs')).logs.join('\n')||'Nenhum evento registrado.'}catch(error){showToast(error.message,true)}finally{setBusy(button,false)}};
$('restart').onclick=event=>{if(confirm('Reiniciar ESP32?'))action(()=>api('system/reboot','POST',{}),event.currentTarget,'ESP32 reiniciando.')};
$('reset').onclick=event=>{if(prompt('Digite FACTORY_RESET para apagar a configuração')==='FACTORY_RESET')action(()=>api('system/reset','POST',{confirm:'FACTORY_RESET'}),event.currentTarget,'Configuração apagada.')};
}
