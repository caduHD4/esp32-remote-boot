'use strict';
const assert=require('node:assert/strict');
require('../firmware/web/app.js');

const format=globalThis.RemoteBootValidation.formatTailscaleStatus;

assert.deepEqual(format({built:false,configured:false,state:'disabled'}),{
  label:'DESATIVADO',detail:'Firmware padrão',tone:'neutral'
});
assert.deepEqual(format({built:true,configured:false,state:'not_configured'}),{
  label:'NÃO CONFIGURADO',detail:'Adicione a chave no build',tone:'warning'
});
assert.deepEqual(format({built:true,configured:true,state:'registering'}),{
  label:'REGISTRANDO',detail:'Aguardando a tailnet',tone:'warning'
});
assert.deepEqual(format({built:true,configured:true,connected:true,state:'connected',ip:'100.64.1.2',peers:3}),{
  label:'CONECTADO',detail:'100.64.1.2 • 3 peers',tone:'online'
});
assert.deepEqual(format({built:true,configured:true,state:'error'}),{
  label:'ERRO',detail:'Acesso local preservado',tone:'error'
});

console.log('PASS: Tailscale dashboard status formatting');
assert.deepEqual(format({built:true,configured:true,connected:false,control_online:true,state:'peer_wait',derp_online:true}),{
  label:'CONTROLE ONLINE',detail:'Relay online • Sem tráfego autenticado recente',tone:'warning'
});
assert.equal(format({built:true,configured:true,connected:true,state:'wifi_offline'}).label,'AGUARDANDO WI-FI');
