'use strict';
const assert=require('node:assert/strict');
require('../firmware/web/app.js');

const makePoller=globalThis.RemoteBootValidation.createStatusPoller;
const statusControls=globalThis.RemoteBootValidation.statusControls;

assert.deepEqual(statusControls({shutdown_enabled:true}),{shutdownDisabled:false});
assert.deepEqual(statusControls({shutdown_enabled:false}),{shutdownDisabled:true});

async function flush(){await Promise.resolve();await Promise.resolve()}

(async()=>{
  let hidden=false,calls=0,resolveRequest;
  const poller=makePoller({
    poll:()=>{calls++;return new Promise(resolve=>{resolveRequest=resolve})},
    isHidden:()=>hidden
  });

  poller.tick();poller.tick();
  assert.equal(calls,1,'a pending status request prevents overlap');
  resolveRequest();await flush();
  poller.tick();assert.equal(calls,2,'polling resumes after completion');
  resolveRequest();await flush();

  hidden=true;poller.tick();
  assert.equal(calls,2,'hidden tabs do not poll');
  hidden=false;poller.visibilityChanged();
  assert.equal(calls,3,'becoming visible refreshes immediately');
  resolveRequest();await flush();
  console.log('PASS: nonoverlapping visibility-aware dashboard polling');
})().catch(error=>{console.error(error);process.exitCode=1});
