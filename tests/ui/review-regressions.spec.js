const {test,expect}=require('@playwright/test');
const {fakeApi,login,A,B}=require('./fake-api');
test('PC save waits for every catalog page',async({page})=>{
 const calls=await fakeApi(page);let secondPage=false,savedSystems;
 await page.route('**/api/v2/pcs/'+B+'/systems?*',async route=>{const offset=Number(new URL(route.request().url()).searchParams.get('offset'));if(offset===8)secondPage=true;await new Promise(resolve=>setTimeout(resolve,500));await route.fulfill({json:{systems:Array.from({length:offset?1:8},(_,index)=>({id:String(offset+index).padStart(4,'0'),name:'Linux '+(offset+index)})),offset,generation:1,total:9}})});
 await page.route('**/api/v2/pcs/'+B+'/systems',async route=>{savedSystems=route.request().postDataJSON().systems;await route.fulfill({json:{saved:true}})});
 await login(page);await page.getByLabel('PC selecionado').selectOption(B);await page.getByRole('button',{name:'Configuração',exact:true}).click();await expect(page.getByRole('button',{name:'Salvar ajustes do PC'})).toBeDisabled();await expect.poll(()=>secondPage).toBeTruthy();await expect(page.getByRole('button',{name:'Salvar ajustes do PC'})).toBeDisabled();expect(calls.some(c=>c.method==='PUT'&&c.path==='pcs/'+B)).toBeFalsy();await expect(page.getByRole('button',{name:'Salvar ajustes do PC'})).toBeEnabled();await page.getByRole('button',{name:'Salvar ajustes do PC'}).click();await expect.poll(()=>savedSystems?.length).toBe(9);
});
test('all PC cards refresh sequentially without overwriting selected status',async({page})=>{
 await fakeApi(page);await page.clock.install();let active=0,maxActive=0,bRequests=0;
 await page.route('**/api/v2/pcs/*/status',async route=>{active++;maxActive=Math.max(maxActive,active);const id=route.request().url().includes(B)?B:A;if(id===B)bRequests++;await new Promise(resolve=>setTimeout(resolve,250));await route.fulfill({json:{online:id===B,os:id===B?'Linux Servidor':'Windows Estúdio',shutdown_enabled:true}});active--});
 await login(page);await expect(page.locator('#cardOs')).toHaveText('Windows Estúdio');await page.clock.fastForward(15000);await expect.poll(()=>bRequests).toBe(1);await expect(page.locator('#pcCards .pc-card').filter({hasText:'Servidor'})).toContainText('Online');expect(maxActive).toBe(1);expect(bRequests).toBe(1);await expect(page.locator('#cardOs')).toHaveText('Windows Estúdio');await expect(page.getByLabel('PC selecionado')).toHaveValue(A);
});
for(const code of ['AUTH_REQUIRED','FORBIDDEN'])test('remembered login clears on '+code,async({page})=>{
 await fakeApi(page);await page.addInitScript(()=>localStorage.setItem('remote-boot-admin-token','old-password'));await page.route('**/api/v2/bootstrap',route=>route.fulfill({status:code==='FORBIDDEN'?403:401,json:{error:code}}));await page.goto('/');await expect(page.locator('#loginError')).toContainText(code);expect(await page.evaluate(()=>localStorage.getItem('remote-boot-admin-token'))).toBeNull();await expect(page.getByLabel('Permanecer conectado neste navegador')).not.toBeChecked();
});
test('discovery exact path works and unknown PC subpaths fail',async({page})=>{
 const calls=await fakeApi(page);await login(page);await page.getByRole('button',{name:'Boot',exact:true}).click();await page.getByRole('button',{name:'Atualizar catálogo',exact:true}).click();await expect.poll(()=>calls.some(c=>c.path==='pcs/'+A+'/discovery/request'&&c.method==='POST')).toBeTruthy();const response=await page.evaluate(async id=>{const response=await fetch('/api/v2/pcs/'+id+'/discovery/typo',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});return {status:response.status,body:await response.json()}},A);expect(response).toEqual({status:404,body:{error:'NOT_FOUND'}});
});
