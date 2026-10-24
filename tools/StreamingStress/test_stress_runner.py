import unittest
from run_stress import validate_report, DEMAND_FIELDS

class ReportValidation(unittest.TestCase):
    def report(self):
        return dict(status="complete", stress_completed=True, scene_loading=False,
            stream_frames=600,stress_configured_frames=600,animation_budget_bytes=16*1024*1024,
            animation_rgba_bytes=100,animation_retired_bytes=200,animation_pending_reserved_bytes=300,
            stream_peak_accounted_bytes=600,stream_snapshots=3,stream_replacements=2,
            stream_travel_cells=4,stream_visible_samples=100,stream_ready_visible_samples=25,
            lifetime_fps=500,lifetime_frame_max_ms=40,frame_p99_ms=3)
    def test_partial_coverage_is_measurement_not_false_completion(self):
        self.assertEqual(validate_report(self.report(),16,600,600),.25)
    def test_deferred_counters_are_optional_for_old_clients_and_bounded(self):
        report=self.report()
        report.update(stream_budget_deferred_appearances=2,stream_peak_budget_deferred_appearances=1024,stream_budget_deferred_frames=600)
        self.assertEqual(validate_report(report,16,600,600),.25)
        for key,value in [("stream_budget_deferred_appearances",1025),("stream_peak_budget_deferred_appearances",1),
                          ("stream_budget_deferred_frames",601),("stream_budget_deferred_frames",-1),
                          ("stream_budget_deferred_appearances",1.5)]:
            invalid=dict(report);invalid[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):
                validate_report(invalid,16,600,600)
        del report["stream_budget_deferred_frames"]
        with self.assertRaises(ValueError):validate_report(report,16,600,600)
    def test_demand_latency_outcomes_windows_and_timings(self):
        report=self.report();report.update({k:0 for k in DEMAND_FIELDS})
        report.update(demand_started=4,demand_completed=1,demand_withdrawn=1,demand_failed=1,demand_pending=1,
            demand_samples=1,demand_mean_ms=30,demand_p95_ms=30,demand_p99_ms=30,demand_max_ms=30,
            demand_queued_mean_ms=10,demand_deferred_mean_ms=15,demand_loading_mean_ms=5)
        self.assertEqual(validate_report(report,16,600,600),.25)
        for k,v in [("demand_started",5),("demand_pending",1025),("demand_visible_pending",2),
                    ("demand_samples",257),("demand_visible_completed",2),("demand_mean_ms",float("nan")),
                    ("demand_p99_ms",31),("demand_loading_mean_ms",6),("demand_oldest_visible_ms",1),("demand_peak_visible_ms",1)]:
            invalid=dict(report);invalid[k]=v
            with self.subTest(key=k),self.assertRaises(ValueError):validate_report(invalid,16,600,600)
        del report["demand_samples"]
        with self.assertRaises(ValueError):validate_report(report,16,600,600)
    def test_wrong_replacement_cadence_is_rejected(self):
        with self.assertRaises(ValueError):
            validate_report(self.report(),16,600,300)
    def test_incomplete_or_over_budget_runs_are_rejected(self):
        for key,value in [("stress_completed",False),("scene_loading",True),("stream_frames",599),
                          ("animation_budget_bytes",1024),("stream_peak_accounted_bytes",17*1024*1024),
                          ("stream_ready_visible_samples",101),("stream_replacements",1),("lifetime_fps",float("nan"))]:
            report=self.report();report[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):
                validate_report(report,16,600,600)

if __name__ == "__main__":
    unittest.main()
