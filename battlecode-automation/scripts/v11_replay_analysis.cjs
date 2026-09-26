// Supplementary exact event metrics, separate from historical match records.
const fs=require('fs');require(process.argv[2]);
const d=decodeBattlecode(fs.readFileSync(process.argv[3]));
const bodies=new Map(),teams=new Map();
for(const b of d.match.map.initialDragons){bodies.set(b.id,b.body.map(p=>({...p})));teams.set(b.id,b.team);}
const out={A:{peak:0,long_deaths:[],sprints:0,sprint_steps:0,cycle_ordered_turns:0,long_sprints:0},B:{peak:0,long_deaths:[],sprints:0,sprint_steps:0,cycle_ordered_turns:0,long_sprints:0}};
let round=0;const claims=new Map();
for(const e of d.events){
 if(e.type==='roundStart')round=e.round;
 if(e.type==='dragonSplit') {bodies.set(e.parentId,e.parentBody);bodies.set(e.childId,e.childBody);teams.set(e.childId,e.team);}
 const team=teams.get(e.id),s=out[team],b=bodies.get(e.id);
 if(!s)continue;
 if(e.type==='dragonUpdate'&&b){
   const eq=(a,b)=>a&&b&&a.x===b.x&&a.y===b.y;
   if(e.head&&!eq(b[0],e.head))b.unshift(e.head);
   if(e.tail)while(b.length&&!eq(b[b.length-1],e.tail))b.pop();
   s.peak=Math.max(s.peak,b.length);
 }
 if(e.type==='dragonAction'&&e.action?.kind==='move'){
   const claim=claims.get(e.id);
   if(claim&&b?.length>=8){
     const ranks=b.map(p=>claim.get(`${p.x},${p.y}`));
     if(ranks.every(r=>r!==undefined)){
       const n=claim.size,h=ranks[0];
       const ordered=sign=>{let previous=-1;return ranks.every(r=>{const gap=((h-r)*sign+n)%n;if(gap<=previous)return false;previous=gap;return true;});};
       if(ordered(1)||ordered(-1))s.cycle_ordered_turns++;
     }
   }
   const ds=e.action.steps;
   if(ds&&ds.length>1){s.sprints++;s.sprint_steps+=ds.length;if(b?.length>=8)s.long_sprints++;}
 }
 if(e.type==='dragonDeath'){
   if(b?.length>=12)s.long_deaths.push({round,id:e.id,reason:e.reason,length:b.length});
   bodies.delete(e.id);
 }
 if(e.type==='dragonLog'&&e.text.startsWith('CLAIM ')){
   const [x,y,w,h]=e.text.trim().split(/\s+/).slice(1).map(Number);const cells=[];
   for(let yy=0;yy<h;yy++)if(yy%2===0){for(let xx=yy===0?0:1;xx<w;xx++)cells.push([x+xx,y+yy]);}else{for(let xx=w-1;xx>=1;xx--)cells.push([x+xx,y+yy]);}
   for(let yy=h-1;yy>=1;yy--)cells.push([x,y+yy]);claims.set(e.id,new Map(cells.map((p,i)=>[p.join(','),i])));
 }
}
for(let r=0;r<=d.match.maxRound;r++)for(const b of d.match.roundAt(r).bodies.dragons.values())out[b.team].peak=Math.max(out[b.team].peak,b.body.length);
console.log(JSON.stringify(out));
