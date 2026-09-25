import unittest
from pathlib import Path
from unittest.mock import patch
import autorun_matches as ladder
from research_report import wilson

class ResearchTests(unittest.TestCase):
    def test_statistical_gate_is_not_one_win(self):
        self.assertLess(wilson([1]),.5)
        self.assertLess(wilson([1]*6+[0]*2),.5)
        self.assertGreater(wilson([1]*7+[0]),.5)

    def test_incumbent_cannot_be_replaced_by_elo(self):
        a=ladder.Bot('bot-v8','a','a','a',(8,))
        b=ladder.Bot('bot-v9-exp001','b','b','b',(9,))
        with patch.object(ladder,'load_json',return_value={'champion_uid':'a'}):
            self.assertEqual(ladder.choose_champion([a,b],{'a':1000,'b':2000},{},{}),a)

    def test_experimental_version(self):
        self.assertEqual(ladder.parse_version('bot-v9-exp001'),(9,))

    def test_no_self_pair(self):
        a=ladder.Bot('a','a','a','a',(1,))
        self.assertEqual(ladder.schedule_pair(a,a,[ladder.MapInfo('x','x')],{},set()),[])

if __name__=='__main__':unittest.main()
