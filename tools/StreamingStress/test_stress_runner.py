import unittest
from run_stress import validate_report

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
