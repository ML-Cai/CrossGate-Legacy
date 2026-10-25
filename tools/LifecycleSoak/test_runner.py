import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock
import run_soak


class LifecycleRunnerTest(unittest.TestCase):
    def fake(self, body, cycles=4, **kwargs):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);script=root/'child.py'
            script.write_text('import os,sys,time\n'+body,encoding='utf-8')
            result=run_soak.run([sys.executable,str(script)],root,root/'output',cycles=cycles,**kwargs)
            self.assertEqual(json.loads((root/'output/summary.json').read_text()),result)
            return result

    def handshake(self, ending=''):
        return '''for i in range(int(os.environ['CGL_SOAK_CYCLES'])):
 print('CGL_SOAK_CHECKPOINT '+str(i+1),flush=True)
 assert input()=='continue'
print('[       OK ] '''+run_soak.TEST_NAME+'''',flush=True)
'''+ending

    def test_one_child_completes_every_acknowledged_cycle(self):
        result=self.fake(self.handshake())
        self.assertEqual(result['status'],'complete')
        self.assertEqual(result['cycles_completed'],4)
        self.assertEqual(len(result['rss_checkpoints_bytes']),4)

    def test_zero_exit_with_missing_test_or_missing_cycles_is_failure(self):
        for body in ['pass',"print('[       OK ] '+"+repr(run_soak.TEST_NAME)+")"]:
            self.assertEqual(self.fake(body)['status'],'failed')

    def test_duplicate_or_out_of_order_checkpoint_is_failure(self):
        for value in [0,2,1001]:
            result=self.fake(f"print('CGL_SOAK_CHECKPOINT {value}',flush=True)\ntime.sleep(5)")
            self.assertEqual(result['status'],'failed')
            self.assertIn('checkpoint',result['error'])

    def test_native_failure_and_diagnostic_cannot_be_reported_complete(self):
        for ending in ['sys.exit(1)',"print('VUID-test-negative-control')", "print('ERROR: AddressSanitizer: test')"]:
            self.assertEqual(self.fake(self.handshake(ending))['status'],'failed')

    def test_log_reader_failure_is_not_a_success_or_hang(self):
        original=Path.open
        def open_file(path,*args,**kwargs):
            if path.suffix=='.log':raise OSError('log unavailable')
            return original(path,*args,**kwargs)
        with mock.patch.object(Path,'open',open_file):result=self.fake(self.handshake(),timeout=2)
        self.assertEqual(result['status'],'failed');self.assertIn('log reader failed',result['error'])

    def test_broken_ack_and_pipe_close_still_preserve_failure_summary(self):
        real=subprocess.Popen
        def start(*args,**kwargs):
            child=real(*args,**kwargs);pipe=child.stdin
            child.stdin=mock.Mock(wraps=pipe)
            child.stdin.write.side_effect=BrokenPipeError('ack failed')
            def close():
                pipe.close();raise BrokenPipeError('close failed')
            child.stdin.close.side_effect=close
            return child
        with mock.patch.object(run_soak.subprocess,'Popen',side_effect=start):
            result=self.fake(self.handshake())
        self.assertEqual(result['status'],'failed');self.assertIn('ack failed',result['error'])

    def test_timeout_terminates_and_reaps_only_child(self):
        real=subprocess.Popen;children=[]
        def start(*args,**kwargs):
            child=real(*args,**kwargs);children.append(child);return child
        with mock.patch.object(run_soak.subprocess,'Popen',side_effect=start):
            result=self.fake('time.sleep(10)',timeout=.3)
        self.assertEqual(result['status'],'failed');self.assertIn('timed out',result['error'])
        self.assertEqual(len(children),1);self.assertIsNotNone(children[0].poll())

    def test_missing_optional_memory_is_explicit_and_required_memory_fails(self):
        original=__import__
        def load(name,*args,**kwargs):
            if name=='psutil':raise ImportError('test missing psutil')
            return original(name,*args,**kwargs)
        with mock.patch('builtins.__import__',side_effect=load):
            result=self.fake(self.handshake())
            self.assertEqual(result['status'],'complete')
            self.assertEqual(result['rss_checkpoints_bytes'],[None]*4)
            result=self.fake(self.handshake(),require_memory=True)
            self.assertEqual(result['status'],'failed');self.assertIn('psutil',result['error'])

    def test_growth_uses_disjoint_quiescent_windows_after_warmup(self):
        summary=run_soak.memory_summary([9999,9999,100,110,120,200,210,220],2)
        self.assertEqual(summary['rss_first_window_bytes'],110)
        self.assertEqual(summary['rss_last_window_bytes'],210)
        self.assertEqual(summary['rss_growth_bytes'],100)
        self.assertIsNone(run_soak.memory_summary([100]*3,0)['rss_growth_bytes'])
        self.assertIsNone(run_soak.memory_summary([100,None,100,100],0)['rss_growth_bytes'])

    def test_growth_gate_requires_samples_and_rejects_excess(self):
        for kwargs in [dict(cycles=0),dict(cycles=1001),dict(timeout=float('nan')),
                       dict(max_growth_mib=-1),dict(max_growth_mib=1)]:
            with self.assertRaises(ValueError):self.fake('pass',**kwargs)
        fake_psutil=SimpleNamespace(Process=lambda _:SimpleNamespace(memory_info=lambda:SimpleNamespace(rss=100)), NoSuchProcess=ProcessLookupError)
        with mock.patch.dict(sys.modules,psutil=fake_psutil), mock.patch.object(run_soak,'memory_summary',return_value={'rss_growth_bytes':2*1024*1024}):
            result=self.fake(self.handshake(),warmup=0,max_growth_mib=1)
        self.assertEqual(result['status'],'failed');self.assertIn('growth',result['error'])


if __name__=='__main__':unittest.main()
