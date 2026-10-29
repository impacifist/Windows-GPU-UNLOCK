"""Synthetic tests; no external checkout, GPU, or firmware required."""

import copy
import hashlib
import unittest

from capture_observations import assess


def fixture():
    return {'schema': 4, 'gpu_writes': False, 'unlock_attempted': False,
        'hardware_support_validated': False, 'unique_expected_identity_observed': True,
        'register_snapshot': {'complete': True}, 'physical_prom': {'complete': True},
        'platform': {'available': True, 'complete': True, 'atomic_snapshot': False,
            'release_authorized': False, 'target': 0,
            'handles': [{'id': 0, 'gpu_descendant': True,
                         'active_opens': [{'agent': 1, 'controller': 0, 'attributes': '0x00000010'}]},
                        {'id': 1, 'gpu_descendant': False, 'active_opens': []}],
            'gpu_agents': [{'agent': 1, 'code': {'length': 3, 'hex': '010203',
                'loaded_sha256': hashlib.sha256(bytes([1, 2, 3])).hexdigest()}}]}}


class ObservationTests(unittest.TestCase):
    def test_complete_observation_never_authorizes_hardware(self):
        data = fixture()
        before = copy.deepcopy(data)
        result = assess(data)
        self.assertTrue(result['target_handle_resolved'])
        self.assertTrue(result['all_recorded_edge_indices_resolved'])
        self.assertTrue(result['recorded_gpu_agent_code_coverage'])
        for flag in ('unlock_ready', 'release_authorized', 'hardware_support_validated',
                     'atomic_snapshot', 'capture_origin_authenticated'):
            self.assertFalse(result[flag])
        self.assertEqual(data, before)

    def test_absent_target_does_not_hide_present_code(self):
        data = fixture(); data['platform']['target'] = None
        result = assess(data)
        self.assertFalse(result['target_handle_resolved'])
        self.assertTrue(result['recorded_gpu_agent_code_coverage'])

    def test_missing_code_is_separate_from_enumeration(self):
        data = fixture(); data['platform']['gpu_agents'][0]['code'] = None
        result = assess(data)
        self.assertTrue(result['platform_enumeration_reported_complete'])
        self.assertFalse(result['recorded_gpu_agent_code_coverage'])

    def test_unknown_edge_agent_is_unresolved(self):
        data = fixture(); data['platform']['handles'][0]['active_opens'][0]['agent'] = None
        result = assess(data)
        self.assertFalse(result['all_recorded_edge_indices_resolved'])
        self.assertFalse(result['recorded_gpu_agent_code_coverage'])

    def test_absent_platform_is_unknown(self):
        data = fixture(); data['platform'] = {'available': False}
        result = assess(data)
        self.assertIsNone(result['target_handle_resolved'])
        self.assertIsNone(result['recorded_gpu_agent_code_coverage'])

    def test_absent_driver_record_is_incomplete(self):
        data = fixture(); data['platform']['gpu_agents'] = []
        self.assertFalse(assess(data)['recorded_gpu_agent_code_coverage'])

    def test_code_budget_omission_is_incomplete(self):
        data = fixture(); data['platform']['gpu_agents'][0]['code']['hex'] = None
        self.assertFalse(assess(data)['recorded_gpu_agent_code_coverage'])

    def test_rejects_wrong_code_hash(self):
        data = fixture(); data['platform']['gpu_agents'][0]['code']['loaded_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'Code hash'):
            assess(data)

    def test_rejects_duplicate_driver_record(self):
        data = fixture(); data['platform']['gpu_agents'] *= 2
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            assess(data)

    def test_boolean_is_not_a_handle_index(self):
        data = fixture(); data['platform']['target'] = True
        self.assertFalse(assess(data)['target_handle_resolved'])

    def test_unobserved_graph_is_not_vacuously_complete(self):
        data = fixture()
        for handle in data['platform']['handles']: handle['active_opens'] = []
        data['platform']['gpu_agents'] = []
        result = assess(data)
        self.assertIsNone(result['all_recorded_edge_indices_resolved'])
        self.assertIsNone(result['recorded_gpu_agent_code_coverage'])

    def test_rejects_claimed_release(self):
        data = fixture(); data['platform']['release_authorized'] = True
        with self.assertRaisesRegex(ValueError, 'authorize release'):
            assess(data)

    def test_optional_null_controller_is_only_literal_missing_slot(self):
        data = fixture()
        edge = data['platform']['handles'][0]['active_opens'][0]
        edge['attributes'] = '0x00000020'  # application-exclusive open
        edge['controller'] = None
        result = assess(data)
        self.assertFalse(result['all_recorded_edge_indices_resolved'])
        self.assertEqual(result['null_or_unresolved_reference_slots'], 1)
        self.assertFalse(result['open_roles_interpreted'])
        self.assertFalse(result['release_authorized'])

    def test_application_agent_without_code_is_not_a_role_failure(self):
        data = fixture()
        data['platform']['handles'][0]['active_opens'][0]['attributes'] = '0x00000020'
        data['platform']['gpu_agents'][0]['code'] = None
        result = assess(data)
        self.assertFalse(result['recorded_gpu_agent_code_coverage'])
        self.assertFalse(result['open_roles_interpreted'])
        self.assertNotIn('ownership_complete', result)

    def test_unknown_open_role_remains_uninterpreted(self):
        data = fixture()
        data['platform']['handles'][0]['active_opens'][0]['attributes'] = '0x80000000'
        self.assertFalse(assess(data)['open_roles_interpreted'])


if __name__ == '__main__':
    unittest.main()
