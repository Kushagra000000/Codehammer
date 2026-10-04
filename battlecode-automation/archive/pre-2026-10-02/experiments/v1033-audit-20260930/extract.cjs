const fs=require('fs'), path=require('path');
require(path.resolve(process.argv[2]));
const d=decodeBattlecode(fs.readFileSync(process.argv[3])), model=d.match.roundAt(0).bodies.copy(), turns=[],deaths=[],splits=[],drops=new Map(),collections=[];
let round=-1,active=null,drop=null;const last=new Map();
const dist=(a,b)=>Math.max(Math.min(Math.abs(a.x-b.x),d.match.map.width-Math.abs(a.x-b.x)),Math.min(Math.abs(a.y-b.y),d.match.map.height-Math.abs(a.y-b.y)));
const summary=b=>({id:b.id,team:b.team,length:b.body.length,head:{...b.body[0]}});
const canon=m=>JSON.stringify([...m.dragons.values()].sort((a,b)=>a.id-b.id).map(b=>[b.id,b.team,b.facing,b.body]));
for(const e of d.events){
 if(e.type==='roundStart'){round=e.round;active=null;drop=null;if(canon(model)!==canon(d.match.roundAt(round).bodies))throw Error('snapshot mismatch '+round);}
 if(round<0)continue;
 if(e.type==='turnStart'){drop=null;const b=model.dragons.get(e.id);active={round,id:e.id,...(b?summary(b):{}),logs:[],action:null};turns.push(active);last.set(e.id,active);}
 if(e.type==='dragonLog'&&active&&active.id===e.id)active.logs.push(e.text.trim());
 if(e.type==='dragonAction'&&active&&active.id===e.id){active.action=e.action;active.tle=e.tle;}
 if(e.type==='dragonSplit')splits.push({round,parent:e.parentId,child:e.childId,childLength:e.childBody.length,team:e.team,logs:last.get(e.parentId)?.logs});
 if(e.type==='dragonDeath'){
  const b=model.dragons.get(e.id);if(!b)throw Error('missing death body');const own=last.get(e.id),logs=own?.round===round?own.logs:[];
  const rec={round,...summary(b),reason:e.reason,actor:active?.id,logs,priorTurns:turns.filter(t=>t.id===e.id).slice(-4),nearby:[...model.dragons.values()].filter(v=>v.id!==e.id&&dist(v.body[0],b.body[0])<=6).map(summary)};
  deaths.push(rec);drop=rec;
 }
 if(e.type==='tileChange'){
  const k=`${e.tile.x},${e.tile.y}`;
  if(e.hasPearl&&drop)drops.set(k,{deathId:drop.id,deathRound:drop.round,donorTeam:drop.team});
  else if(!e.hasPearl&&drops.has(k)){
   const v=drops.get(k);collections.push({...v,round,collector:active?.id,collectorTeam:active?.team,tile:e.tile});drops.delete(k);
  }
 }
 model.applyEvent(e);
}
for(const x of deaths){const ids=new Set(x.nearby.map(b=>b.id));x.nearbyActions=turns.filter(t=>ids.has(t.id)&&t.round>=x.round-1&&t.round<=x.round+2);x.dropCollections=collections.filter(c=>c.deathId===x.id&&c.deathRound===x.round);}
const result={replay:process.argv[3],seed:String(d.seed),result:d.result,deathCount:deaths.length,deaths,splits,turns};
fs.writeFileSync(process.argv[4],JSON.stringify(result));
for(const team of ['A','B']){
 const ds=deaths.filter(x=>x.team===team);const counts={};for(const x of ds)counts[x.reason]=(counts[x.reason]||0)+1;
 const ownLog=x=>x.logs.find(l=>/^(FEED|REMNANT_CLEAR|TRAPPED_SELF)/.test(l));
 console.log(team,JSON.stringify({deaths:ds.length,reasons:counts,classified:ds.reduce((a,x)=>{const k=ownLog(x)?.split(' ')[0]||'other';a[k]=(a[k]||0)+1;return a;},{}),splits:splits.filter(x=>x.team===team).length}));
 console.log('LONG',JSON.stringify(ds.filter(x=>x.length>=12).map(({round,id,length,reason,actor,logs,dropCollections})=>({round,id,length,reason,actor,logs,collections:dropCollections.length}))))
}
console.log(JSON.stringify(d.result));
