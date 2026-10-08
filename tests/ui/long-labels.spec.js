const {test,expect}=require('@playwright/test');
const {fakeApi,login}=require('./fake-api');
test('63-character unbroken system labels stay inside quick boot and boot cards',async({page})=>{
 await fakeApi(page);
 await page.route('**/api/v2/pcs/*/systems?*',route=>route.fulfill({json:{systems:Array.from({length:3},(_,index)=>({id:String(index+1).padStart(4,'0'),name:String(index+1)+'x'.repeat(62)})),offset:0,generation:1,total:3}}));
 await login(page);await expect(page.locator('#overviewBoot .quick-item')).toHaveCount(3);
 const overflow=selector=>page.locator(selector).evaluateAll(items=>items.some(item=>{const text=item.querySelector('strong'),outer=item.getBoundingClientRect(),inner=text.getBoundingClientRect();return text.scrollWidth>text.clientWidth+1||inner.right>outer.right+1||inner.left<outer.left-1}));
 expect(await overflow('#overviewBoot .quick-item')).toBe(false);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 await page.getByRole('button',{name:'Boot',exact:true}).click();await expect(page.locator('#buttons .boot-card')).toHaveCount(3);
 expect(await overflow('#buttons .boot-card')).toBe(false);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 await page.getByRole('button',{name:'Configuração',exact:true}).click();
 expect(await overflow('#entries .entry-row')).toBe(false);
});
