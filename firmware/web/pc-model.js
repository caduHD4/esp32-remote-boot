'use strict';
globalThis.RemoteBootPcModel=function(){
  let selectedPcId='',requestGeneration=0;const controllers=new Set();
  return {
    get selectedPcId(){return selectedPcId},
    select(id){selectedPcId=id;requestGeneration++;for(const controller of controllers)controller.abort();controllers.clear()},
    capture(){return {pcId:selectedPcId,generation:requestGeneration}},
    current(context){return context.pcId===selectedPcId&&context.generation===requestGeneration},
    track(controller){controllers.add(controller);return ()=>controllers.delete(controller)}
  };
};
