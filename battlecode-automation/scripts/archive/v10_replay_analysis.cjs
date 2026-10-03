// Replay-grounded feeding accounting. No inference of intent without FEED logs.
const fs=require('fs');
require(require('path').resolve(process.argv[2]));
const d=decodeBattlecode(fs.readFileSync(process.argv[3]));
const teams=new Map(d.match.map.initialDragons.map(b=>[b.id,b.team]));
const stats=Object.fromEntries(['A','B'].map(t=>[t,{feedIntents:0,confirmedSelfFeeds:0,droppedFeedPearls:0,recipientCollected:0,otherAllyCollected:0,enemyCollected:0,claims:0,growerReversals:0,patrolSteps:0}]));
const heads=new Map(d.match.map.initialDragons.map(b=>[b.id,b.body[0]]));
const claims=new Map(),intents=new Map(),pearls=new Map();
let round=0,actor=null,dropping=null;
const key=p=>`${p.x},${p.y}`;
for(const e of d.events){
  if(e.type==='roundStart'){round=e.round;dropping=null;actor=null;}
  if(e.type==='turnStart'){actor=e.id;dropping=null;}
  if(e.type==='dragonSplit'){
    teams.set(e.childId,e.team);heads.set(e.parentId,e.parentBody[0]);heads.set(e.childId,e.childBody[0]);
  }
  const team=teams.get(e.id),s=stats[team];
  if(e.type==='dragonLog'&&s){
    const words=e.text.trim().split(/\s+/);
    if(words[0]==='FEED'){s.feedIntents++;intents.set(e.id,{recipient:Number(words[1]),team,round});}
    if(words[0]==='GROWER_REVERSE')s.growerReversals++;
    if(words[0]==='CLAIM'){
      s.claims++;const [x,y,w,h]=words.slice(1).map(Number),cycle=[];
      for(let yy=0;yy<h;yy++)if(yy%2===0){for(let xx=yy===0?0:1;xx<w;xx++)cycle.push({x:x+xx,y:y+yy});}
      else {for(let xx=w-1;xx>=1;xx--)cycle.push({x:x+xx,y:y+yy});}
      for(let yy=h-1;yy>=1;yy--)cycle.push({x,y:y+yy});
      claims.set(e.id,new Map(cycle.map((p,i)=>[key(p),key(cycle[(i+1)%cycle.length])])));
    }
  }
  if(e.type==='dragonDeath'&&s){
    const intent=intents.get(e.id);
    if(intent&&intent.round===round&&e.reason==='S'){s.confirmedSelfFeeds++;dropping=intent;}
  }
  if(e.type==='tileChange'){
    const k=key(e.tile);
    if(e.hasPearl&&dropping){pearls.set(k,dropping);stats[dropping.team].droppedFeedPearls++;}
    if(!e.hasPearl&&pearls.has(k)){
      const feed=pearls.get(k),st=stats[feed.team];
      if(actor===feed.recipient)st.recipientCollected++;
      else if(teams.get(actor)===feed.team)st.otherAllyCollected++;
      else st.enemyCollected++;
      pearls.delete(k);
    }
  }
  if(e.type==='dragonUpdate'&&e.head){
    const old=heads.get(e.id);
    if(s&&old&&claims.get(e.id)?.get(key(old))===key(e.head))s.patrolSteps++;
    heads.set(e.id,e.head);
  }
}
console.log(JSON.stringify({replay:process.argv[3],result:d.result,stats,uncollectedTrackedFeedPearls:pearls.size},null,2));
