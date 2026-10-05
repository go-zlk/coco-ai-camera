import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location('verify', pathlib.Path(__file__).parents[1] / 'scripts/verify_runtime.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


def sample(sequence=1, health='online', mode='live'):
    return dict(mode=mode, health=health, pet_state='offline' if health == 'offline' else 'unknown',
                sequence=sequence, dropped_frames=0, cat_count=0, person_count=0,
                freshness_seconds=1, inference_ms=10, pet_confidence=0)


class AcceptanceTests(unittest.TestCase):
    def test_repeated_snapshot_is_not_more_inference(self):
        monitor = verify.Monitor()
        monitor.observe(sample(), 2)
        monitor.observe(sample(), 3)
        self.assertEqual(len(monitor.inferences), 1)

    def test_offline_snapshot_is_not_an_inference_sample(self):
        monitor = verify.Monitor()
        monitor.observe(sample(health='offline'), 2)
        self.assertEqual(len(monitor.inferences), 0)

    def test_recovery_requires_online_offline_online(self):
        monitor = verify.Monitor()
        monitor.timeline_checks = 1
        monitor.observe(sample(health='offline'), 1)
        monitor.observe(sample(), 1)
        self.assertFalse(monitor.report(require_recovery=True)['contract_checks_passed'])
        monitor.observe(sample(health='offline'), 1)
        monitor.observe(sample(2), 1)
        self.assertTrue(monitor.report(require_recovery=True)['contract_checks_passed'])

    def test_simulation_cannot_pass_live_gate(self):
        monitor = verify.Monitor()
        monitor.observe(sample(mode='replay'), 1)
        monitor.timeline_checks = 1
        self.assertFalse(monitor.report(require_live=True)['contract_checks_passed'])

    def test_offline_only_run_cannot_pass_real_gate(self):
        monitor = verify.Monitor()
        monitor.observe(sample(health='offline'), 1)
        monitor.timeline_checks = 1
        self.assertFalse(monitor.report(require_live=True)['contract_checks_passed'])

    def test_multi_cat_cannot_be_assigned_single_state(self):
        context = sample()
        context.update(cat_count=2, pet_state='resting')
        with self.assertRaises(ValueError):
            verify.validate_context(context)

    def test_timeline_conservation_and_overlap(self):
        timeline = dict(day='2026-01-01', intervals=[dict(state='active', start='2026-01-01T00:00:00Z',
                        end='2026-01-01T00:00:10Z', duration_seconds=10)],
                        summary_seconds=dict(active=10, resting=0, unknown=0, offline=0, out_of_view=0))
        verify.validate_timeline(timeline)
        timeline['summary_seconds']['active'] = 11
        with self.assertRaises(ValueError):
            verify.validate_timeline(timeline)
        timeline['summary_seconds']['active'] = 10
        timeline['intervals'][0]['start'] = '2026-01-01T00:00:01Z'
        with self.assertRaises(ValueError):
            verify.validate_timeline(timeline)


if __name__ == '__main__':
    unittest.main()
