import tempfile
import unittest
from pathlib import Path
from round_robin import schedule, standings, reports, decision

class RoundRobinTests(unittest.TestCase):
    def row(self, winner='A', status='ok'):
        return dict(id='00001', a='alpha', b='beta', map='arena.map', seed=1,
                    winner=winner,status=status,rounds=500,seconds=1.2,
                    a_queen=10,b_queen=0,a_longest=10,b_longest=20,
                    a_length=10,b_length=30,end_reason=1,decided_by='queen')
    def test_unordered_schedule(self):
        jobs=schedule(['a','b','c'],['x','y'],[1,2])
        self.assertEqual(len(jobs),12)
        self.assertEqual(len({(frozenset((j['a'],j['b'])),j['map'],j['seed']) for j in jobs}),12)
        self.assertTrue(all(j['a']<j['b'] for j in jobs))
    def test_draw_and_errors_denominator(self):
        a,b=standings([self.row(),self.row(None),self.row('B','bot_error')],['alpha','beta'])
        self.assertEqual((a['wins'],a['draws'],a['losses'],a['games'],a['errors']),(1,1,0,2,1))
        self.assertEqual(a['win_rate'],.5);self.assertEqual(a['score_rate'],.75)
        self.assertEqual(b['losses'],1)
        self.assertIsNone(standings([],['alpha'])[0]['win_rate'])
    def test_tiebreak_order(self):
        r=self.row();self.assertEqual(decision(r),'queen')
        r['b_queen']=10;self.assertEqual(decision(r),'longest dragon')
        r['b_longest']=10;self.assertEqual(decision(r),'total length')
        r['b_length']=10;r['winner']=None;self.assertEqual(decision(r),'draw')
        r['end_reason']=0;self.assertEqual(decision(r),'mutual elimination')
    def test_reports(self):
        manifest=dict(engine='1.2.9',bots={'alpha':'hash','beta':'hash'},maps={'arena.map':'hash'},seeds=[1],schedule=[{}])
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)
            self.assertEqual(reports(out,[],manifest)['successful'],0)
            s=reports(out,[self.row()],manifest)
            self.assertEqual(s['by_map']['arena.map'][0]['wins'],1)
            self.assertEqual(s['matchups']['alpha vs beta'][0]['wins'],1)
            page=(out/'report.html').read_text()
            self.assertIn('width:100.000%',page);self.assertIn('width:0.000%',page)
            self.assertIn('replays/00001.replay',page)
            for name in ['summary.json','matches.jsonl','standings.csv','matches.csv']:
                self.assertTrue((out/name).is_file())

if __name__=='__main__':unittest.main()
