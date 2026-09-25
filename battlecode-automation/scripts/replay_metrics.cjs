// Uses the parser shipped with the installed toolkit, extracted by research.py.
const fs = require('fs');
require(process.argv[2]);
const d = decodeBattlecode(fs.readFileSync(process.argv[3]));
const stats = {};
const teams = new Map();
const heads = new Map();
const alive = new Set();
const failures = {A:new Set(),B:new Set()};
for (const t of ['A', 'B']) stats[t] = {first_split_round:null,max_units:0,portal_entries:0,sacrifice_count:0,runtime_failures:0,max_turn_points:null,deaths:{},visited:new Set(),vision:new Set(),longest_trajectory:[]};
const map = d.match.map;
for (const b of map.initialDragons) { teams.set(b.id,b.team); heads.set(b.id,b.body[0]); alive.add(b.id); }
let round = 0;
for (const e of d.events) {
  if(e.type==='roundStart') round=e.round;
  if(e.type==='dragonSplit') { teams.set(e.childId,e.team);heads.set(e.childId,e.childBody[0]);heads.set(e.parentId,e.parentBody[0]);alive.add(e.childId); if(stats[e.team].first_split_round===null) stats[e.team].first_split_round=round; }
  if(e.type==='dragonDeath') alive.delete(e.id);
  if(e.type==='dragonSplit'||e.type==='dragonDeath'||e.type==='roundStart') {
    for(const t of ['A','B']) stats[t].max_units=Math.max(stats[t].max_units,[...alive].filter(id=>teams.get(id)===t).length);
  }
  const team=teams.get(e.id), s=stats[team];
  if(!s) continue;
  if(e.type==='turnStart') {
    const head=heads.get(e.id);
    if(head) for(let y=-3;y<=3;y++) for(let x=-3;x<=3;x++) s.vision.add(`${(head.x+x+map.width)%map.width},${(head.y+y+map.height)%map.height}`);
  }
  if(e.type==='dragonAction') {
    if(e.tle||e.instructions?.exceeded||!e.action) failures[team].add(`${round}:${e.id}`);
    if(e.instructions) s.max_turn_points=Math.max(s.max_turn_points||0,e.instructions.count);
  }
  if(e.type==='dragonDeath') s.deaths[e.reason]=(s.deaths[e.reason]||0)+1;
  if(e.type==='dragonDeath' && e.reason==='A') failures[team].add(`${round}:${e.id}`);
  if(e.type==='dragonLog' && /^FEED /.test(e.text.trim())) s.sacrifice_count++;
  if(e.type==='dragonUpdate' && e.head) {
    const old=heads.get(e.id);
    if(old && (old.x!==e.head.x||old.y!==e.head.y)) {
      const dir=e.facing;
      const x=(old.x+(dir==='E'?1:0))%map.width;
      const y=(old.y+(dir==='S'?1:0))%map.height;
      const key=(dir==='N'||dir==='S'?'H':'V')+`:${x},${y}`;
      const exit=map.portalMap.get(key);
      if(exit) {
        const dest={x:(exit.x+(dir==='W'?-1:0)+map.width)%map.width,y:(exit.y+(dir==='N'?-1:0)+map.height)%map.height};
        if(dest.x===e.head.x&&dest.y===e.head.y) s.portal_entries++;
      }
    }
    heads.set(e.id,e.head);
    s.visited.add(`${e.head.x},${e.head.y}`);
  }
  // Unknown event encodings are deliberately not guessed as portal traversals.
}
for(let r=0;r<=d.match.maxRound;r++) {
  const bodies=[...d.match.roundAt(r).bodies.dragons.values()];
  for(const team of ['A','B']) {
    const bs=bodies.filter(b=>b.team===team),s=stats[team];
    s.max_units=Math.max(s.max_units,bs.length);
    if(r%25===0||r===d.match.maxRound) s.longest_trajectory.push([r,Math.max(0,...bs.map(b=>b.body.length))]);
  }
}
for(const team of ['A','B']) {
  const s=stats[team];s.unique_head_tiles=s.visited.size;s.exploration_coverage=s.vision.size/(map.width*map.height);
  delete s.visited;delete s.vision;
  s.king_length=null;s.portal_deaths=null;
  s.runtime_failures=failures[team].size;
  Object.assign(s,d.result[team==='A'?'teamA':'teamB']);
}
console.log(JSON.stringify({metrics_schema:5,result:d.result,rounds:d.match.maxRound,seed:String(d.seed),teams:stats}));
