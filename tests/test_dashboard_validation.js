'use strict';
const assert=require('node:assert/strict');
require('../firmware/web/app.js');

const validate=globalThis.RemoteBootValidation.validateSinric;
const credential=globalThis.RemoteBootValidation.validateCredential;
assert.equal(credential('12345678'),true);
assert.equal(credential('senha !@#'),true);
assert.equal(credential('1234567'),false);
assert.equal(credential('linha\nquebrada'),false);
const base={enabled:false,appKey:'',appKeySet:false,appSecret:'',appSecretSet:false,slots:[],validBootIds:['0000','0008']};
const check=overrides=>validate({...base,...overrides});

let result=check({slots:[{device_id:'',boot_id:'default'}]});
assert.equal(result.valid,true);
assert.deepEqual(result.slots,[]);

result=check({enabled:true});
assert.equal(result.valid,false);
assert.match(result.errors.sinric_app_key,/App Key/);
assert.match(result.errors.sinric_app_secret,/App Secret/);

result=check({enabled:true,appKeySet:true,appSecretSet:true});
assert.equal(result.valid,true);
assert.equal(result.warnings.no_slots,true);

result=check({enabled:true,appKey:'abcdefghij',appSecret:'abcdefghij',slots:[{device_id:'abc',boot_id:'default'}]});
assert.equal(result.valid,false);
assert.match(result.errors.slot_0,/24/);

const id='0123456789abcdef01234567';
result=check({enabled:true,appKey:'abcdefghij',appSecret:'abcdefghij',slots:[{device_id:id,boot_id:'default'},{device_id:id.toUpperCase(),boot_id:'shutdown'}]});
assert.equal(result.valid,false);
assert.match(result.errors.slot_1,/duplicado/);

result=check({enabled:true,appKey:'abcdefghij',appSecret:'abcdefghij',slots:[{device_id:id,boot_id:'9999'}]});
assert.equal(result.valid,false);
assert.match(result.errors.slot_0,/ação/);

result=check({enabled:true,appKey:'abcdefghij',appSecret:'abcdefghij',slots:[{device_id:id,boot_id:'0008'}]});
assert.equal(result.valid,true);
assert.deepEqual(result.slots,[{device_id:id,boot_id:'0008'}]);

console.log('PASS: browser-side Sinric validation');
