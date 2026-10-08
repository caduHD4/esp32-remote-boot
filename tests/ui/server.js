const http=require('node:http'),fs=require('node:fs');
const root=process.cwd();let html=fs.readFileSync(root+'/firmware/web/index.html','utf8');
html=html.replace('{{APP_CSS}}',()=>fs.readFileSync(root+'/firmware/web/app.css','utf8')).replace('{{APP_JS}}',()=>['pc-model.js','pairing-ui.js','app.js'].filter(name=>fs.existsSync(root+'/firmware/web/'+name)).map(name=>fs.readFileSync(root+'/firmware/web/'+name,'utf8')).join('\n'));
http.createServer((req,res)=>{res.setHeader('Content-Type','text/html');res.end(html)}).listen(4173,'127.0.0.1');
