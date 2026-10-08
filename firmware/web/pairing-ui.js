'use strict';
globalThis.RemoteBootPairing=function({api,pcs,refresh,onError}){
  const $=id=>document.getElementById(id);const dialog=$('pairingDialog');let pairing=null,timer=null,epoch=0,busy=false,approving=false,opener;
  const error=text=>{$('pairError').textContent=text;$('pairError').hidden=!text};
  const close=async(approved=false)=>{if(approving&&!approved)return;const pending=pairing;epoch++;clearInterval(timer);pairing=null;dialog.close();opener?.focus();try{if(pending&&!approved)await api('pairing/'+pending.pairing_id,'DELETE')}catch(e){onError(e.message)}try{await api('pairing/window','DELETE')}catch(e){onError(e.message)}$('pairCode').value='';};
  dialog.addEventListener('cancel',event=>{event.preventDefault();close()});$('pairCancel').onclick=()=>close();
  $('pairLookup').onclick=async()=>{
    if(busy)return;busy=true;pairing=null;$('pairReview').hidden=true;clearInterval(timer);const generation=epoch;error('');$('pairApprove').disabled=true;
    try{
      const code=$('pairCode').value.toUpperCase().replace(/[-\s]/g,'');if(!/^[0-9A-HJKMNP-TV-Z]{8}$/.test(code))throw Error('Informe os 8 caracteres do código do instalador.');
      const result=await api('pairing/lookup','POST',{code});if(generation!==epoch)return;
      pairing=result;$('pairDetails').textContent=result.hostname+' • '+result.os;$('pairReview').hidden=false;$('pairConfirmed').checked=false;
      let remaining=Number(result.expires_in)||0;clearInterval(timer);const tick=()=>{$('pairTimer').textContent='Expira em '+remaining+' s';if(remaining<=0){pairing=null;$('pairApprove').disabled=true;error('Código expirado. Reinicie o pareamento no instalador.');clearInterval(timer)}remaining--};tick();timer=setInterval(tick,1000);
      $('pairConfirmed').focus();
    }catch(e){if(generation===epoch)error(e.message)}finally{busy=false}
  };
  $('pairConfirmed').onchange=()=>{$('pairApprove').disabled=!pairing||!$('pairConfirmed').checked};
  $('pairApprove').onclick=async()=>{
    if(!pairing||!$('pairConfirmed').checked||busy)return;busy=true;approving=true;const generation=epoch,id=pairing.pairing_id,pcId=$('pairPc').value;const pc=pcs().find(pc=>pc.id===pcId);
    if(!pc){busy=false;approving=false;error('Selecione um PC cadastrado.');return}
    $('pairApprove').disabled=true;
    try{await api('pairing/'+id+'/approve','POST',{pc_id:pcId,installation_name:$('pairName').value.trim()||pairing.hostname});if(generation!==epoch)return;await close(true);await refresh();}
    catch(e){if(generation===epoch){error(e.message);$('pairApprove').disabled=!pairing}}finally{busy=false;approving=false}
  };
  $('pairCreatePc').onclick=()=>{$('pcError').textContent='';$('newPcForm').reset();$('pcDialog').showModal();$('newPcName').focus()};
  return {async open(button){
    if(dialog.open)return;opener=button;epoch++;pairing=null;error('');$('pairWindowStatus').textContent='Abrindo janela…';$('pairCode').value='';$('pairName').value='';$('pairReview').hidden=true;$('pairApprove').disabled=true;
    $('pairPc').replaceChildren(...pcs().map(pc=>{const option=document.createElement('option');option.value=pc.id;option.textContent=pc.name;return option}));
    dialog.showModal();$('pairCode').focus();const generation=epoch;
    try{await api('pairing/window','POST',{});if(generation===epoch)$('pairWindowStatus').textContent='Janela aberta por 5 minutos. Execute o instalador no computador.'}catch(e){if(generation===epoch)error(e.message)}
  },updatePcs(selectedId){const selected=selectedId||$('pairPc').value;$('pairPc').replaceChildren(...pcs().map(pc=>{const option=document.createElement('option');option.value=pc.id;option.textContent=pc.name;return option}));if(pcs().some(pc=>pc.id===selected))$('pairPc').value=selected}};
};
