"""Browser RSS recorder behavior without opening a browser or collecting user data."""
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from record_browser_memory import BrowserTree, record, summarize


class Missing(Exception):
    pass


class Denied(Exception):
    pass


def process(pid, rss=100, name="chrome", created=10):
    return SimpleNamespace(pid=pid, name=Mock(return_value=name), parent=Mock(return_value=None),
                           create_time=Mock(return_value=created), is_running=Mock(return_value=True),
                           children=Mock(return_value=[]), memory_info=Mock(return_value=SimpleNamespace(rss=rss)))


class BrowserMemoryTests(unittest.TestCase):
    def setup_tree(self):
        self.root = process(1)
        self.child = process(2, 50)
        self.root.children.return_value = [self.child, self.child]
        self.api = SimpleNamespace(Process=Mock(return_value=self.root),
                                   NoSuchProcess=Missing, AccessDenied=Denied)
        return BrowserTree(1, self.api)

    def test_sum_deduplicates_children_and_discovers_each_time(self):
        tree = self.setup_tree()
        sample = tree.sample()
        self.assertTrue(sample["complete"])
        self.assertEqual(sample["rss_sum_bytes"], 150)
        self.assertEqual(sample["root_rss_bytes"], 100)
        self.assertEqual(sample["process_count"], 2)
        self.root.children.return_value = []
        self.assertEqual(tree.sample()["rss_sum_bytes"], 100)

    def test_denied_child_never_publishes_partial_total_as_zero_or_complete(self):
        tree = self.setup_tree()
        self.child.memory_info.side_effect = Denied()
        sample = tree.sample()
        self.assertFalse(sample["complete"])
        self.assertIsNone(sample["rss_sum_bytes"])
        self.assertEqual(sample["unavailable_processes"], 1)
        self.assertEqual(sample["sampled_process_count"], 1)

    def test_child_exit_or_pid_reuse_during_read_is_incomplete(self):
        tree = self.setup_tree()
        self.child.is_running.side_effect = [True, False]
        self.assertIsNone(tree.sample()["rss_sum_bytes"])
        self.child.is_running.side_effect = None
        self.child.memory_info.side_effect = Missing()
        self.assertFalse(tree.sample()["complete"])

    def test_root_identity_change_exit_and_enumeration_denied(self):
        tree = self.setup_tree()
        self.root.create_time.return_value = 11
        self.assertEqual(tree.sample()["root_state"], "identity_changed")
        self.api.Process.side_effect = Missing()
        self.assertEqual(tree.sample()["root_state"], "exited")
        self.api.Process.side_effect = None
        self.root.create_time.return_value = 10
        self.root.children.side_effect = Denied()
        self.assertEqual(tree.sample()["root_state"], "access_denied")

    def test_root_replaced_during_collection_discards_total(self):
        tree = self.setup_tree()
        self.root.create_time.side_effect = [10, 11]
        sample = tree.sample()
        self.assertIsNone(sample["rss_sum_bytes"])
        self.assertEqual(sample["root_state"], "identity_changed")

    def test_only_browser_main_process_is_admitted(self):
        self.setup_tree()
        self.root.name.return_value = "python.exe"
        with self.assertRaises(ValueError):
            BrowserTree(1, self.api)
        self.root.name.return_value = "chrome"
        self.root.parent.return_value = process(3)
        with self.assertRaises(ValueError):
            BrowserTree(1, self.api)

    def test_disjoint_windows_missing_data_and_insufficient_samples(self):
        samples = [dict(complete=True, rss_sum_bytes=x) for x in (999, 100, 110, 120, 130)]
        self.assertEqual(summarize(samples, 1, 2)["rss_sum_growth_bytes"], 20)
        self.assertIsNone(summarize(samples[:4], 1, 2)["rss_sum_growth_bytes"])
        samples[2] = dict(complete=False, rss_sum_bytes=None)
        summary = summarize(samples, 1, 2)
        self.assertIsNone(summary["rss_sum_growth_bytes"])
        self.assertEqual(summary["incomplete_samples"], 1)

    def test_schedule_stops_on_root_exit_and_is_not_a_leak_pass(self):
        tree = self.setup_tree()
        tree.sample = Mock(side_effect=[dict(complete=True, rss_sum_bytes=100, root_state="running"),
                                       dict(complete=False, rss_sum_bytes=None, root_state="exited")])
        sleep = Mock()
        report = record(tree, 5, 1, 0, 1, now=Mock(side_effect=[0, 0, .2, 1]), sleep=sleep)
        self.assertEqual(report["status"], "incomplete")
        self.assertEqual(len(report["samples"]), 2)
        sleep.assert_called_once_with(.8)
        self.assertIsNone(report["rss_sum_growth_bytes"])
        tree.sample = Mock(return_value=dict(complete=True, rss_sum_bytes=100, root_state="running"))
        self.assertEqual(record(tree, 1, 1, 0, 1)["status"], "recorded")

    def test_invalid_bounds_do_not_sample_or_sleep(self):
        tree = self.setup_tree()
        tree.sample = Mock()
        for values in ((0, 1, 0, 1), (601, 1, 0, 1), (2, float("nan"), 0, 1),
                       (2, float("inf"), 0, 1), (2, .01, 0, 1), (2, 1, 2, 1), (2, 1, 0, 0)):
            with self.assertRaises(ValueError):
                record(tree, *values)
        tree.sample.assert_not_called()


if __name__ == "__main__":
    unittest.main()
