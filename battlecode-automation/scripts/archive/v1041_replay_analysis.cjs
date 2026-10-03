#!/usr/bin/env node
// Exact behavior evidence using the installed toolkit's extracted replay decoder.
// Usage: node v1041_replay_analysis.cjs <decoder.cjs> <match.replay>
// PATROL_FOOD is an intention, not a claim that food was reached; both are counted.
'use strict';
const fs = require('fs');
const path = require('path');
if (process.argv.length !== 4) {
  console.error('Usage: node v1041_replay_analysis.cjs <decoder.cjs> <match.replay>');
  process.exit(2);
}
require(path.resolve(process.argv[2]));
const decoded = decodeBattlecode(fs.readFileSync(process.argv[3]));
const map = decoded.match.map;
const key = p => `${p.x},${p.y}`;
const same = (a, b) => a && b && a.x === b.x && a.y === b.y;
const reasons = {S: 'self_collision', O: 'other_body_collision', H: 'head_collision', W: 'kelp_collision', A: 'no_valid_action'};
const stats = Object.fromEntries(['A', 'B'].map(team => [team, {
  peakLength: 0, peakDragon: null, peakRound: null, longestTrajectory: [],
  longDeaths: [], allDeaths: {}, claims: [], patrolFood: [], escapeSplits: [],
  cycleSteps: 0, cycleDeviations: 0, cycleTurnsWithVisiblePearls: 0,
  pearlsCollected: 0, fedPearlsCollected: 0, runtimeFailures: []
}]));
const bodies = decoded.match.roundAt(0).bodies.copy();
const teams = new Map(map.initialDragons.map(b => [b.id, b.team]));
const pearls = new Set([...map.initialTiles].flatMap((tile, i) => tile === 2 ? [`${i % map.width},${Math.floor(i / map.width)}`] : []));
const claims = new Map(), turnCounts = new Map(), moves = new Map(), feedIntent = new Map();
const escapeIntent = new Map(), allEscapes = [], allFood = [], trackedFeedPearls = new Map();
let round = -1, turn = null, dropping = null, checkedRounds = 0;
function summarizeBodies(model) {
  return [...model.dragons.values()].sort((a, b) => a.id - b.id)
    .map(b => [b.id, b.team, b.facing, b.body.map(key)]);
}
function checkBodies() {
  const expected = decoded.match.roundAt(round).bodies;
  if (JSON.stringify(summarizeBodies(bodies)) !== JSON.stringify(summarizeBodies(expected)))
    throw new Error(`Body reconstruction disagrees with toolkit snapshot at round ${round}`);
  checkedRounds++;
}
function updatePeaks() {
  for (const b of bodies.dragons.values()) {
    const s = stats[b.team];
    if (b.body.length > s.peakLength) {
      s.peakLength = b.body.length; s.peakDragon = b.id; s.peakRound = Math.max(0, round);
    }
  }
}
function finishTurn() {
  if (!turn) return;
  const body = bodies.dragons.get(turn.id);
  for (const food of turn.food) {
    food.afterTurnLength = body?.body.length ?? 0;
    food.survivedTurn = !!body;
    food.pearlsSameTurn = turn.eaten;
    food.fedPearlsSameTurn = turn.fed;
    food.actualHeadAfterTurn = body?.body[0] ?? null;
  }
  turn = null;
}
function cycleFor(x, y, w, h) {
  const cells = [];
  for (let yy = 0; yy < h; yy++) {
    if (yy % 2 === 0) for (let xx = yy === 0 ? 0 : 1; xx < w; xx++) cells.push({x:x + xx, y:y + yy});
    else for (let xx = w - 1; xx >= 1; xx--) cells.push({x:x + xx, y:y + yy});
  }
  for (let yy = h - 1; yy >= 1; yy--) cells.push({x, y:y + yy});
  return new Map(cells.map((p, i) => [key(p), cells[(i + 1) % cells.length]]));
}
function spawnStats(x, y, w, h) {
  let spawnTiles = 0, theoreticalRespawnsPerRound = 0;
  for (let yy = y; yy < y + h; yy++) for (let xx = x; xx < x + w; xx++) {
    const r = map.pearlRespawn[yy * map.width + xx];
    if (r && r.maxRounds > 0) {
      spawnTiles++;
      theoreticalRespawnsPerRound += 2 / (r.minRounds + r.maxRounds);
    }
  }
  return {spawnTiles, spawnTileFraction: spawnTiles / (w * h), theoreticalRespawnsPerRound};
}
function visiblePearls(head) {
  let count = 0;
  for (let dy = -3; dy <= 3; dy++) for (let dx = -3; dx <= 3; dx++)
    if (pearls.has(`${(head.x + dx + map.width) % map.width},${(head.y + dy + map.height) % map.height}`)) count++;
  return count;
}
updatePeaks();
for (const event of decoded.events) {
  if (event.type === 'roundStart') {
    finishTurn(); dropping = null; round = event.round; checkBodies();
    if (round % 25 === 0) for (const team of ['A', 'B']) stats[team].longestTrajectory.push([
      round, Math.max(0, ...[...bodies.dragons.values()].filter(b => b.team === team).map(b => b.body.length))
    ]);
    continue;
  }
  // roundAt(0) already incorporates initial dragon updates and map initialization.
  if (round < 0) continue;
  if (event.type === 'turnStart') {
    finishTurn(); dropping = null;
    const b = bodies.dragons.get(event.id);
    const count = (turnCounts.get(event.id) ?? 0) + 1;
    turnCounts.set(event.id, count);
    turn = {id: event.id, team: teams.get(event.id), eaten: 0, fed: 0, food: [], count};
    const cycle = claims.get(event.id);
    if (b?.body.length >= 16 && cycle?.size >= Math.max(80, b.body.length + 20) && cycle.has(key(b.body[0])) && visiblePearls(b.body[0]) > 0)
      stats[b.team].cycleTurnsWithVisiblePearls++;
  }
  const b = bodies.dragons.get(event.id), team = teams.get(event.id), s = stats[team];
  if (event.type === 'dragonLog' && s) {
    const words = event.text.trim().split(/\s+/);
    if (words[0] === 'CLAIM') {
      const [x, y, w, h] = words.slice(1).map(Number);
      if (![x, y, w, h].every(Number.isInteger) || w < 3 || h < 4 || h % 2 || x < 0 || y < 0 || x + w > map.width || y + h > map.height)
        throw new Error(`Invalid CLAIM geometry: ${event.text}`);
      claims.set(event.id, cycleFor(x, y, w, h));
      s.claims.push({round, id:event.id, length:b?.body.length, x, y, w, h, ...spawnStats(x, y, w, h)});
    }
    if (words[0] === 'PATROL_FOOD') {
      const record = {round, id:event.id, length:b?.body.length, reportedLength:Number(words[1]),
        selectedDestination:{x:Number(words[2]), y:Number(words[3])},
        nominalCycleNext:b ? claims.get(event.id)?.get(key(b.body[0])) ?? null : null,
        ownTurn:turn?.count, pearlsNextFourOwnTurns:0, fedPearlsNextFourOwnTurns:0};
      record.differsFromCycle = !!record.nominalCycleNext && !same(record.nominalCycleNext, record.selectedDestination);
      allFood.push(record); s.patrolFood.push(record); turn?.food.push(record);
    }
    if (['GROWER_REVERSE', 'WORKER_REVERSE'].includes(words[0])) {
      const record = {round, id:event.id, kind:words[0], originalLength:b?.body.length,
        requestedChildLength:Number(words[1]), actualSplit:false, parentDeath:null};
      if (b?.body.length) {
        const head = b.body[0], tail = b.body[b.body.length - 1];
        const dx = Math.min(Math.abs(head.x - tail.x), map.width - Math.abs(head.x - tail.x));
        const dy = Math.min(Math.abs(head.y - tail.y), map.height - Math.abs(head.y - tail.y));
        record.tailInsideParentVision = dx <= 3 && dy <= 3;
      }
      allEscapes.push(record); s.escapeSplits.push(record); escapeIntent.set(event.id, record);
    }
    if (words[0] === 'FEED') feedIntent.set(event.id, {round, team, recipient:Number(words[1])});
  }
  if (event.type === 'dragonAction' && s) {
    moves.set(event.id, {round, action:event.action});
    if (!event.action || event.tle || event.instructions?.exceeded) s.runtimeFailures.push({round, id:event.id, event});
  }
  if (event.type === 'dragonSplit') {
    teams.set(event.childId, event.team);
    const record = escapeIntent.get(event.parentId);
    if (record?.round === round) Object.assign(record, {
      actualSplit:true, childId:event.childId, childLength:event.childBody.length,
      childHead:event.childBody[0], childFacing:event.childFacing,
      parentLength:event.parentBody.length, childFirstAction:null, childDeath:null
    });
  }
  if (event.type === 'dragonAction') for (const record of allEscapes)
    if (record.childId === event.id && record.childFirstAction === null)
      record.childFirstAction = {round, action:event.action};
  if (event.type === 'dragonUpdate' && b) {
    const cycle = claims.get(event.id), next = cycle?.get(key(b.body[0]));
    if (b.body.length >= 16 && cycle?.size >= Math.max(80, b.body.length + 20) && next && !same(b.body[0], event.head)) {
      if (same(next, event.head)) s.cycleSteps++; else s.cycleDeviations++;
    }
  }
  if (event.type === 'dragonDeath' && s) {
    s.allDeaths[event.reason] = (s.allDeaths[event.reason] ?? 0) + 1;
    const intent = feedIntent.get(event.id);
    const deliberateFeed = event.reason === 'S' && intent?.round === round;
    if (deliberateFeed) dropping = intent;
    if (b?.body.length >= 16) s.longDeaths.push({round, id:event.id, length:b.body.length,
      reason:event.reason, reasonName:reasons[event.reason], deliberateFeed,
      action:moves.get(event.id), head:b.body[0]});
    for (const record of allEscapes) if (record.childId === event.id)
      record.childDeath = {round, reason:event.reason, length:b?.body.length};
    for (const record of allEscapes) if (record.id === event.id && record.round <= round)
      record.parentDeath = {round, reason:event.reason, length:b?.body.length};
  }
  if (event.type === 'tileChange') {
    const k = key(event.tile);
    if (event.hasPearl) {
      pearls.add(k);
      if (dropping) trackedFeedPearls.set(k, {...dropping});
    } else {
      // In the engine event stream removal during a move is pearl collection.
      if (turn && pearls.has(k)) {
        const fed = trackedFeedPearls.has(k);
        turn.eaten++; if (fed) turn.fed++;
        stats[turn.team].pearlsCollected++; if (fed) stats[turn.team].fedPearlsCollected++;
        for (const record of allFood) if (record.id === turn.id && turn.count >= record.ownTurn && turn.count < record.ownTurn + 4) {
          record.pearlsNextFourOwnTurns++; if (fed) record.fedPearlsNextFourOwnTurns++;
        }
      }
      pearls.delete(k); trackedFeedPearls.delete(k);
    }
  }
  bodies.applyEvent(event);
  updatePeaks();
}
finishTurn();
for (const team of ['A', 'B']) {
  const s = stats[team], alive = [...bodies.dragons.values()].filter(b => b.team === team);
  s.final = {dragonCount:alive.length, longestDragon:Math.max(0,...alive.map(b => b.body.length)), totalLength:alive.reduce((n,b) => n+b.body.length,0)};
  if (JSON.stringify(s.final) !== JSON.stringify(decoded.result[team === 'A' ? 'teamA' : 'teamB']))
    throw new Error(`Final reconstructed standing disagrees with replay result for team ${team}`);
  s.longestTrajectory.push([decoded.match.maxRound,s.final.longestDragon]);
  for (const record of s.escapeSplits) {
    record.childAliveAtFinish = bodies.dragons.has(record.childId);
    record.childSurvivedBirthRound = record.actualSplit && (!record.childDeath || record.childDeath.round > record.round);
    record.childSurvivedFiveRounds = record.actualSplit && decoded.match.maxRound > record.round + 5 && (!record.childDeath || record.childDeath.round > record.round + 5);
  }
  s.summary = {
    patrolFoodDecisions:s.patrolFood.length,
    foodDecisionsOffCycle:s.patrolFood.filter(r => r.differsFromCycle).length,
    foodDecisionsEatingSameTurn:s.patrolFood.filter(r => r.pearlsSameTurn > 0).length,
    foodDecisionsEatingWithinFourOwnTurns:s.patrolFood.filter(r => r.pearlsNextFourOwnTurns > 0).length,
    foodDecisionsDyingSameTurn:s.patrolFood.filter(r => !r.survivedTurn).length,
    foodDecisionsEatingFeedDrops:s.patrolFood.filter(r => r.fedPearlsNextFourOwnTurns > 0).length,
    escapeSplitIntents:s.escapeSplits.length,
    actualEscapeSplits:s.escapeSplits.filter(r => r.actualSplit).length,
    escapeChildrenSurvivedBirthRound:s.escapeSplits.filter(r => r.childSurvivedBirthRound).length,
    escapeChildrenSurvivedFiveRounds:s.escapeSplits.filter(r => r.childSurvivedFiveRounds).length
  };
}
console.log(JSON.stringify({schema:1, replay:process.argv[3], result:decoded.result, seed:String(decoded.seed),
  checkedRoundSnapshots:checkedRounds, map:{width:map.width,height:map.height,...spawnStats(0,0,map.width,map.height)},
  caveats:['Respawn rate is theoretical: a tile already holding a pearl does not accumulate more.',
    'Four-turn collection windows can overlap; their counts do not establish causal attribution.',
    'Cycle-step counts compare the last logged eligible claim geometry; unlogged claim revocations cannot be reconstructed.',
    'A visible nearby pearl can be blocked; visible-pearl counts do not imply a safe detour exists.'],
  teams:stats},null,2));
