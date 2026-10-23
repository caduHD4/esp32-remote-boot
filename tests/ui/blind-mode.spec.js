const {test,expect}=require('@playwright/test');
const {fakeApi,login,A,B}=require('./fake-api');
test('blind mode is opt-in when registering a PC',async({page})=>{
 const calls=await fakeApi(page,{empty:true});await login(page);
 await page.getByRole('button',{name:'Adicionar PC',exact:true}).click();
 await expect(page.locator('#newPcBlindMode')).not.toBeChecked();
 await page.getByLabel('Nome do PC',{exact:true}).fill('Modo dinâmico');
 await page.getByLabel('MAC Ethernet',{exact:true}).fill('AA:BB:CC:DD:EE:02');
 await page.locator('#newPcBlindMode').check();
 await page.getByRole('button',{name:'Criar PC',exact:true}).click();
 await expect.poll(()=>calls.some(c=>c.path==='pcs'&&c.method==='POST'&&c.body.blind_mode===true)).toBeTruthy();
 await expect(page.getByLabel('PC selecionado')).toHaveValue(B);
});
test('existing PC mode can be requested without changing DHCP or creating pairing',async({page})=>{
 const calls=await fakeApi(page);await login(page);
 await page.getByRole('button',{name:'Configuração',exact:true}).click();
 await expect(page.locator('#pc_blind_mode')).not.toBeChecked();await page.locator('#pc_blind_mode').check();
 await page.getByRole('button',{name:'Salvar ajustes do PC'}).click();
 await expect.poll(()=>calls.some(c=>c.path===`pcs/${A}`&&c.method==='PUT'&&c.body.blind_mode===true)).toBeTruthy();
 expect(calls.some(c=>c.path==='config'&&c.method==='PUT')).toBeFalsy();
 expect(calls.some(c=>c.path.startsWith('pairing/'))).toBeFalsy();
});
test('profile export uses authenticated API and downloads public metadata',async({page})=>{
 const calls=await fakeApi(page);await login(page);
 const profile={profile_version:1,protocol:2,discovery_version:1,pc_id:A,blind_mode:true,mode_revision:2,device_id:'d'.repeat(32),public_key:'04'+'1'.repeat(128),fingerprint:'f'.repeat(64),last_ipv4:'192.168.1.10',http_port:80,ws_port:81,hostname:'remote-boot-dddddddddddd.local'};
 await page.route(`**/api/v2/pcs/${A}/discovery-profile`,async route=>{
  expect(route.request().headers().authorization).toBe('Bearer admin-password');
  await route.fulfill({contentType:'application/json',body:JSON.stringify(profile)});
 });
 await page.getByRole('button',{name:'Configuração',exact:true}).click();
 const ready=page.waitForEvent('download');await page.locator('#exportProfile').click();const download=await ready;
 expect(download.suggestedFilename()).toBe(`remote-boot-${A}-profile.json`);
 const fs=require('fs');const saved=JSON.parse(fs.readFileSync(await download.path(),'utf8'));
 expect(saved).toEqual(profile);expect(saved).not.toHaveProperty('admin_token');expect(saved).not.toHaveProperty('token');
 expect(calls.some(c=>c.method==='POST'&&c.path.startsWith('pairing/'))).toBeFalsy();
});
