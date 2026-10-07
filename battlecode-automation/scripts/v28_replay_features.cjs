// Compact action verification using the installed viewer decoder extracted by v28_eval.py.
// node v28_replay_features.cjs /absolute/decoder.cjs matches.jsonl [more matches.jsonl]
const fs = require('fs');
require(process.argv[2]);
const output = [];
for (const file of process.argv.slice(3)) {
  for (const line of fs.readFileSync(file, 'utf8').trim().split('\n')) {
    if (!line) continue;
    const r = JSON.parse(line);
    if (r.status !== 'ok') continue;
    const d = decodeBattlecode(fs.readFileSync(r.replay));
    const teams = new Map([...d.match.roundAt(0).bodies.dragons].map(([id,b]) => [id,b.team]));
    let actor = null, round = -1, lane = false, feed = false;
    const counts = {lane_intents:0, lane_splits:0, feed_intents:0, feed_deaths:0};
    const examples = [];
    for (const e of d.events) {
      if (e.type === 'roundStart') round = e.round;
      if (e.type === 'turnStart') { actor=e.id; lane=false; feed=false; }
      if (e.type === 'dragonSplit') teams.set(e.childId, e.team);
      if (teams.get(actor) !== r.candidate_side) continue;
      if (e.type === 'dragonLog' && e.id === actor) {
        if (e.text.includes('lane_depleted')) { lane=true; counts.lane_intents++; }
        if (e.text.startsWith('FEED ')) { feed=true; counts.feed_intents++; }
      }
      if (e.type === 'dragonSplit' && e.parentId === actor && lane) {
        counts.lane_splits++;
        if (examples.length<3) examples.push({round:round+1, parent:e.parentId, child:e.childId});
        lane=false;
      }
      if (e.type === 'dragonDeath' && e.id === actor && feed) {counts.feed_deaths++;feed=false;}
    }
    output.push({id:r.id, map:r.map, side:r.candidate_side, opponent:r.candidate_side==='A'?r.b:r.a, ...counts, examples});
  }
}
console.log(JSON.stringify({totals:output.reduce((t,r)=>{for(const k of ['lane_intents','lane_splits','feed_intents','feed_deaths'])t[k]+=r[k];return t;},{lane_intents:0,lane_splits:0,feed_intents:0,feed_deaths:0}),matches:output},null,2));
