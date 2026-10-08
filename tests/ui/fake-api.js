const A='a'.repeat(32),B='b'.repeat(32);
async function fakeApi(page,{empty=false,delayed=false}={}){
 const calls=[];let pcs=empty?[]:[{id:A,name:'Estúdio',mac:'AA:BB:CC:DD:EE:01',wol_port:9,wol_repeat:3,wol_interval_ms:100,pending_ttl_s:30,physical_boot_behavior:'default_target'},{id:B,name:'Servidor',mac:'AA:BB:CC:DD:EE:02',wol_port:9,wol_repeat:3,wol_interval_ms:100,pending_ttl_s:30,physical_boot_behavior:'default_target'}];let agents=[];
 await page.route('**/api/v2/**',async route=>{
  const request=route.request(),path=new URL(request.url()).pathname.replace('/api/v2/','');const method=request.method(),body=request.postDataJSON();calls.push({path,method,body});let data={};let status=200;
  if(path==='setup')data={required:false,config_locked:false};
  else if(path==='status')data={ip:'192.168.1.10',rssi:-50,version:'test',tailscale:{built:false}};
  else if(path==='bootstrap')data={config:{dhcp:true,sinric_pc_id:A,sinric_slots:[{device_id:'1'.repeat(24),boot_id:'0001'}]},pcs:pcs.map(({id,...pc})=>({pc_id:id,...pc})),agents,status:{ip:'192.168.1.10',rssi:-50,version:'test',tailscale:{built:false}}};
  else if(path==='pcs'&&method==='POST'){const pc={id:B,...body};pcs.push(pc);data={pc:{...pc,pc_id:pc.id}}}
  else if(path==='pcs')data={pcs};
  else if(path==='agents')data={agents:agents.map(({id,...agent})=>({agent_id:id,...agent}))};
  else if(path.startsWith('pairing/lookup'))data={pairing_id:'c'.repeat(32),hostname:'desktop-test',os:'Linux',expires_in:300};
  else if(path.endsWith('/approve')){agents.push({id:'d'.repeat(32),pc_id:body.pc_id,installation_name:body.installation_name,os:'Linux',connected:false});data={approved:true}}
  else if(path.startsWith('pcs/')){
   const match=path.match(/^pcs\/([a-f0-9]{32})(?:\/(.*))?$/),pc=match&&pcs.find(pc=>pc.id===match[1]),action=match?.[2]||'';
   if(!pc){status=404;data={error:'PC_NOT_FOUND'}}
   else if(action==='systems'&&method==='GET')data={systems:[{id:'0001',name:pc.id===A?'Windows Estúdio':'Linux Servidor'}],offset:0,generation:1,total:1};
   else if(action==='status'&&method==='GET'){if(delayed&&pc.id===A)await new Promise(resolve=>setTimeout(resolve,600));data={online:pc.id===B,os:pc.id===A?'Windows Estúdio':'Linux Servidor',shutdown_enabled:true}}
   else if(!action&&method==='PUT'){Object.assign(pc,body);data={saved:true}}
   else if(!action&&method==='DELETE'){if(body?.confirm!=='DELETE_PC'){status=400;data={error:'CONFIRM_REQUIRED'}}else pcs=pcs.filter(candidate=>candidate!==pc)}
   else if(action==='systems'&&method==='PUT'){if(body?.systems?.length!==1){status=400;data={error:'CATALOG_MISMATCH'}}else data={saved:true}}
   else if(['discovery/request','discovery','boot','reboot','shutdown'].includes(action)&&method==='POST')data={queued:true};
   else{status=404;data={error:'NOT_FOUND'}}
  }
  else if(path==='integrations/sinric')data={ok:true};
  try{await route.fulfill({status,contentType:'application/json',body:JSON.stringify(data)})}catch(error){if(!error.message.includes('closed'))throw error}
 });return calls;
}
async function login(page){await page.goto('/');await page.getByLabel('Senha administrativa',{exact:true}).fill('admin-password');await page.getByRole('button',{name:'Conectar',exact:true}).click();}
module.exports={fakeApi,login,A,B};
