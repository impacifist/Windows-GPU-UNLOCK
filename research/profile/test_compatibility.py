"""Offline compatibility behavior on synthetic data; never hardware evidence."""
import copy
import contextlib
import hashlib
import importlib.util
import json
import io
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, HERE / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


checker = load('compatibility_test_subject', 'check-compatibility.py')
topology = load('compatibility_synthetic_topology', 'test_local_topology.py')


def driver_image(size, rva, length):
    data = bytearray(size)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', data, 0x84, 0x8664, 1)
    struct.pack_into('<H', data, 0x94, 0xf0)
    struct.pack_into('<H', data, 0x98, 0x20b)
    struct.pack_into('<II', data, 0x98 + 56, size, 0x200)
    struct.pack_into('<I', data, 0x98 + 108, 16)
    struct.pack_into('<8sII', data, 0x188, b'.text', length, rva)
    struct.pack_into('<I', data, 0x188 + 36, 0x60000020)
    return data


class CompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.envelope = copy.deepcopy(topology.ENVELOPE)
        self.envelope.update(min_captures=2, nvgop=[{
            'version': '60009', 'text_sha256': hashlib.sha256(bytes(0x500)).hexdigest()}])
        self.envelope['chips'][0]['nvgop'] = copy.deepcopy(self.envelope['nvgop'])

    def populate(self, *, no_edid=True, count=2, **options):
        text = topology.synthetic_capture(no_edid=no_edid, **options)
        first_binding = text.index('binding handle=')
        binding_count = (3 if no_edid else 4) + int(options.get('aggregate', False))
        text = text[:first_binding] + f'driver-bindings={binding_count:08x}\n' + text[first_binding:]
        for agent, index in ((3, 0), (9, 1)):
            if agent == 9 and no_edid:
                continue
            pattern = rf'(^binding handle=0x{agent:x} .*?)(?=^binding handle=|^handles status=)'
            text, hits = re.subn(pattern, lambda m: m[1] + f'img dump index={index:02x}\n', text, flags=re.M | re.S)
            self.assertEqual(hits, 1)
        for index in range(count):
            (self.folder / f'DIAG{index:02d}.TXT').write_text(text, encoding='ascii')
        nv_size=0x26ae0 if options.get('board') in (0x220310de,0x248710de,0x250410de) else 0x2be40
        if options.get('board')==0x24c910de:nv_size=0x26e00
        (self.folder / 'IMG00.BIN').write_bytes(driver_image(nv_size, 0x280, 0x500))
        if not no_edid:
            (self.folder / 'IMG01.BIN').write_bytes(driver_image(0x9000, 0x1000, 0x800))
        (self.folder / 'PROM.BIN').write_bytes(bytes(1048576))
        return text

    def result(self):
        return checker.check(self.folder, self.envelope)

    def assert_reason(self, status, code):
        result = self.result()
        self.assertEqual(result['status'], status, result)
        self.assertIn(code, [entry['code'] for entry in result['checks']])
        self.assertIs(result['write_eligible'], False)
        self.assertIs(result['hardware_tested'], False)

    def test_complete_candidate_matrix_is_not_hardware_approval(self):
        for bus in (1, 0x2b):
            for igpu in (False, True):
                for no_edid in (False, True):
                    for children in (1, 2):
                        with self.subTest(bus=bus, igpu=igpu, no_edid=no_edid, children=children):
                            self.populate(bus=bus, igpu=igpu, no_edid=no_edid, children=children)
                            self.assert_reason('candidate', 'images_pass')

    def test_missing_and_single_capture(self):
        self.assert_reason('needs_data', 'no_captures')
        self.populate(count=1)
        self.assert_reason('needs_data', 'capture_count')

    def test_secondary_display_path_must_agree_across_cold_boots(self):
        for no_edid in (False, True):
            with self.subTest(no_edid=no_edid):
                first = self.populate(no_edid=no_edid, children=2, child_adr=0x10)
                self.populate(no_edid=no_edid, children=2, child_adr=0xa0)
                (self.folder / 'DIAG00.TXT').write_text(first, encoding='ascii')
                self.assert_reason('invalid_data', 'captures_disagree')
                with self.assertRaisesRegex(checker.mp.Refused, 'disagree on the display child paths'), \
                     patch.object(checker.mp, 'locate_fwsec', side_effect=AssertionError('must reject before PROM parsing')):
                    checker.mp.generate(self.folder, Path('not-invoked'), self.envelope)

    def test_generated_header_and_report_pin_nonaggregate_child_count(self):
        for no_edid in (False, True):
            for children in (1, 2):
                with self.subTest(no_edid=no_edid, children=children):
                    self.populate(no_edid=no_edid, children=children)
                    # This test covers topology metadata, not the native PROM parser.
                    with patch.object(checker.mp, 'locate_fwsec', return_value={'image_off': 0x100, 'image_size': 0x200}):
                        header, report, _ = checker.mp.generate(self.folder, Path('not-invoked'), self.envelope)
                    self.assertIn('#define GU_LOCAL_CONSOLE_AGGREGATE 0\n', header)
                    self.assertIn(f'#define GU_LOCAL_CHILD_COUNT {children}\n', header)
                    self.assertIs(report['Replay']['Aggregate'], False)
                    self.assertEqual(report['Replay']['ChildCount'], children)

    def test_complete_root_and_bridge_ancestry_must_agree_across_cold_boots(self):
        changes=(('02010c00d041030a00000000','02010c00d041030a01000000'),
                 ('02010c00d041030a00000000','02010c00d041080a00000000'),
                 ('010106000103','010106000003'))
        for old,new in changes:
            with self.subTest(new=new):
                text=self.populate(no_edid=False)
                changed=text.replace('path-node raw='+old,'path-node raw='+new)
                self.assertNotEqual(changed,text)
                (self.folder/'DIAG00.TXT').write_text(changed,encoding='ascii')
                self.assert_reason('invalid_data','captures_disagree')
                with self.assertRaisesRegex(checker.mp.Refused,'disagree'), \
                     patch.object(checker.mp,'locate_fwsec',side_effect=AssertionError('ancestry drift must stop before PROM parsing')):
                    checker.mp.generate(self.folder,Path('not-invoked'),self.envelope)

    def test_observed_segment_is_pinned_in_generated_header_and_report(self):
        for segment in (0,1,0xffff):
            text=self.populate()
            text=text.replace('probe-target seg=0x0000 ',f'probe-target seg=0x{segment:04x} ')
            text=text.replace('pci location-status=0x0 segment=0x0 ',f'pci location-status=0x0 segment=0x{segment:x} ')
            for index in (0,1):(self.folder/f'DIAG{index:02d}.TXT').write_text(text,encoding='ascii')
            self.assert_reason('candidate','images_pass')
            with patch.object(checker.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}):
                header,report,_=checker.mp.generate(self.folder,Path('not-invoked'),self.envelope)
            self.assertIn(f'#define GU_TARGET_SEGMENT 0x{segment:x}U\n',header)
            self.assertEqual(report['Replay']['Segment'],segment)
            self.assertIs(type(report['Replay']['Segment']),int)
            # Both observations move together, but only in one cold-boot capture.
            changed=text.replace(f'probe-target seg=0x{segment:04x} ',f'probe-target seg=0x{segment^1:04x} ')
            changed=changed.replace(f'pci location-status=0x0 segment=0x{segment:x} ',f'pci location-status=0x0 segment=0x{segment^1:x} ')
            (self.folder/'DIAG00.TXT').write_text(changed,encoding='ascii')
            self.assert_reason('invalid_data','captures_disagree')
            with self.assertRaisesRegex(checker.mp.Refused,'disagree on GU_TARGET_SEGMENT'), \
                 patch.object(checker.mp,'locate_fwsec',side_effect=AssertionError('segment drift must reject before PROM parsing')):
                checker.mp.generate(self.folder,Path('not-invoked'),self.envelope)

    def test_segment_overflow_and_graph_disagreement_are_refused(self):
        for segment in (0x10000,0x100000001):
            text=self.populate().replace('probe-target seg=0x0000 ',f'probe-target seg=0x{segment:x} ')
            (self.folder/'DIAG01.TXT').write_text(text,encoding='ascii')
            with self.assertRaisesRegex(checker.mp.Refused,'16-bit PCI segment'):
                checker.mp.analyze(self.folder/'DIAG01.TXT',None,self.envelope)
        text=self.populate().replace('probe-target seg=0x0000 ','probe-target seg=0x0001 ')
        (self.folder/'DIAG01.TXT').write_text(text,encoding='ascii')
        with self.assertRaisesRegex(checker.mp.Refused,'PCI location'):
            checker.mp.analyze(self.folder/'DIAG01.TXT',None,self.envelope)

    def test_mandatory_generator_refusals_work_on_nonzero_segments(self):
        refusals=load('segment_generator_refusals','test_make_profile.py')
        for no_edid in (False,True):
            for segment in (0,0x1000,0xffff):
                with self.subTest(no_edid=no_edid,segment=segment):
                    text=self.populate(no_edid=no_edid)
                    text=text.replace('probe-target seg=0x0000 ',f'probe-target seg=0x{segment:04x} ')
                    text=text.replace('pci location-status=0x0 segment=0x0 ',f'pci location-status=0x0 segment=0x{segment:x} ')
                    for index in (0,1):(self.folder/f'DIAG{index:02d}.TXT').write_bytes(text.encode())
                    output=io.StringIO()
                    with patch.object(refusals,'ENVELOPE',self.envelope),patch.object(refusals,'failures',0), \
                         patch.object(refusals.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}), \
                         patch.object(sys,'argv',['test_make_profile.py',str(self.folder),'not-invoked']),contextlib.redirect_stdout(output):
                        result=refusals.main()
                    self.assertEqual(result,0,output.getvalue())

    def test_missing_assets_are_not_candidate(self):
        self.populate()
        (self.folder / 'IMG00.BIN').unlink()
        (self.folder / 'PROM.BIN').unlink()
        self.assert_reason('needs_data', 'missing_images')
        self.assert_reason('needs_data', 'missing_prom')

    def test_dump_reference_required(self):
        text = self.populate().replace('img dump index=00\n', '')
        (self.folder / 'DIAG01.TXT').write_text(text, encoding='ascii')
        self.assert_reason('needs_data', 'missing_images')

    def test_unknown_gpu_does_not_mean_globally_unsupported(self):
        for board in ('248210de', '250410de', 'ffffffff'):
            text = self.populate().replace('220410de', board)
            for index in (0, 1):
                (self.folder / f'DIAG{index:02d}.TXT').write_text(text, encoding='ascii')
            self.assert_reason('outside_scope', 'gpu_scope')

    def test_damaged_graph_is_not_missing_gpu_or_edid(self):
        good = self.populate()
        for text in (good.replace('capture-complete=1', 'capture-complete=0'),
                     good + 'diagnostic-result status=0x0000000000000000\n',
                     good.replace('pci config-status=0x0000000000000000\n', '', 1),
                     good.replace('open agent=0x3 controller=0x2 attributes=0x8 count=0x1', 'open broken')):
            with self.subTest(text=text[-40:]):
                (self.folder / 'DIAG01.TXT').write_text(text, encoding='ascii')
                self.assert_reason('invalid_data', 'invalid_graph')

    def test_attributes_have_actionable_reasons(self):
        good = self.populate()
        record = 'pci-attributes status=0x0000000000000000 value=0x0000000000000300\n'
        for change, status, code in (('', 'needs_data', 'attributes_missing'),
                                    (record.replace('0300', '0700'), 'invalid_data', 'attributes_mismatch')):
            (self.folder / 'DIAG01.TXT').write_text(good.replace(record, change), encoding='ascii')
            self.assert_reason(status, code)

    def test_cold_state_and_missing_registers(self):
        good = self.populate()
        for change, status, code in ((good.replace('value=0xffffff8f', 'value=0x0'), 'invalid_data', 'cold_state'),
                                    (re.sub(r'^mmio off=0x00823804 .*\n', '', good, flags=re.M), 'needs_data', 'deep_capture_missing')):
            (self.folder / 'DIAG01.TXT').write_text(change, encoding='ascii')
            self.assert_reason(status, code)

    def test_individually_valid_captures_must_agree(self):
        self.populate(bus=1)
        (self.folder / 'DIAG01.TXT').write_text(topology.synthetic_capture(bus=0x2b), encoding='ascii')
        self.assert_reason('invalid_data', 'captures_disagree')

    def test_unknown_nvgop_code_is_not_candidate(self):
        self.populate()
        image = bytearray((self.folder / 'IMG00.BIN').read_bytes())
        image[0x280] = 1
        (self.folder / 'IMG00.BIN').write_bytes(image)
        self.assert_reason('outside_scope', 'driver_scope')

    def test_optional_parent_records_cannot_change_profile_assessment(self):
        supplemental=('parent-baseline status=0x0000000000000000 available=1 parents=0x01\n'
            'parent-baseline-device index=0x00 segment=0xffff bus=0xff device=0x00 function=0x00 attributes=0x0000000000000200 supported=0x0000000000000700\n'
            'parent-baseline-config index=0x00 words='+' '.join(['ffffffff']*64)+'\n')
        for mismatch in (False,True):
            self.populate()
            if mismatch:
                for path in self.folder.glob('DIAG*.TXT'):
                    path.write_text(path.read_text().replace('value=0x0000000000000300','value=0x0000000000000700'))
            before=self.result()
            for path in self.folder.glob('DIAG*.TXT'):path.write_text(path.read_text()+supplemental)
            self.assertEqual(self.result(),before)
            self.assert_reason('invalid_data' if mismatch else 'candidate','attributes_mismatch' if mismatch else 'images_pass')

    def test_per_card_driver_pins_do_not_inherit_another_skus_firmware(self):
        self.populate()
        chip=self.envelope['chips'][0]
        captured=copy.deepcopy(chip['nvgop'][0])
        other={'version':'60010','text_sha256':'1'*64}
        chip['nvgop'].append(other)
        self.envelope['nvgop'].append(copy.deepcopy(other))
        chip['pci_nvgop']={'220410de':[captured]}
        self.assert_reason('candidate','images_pass')
        chip['pci_nvgop']['220410de']=[other]
        self.assert_reason('outside_scope','driver_scope')
        with self.assertRaisesRegex(checker.mp.Refused,'has not released cleanly'):
            checker.mp.analyze(self.folder/'DIAG01.TXT',self.folder,self.envelope)

    def test_malformed_per_card_driver_pins_are_installation_errors(self):
        self.populate()
        captured=copy.deepcopy(self.envelope['chips'][0]['nvgop'][0])
        for mapping in (None,[],{}, {'220410de':[]}, {'220410de':[captured,captured]},
                        {'220410de':[{'version':'60010','text_sha256':'1'*64}]},
                        {'220410de':[captured],'ffffffff':[captured]}, {'220410de':[{}]},
                        {'220410de':['bad']}, {'220410de':[{'version':[],'text_sha256':'1'*64}]}):
            with self.subTest(mapping=mapping):
                self.envelope['chips'][0]['pci_nvgop']=mapping
                self.assertEqual(self.result()['status'],'error')
                with self.assertRaises(checker.mp.Refused):
                    checker.mp.analyze(self.folder/'DIAG01.TXT',self.folder,self.envelope)

    def test_ga104_card_memory_and_firmware_pins_stay_coupled(self):
        self.envelope=json.loads((HERE/'proven.json').read_text())
        for driver in self.envelope['nvgop']+[driver for chip in self.envelope['chips'] for driver in chip['nvgop']]+[
                driver for chip in self.envelope['chips'] for pins in chip.get('pci_nvgop',{}).values() for driver in pins]:
            driver['text_sha256']=hashlib.sha256(bytes(0x500)).hexdigest()
        for board,memory,version in ((0x248410de,0x2000,'60009'),(0x248710de,0x3000,'60010'),(0x24c910de,0x2000,'60015')):
            with self.subTest(board=board):
                self.populate(board=board,memory=memory,chip=0xb74000a1)
                self.assert_reason('candidate','images_pass')
                with patch.object(checker.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}):
                    header,report,_=checker.mp.generate(self.folder,Path('not-invoked'),self.envelope)
                self.assertIn(f'#define GU_BOARD_ID 0x{board:08x}U',header)
                self.assertIn(f'#define GU_NVGOP_VERSION 0x{version}U',header)
                image_path=self.folder/'IMG00.BIN'
                original=image_path.read_bytes()
                changed=bytearray(original);changed[0x280]^=1
                image_path.write_bytes(changed)
                self.assert_reason('outside_scope','driver_scope')
                with self.assertRaises(checker.mp.Refused):
                    checker.mp.generate(self.folder,Path('not-invoked'),self.envelope)
                image_path.write_bytes(original)
                # Every other SKU's driver is chip/global-reviewed, but forbidden here.
                chip=next(chip for chip in self.envelope['chips'] if chip['chip']=='b74000a1')
                saved=copy.deepcopy(chip['pci_nvgop'])
                for other,pins in saved.items():
                    if other==f'{board:08x}':continue
                    chip['pci_nvgop'][f'{board:08x}']=copy.deepcopy(pins)
                    self.assert_reason('outside_scope','driver_scope')
                chip['pci_nvgop']=saved
                self.populate(board=board,memory=0x3000 if memory==0x2000 else 0x2000,chip=0xb74000a1)
                self.assert_reason('outside_scope','gpu_scope')
                self.populate(board=board,memory=memory,chip=0xb74000a2)
                self.assert_reason('outside_scope','gpu_scope')
        chip.pop('pci_nvgop')
        self.assertEqual(self.result()['status'],'error')
        with self.assertRaisesRegex(checker.mp.Refused,'requires PCI-specific'):
            checker.mp.card_nvgop(chip,0x248410de)

    def test_malformed_image_and_prom_mismatch(self):
        self.populate()
        (self.folder / 'IMG00.BIN').write_bytes(b'not executable and must never be executed')
        self.assert_reason('invalid_data', 'images_invalid')
        self.populate()
        (self.folder / 'PROM.BIN').write_bytes(bytes(100))
        self.assert_reason('invalid_data', 'prom_invalid')

    def test_bounded_input_and_unreadable_folder(self):
        self.populate()
        with patch.object(checker, 'DIAG_LIMIT', 32):
            self.assert_reason('invalid_data', 'input_limit')
        with patch.object(checker, 'TOTAL_LIMIT', 32):
            self.assert_reason('invalid_data', 'input_limit')
        self.assertEqual(checker.check(self.folder / 'absent', self.envelope)['status'], 'invalid_data')
        for index in range(33):
            (self.folder / f'DIAG{index:02d}.TXT').write_bytes(b'')
        self.assert_reason('invalid_data', 'input_limit')

    def test_no_writes_or_subprocess_even_for_complete_candidate(self):
        self.populate()
        def manifest():
            return {path.name: path.read_bytes() for path in self.folder.iterdir()}
        before = manifest()
        with patch.object(Path, 'write_bytes', side_effect=AssertionError('write forbidden')), \
             patch.object(Path, 'write_text', side_effect=AssertionError('write forbidden')), \
             patch.object(checker.mp, 'generate', side_effect=AssertionError('generate forbidden')), \
             patch.object(checker.mp, 'locate_fwsec', side_effect=AssertionError('subprocess forbidden')), \
             patch.object(subprocess, 'run', side_effect=AssertionError('subprocess forbidden')):
            self.assert_reason('candidate', 'prom_pass')
        self.assertEqual(manifest(), before)

    def test_installed_envelope_errors_are_not_capture_failures(self):
        self.populate()
        for field, value in (('min_captures', 0), ('min_captures', True), ('min_captures', 33),
                             ('probe_device_ids', []), ('chips', [{}]), ('nvgop', [{}])):
            with self.subTest(field=field, value=value):
                envelope = copy.deepcopy(self.envelope)
                envelope[field] = value
                result = checker.check(self.folder, envelope)
                self.assertEqual(result['status'], 'error')
                self.assertEqual(result['checks'][0]['code'], 'tool_error')
        self.assertEqual(checker.check(self.folder, {})['status'], 'error')

    def test_recognized_aggregate_is_candidate_without_write_or_hardware_approval(self):
        for children in (1,2):
            self.populate(aggregate=True,no_edid=False,children=children)
            self.assert_reason('candidate','images_pass')
            with patch.object(checker.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}):
                header,report,_=checker.mp.generate(self.folder,Path('not-invoked'),self.envelope)
            self.assertIn('#define GU_LOCAL_CONSOLE_AGGREGATE 1\n',header)
            self.assertIs(report['Replay']['Aggregate'],True)
            self.assertEqual(report['Replay']['ChildCount'],children)

    def test_cli_candidate_and_installation_error_exit_codes(self):
        self.populate()
        installed = checker.HERE / 'proven.json'
        original = Path.read_text
        for content, code, status in ((json.dumps(self.envelope), 0, 'candidate'), ('{}', 3, 'error')):
            def read(path, *args, **kwargs):
                return content if path == installed else original(path, *args, **kwargs)
            out = io.StringIO()
            with patch.object(Path, 'read_text', read), contextlib.redirect_stdout(out):
                self.assertEqual(checker.main([str(self.folder), '--json']), code)
            report = json.loads(out.getvalue())
            self.assertEqual(report['status'], status)
            self.assertIs(report['write_eligible'], False)

    def test_cli_json_languages_exit_codes_and_privacy(self):
        self.populate()
        private = 'PRIVATE-SERIAL-DO-NOT-REPORT'
        for path in self.folder.glob('DIAG*.TXT'):
            path.write_text(path.read_text() + private + '\n', encoding='ascii')
        # The real envelope intentionally does not know our synthetic code hash.
        for lang in ('en', 'ko'):
            done = subprocess.run([sys.executable, '-I', str(HERE / 'check-compatibility.py'),
                                   str(self.folder), '--json', '--lang', lang], capture_output=True, text=True)
            self.assertEqual(done.returncode, 2, done.stderr)
            report = json.loads(done.stdout)
            self.assertEqual(report['status'], 'outside_scope')
            self.assertIs(report['write_eligible'], False)
            self.assertNotIn(str(self.folder), done.stdout)
            self.assertNotIn(private, done.stdout)
            self.assertNotIn(topology.PROM_HASH, done.stdout)
            self.assertEqual(report['summary'], checker.MESSAGES['outside_scope'][lang == 'ko'])
            self.assertEqual(done.stderr, '')

    def test_direct_cli_leaves_source_and_input_files_unchanged(self):
        self.populate()
        def manifest():
            paths = list(HERE.rglob('*')) + list(self.folder.rglob('*'))
            return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in paths if path.is_file()}
        before = manifest()
        # No -B/-I: the entry point itself must suppress local bytecode writes.
        done = subprocess.run([sys.executable, str(HERE / 'check-compatibility.py'),
                               str(self.folder)], cwd=self.folder, capture_output=True, text=True)
        self.assertEqual(done.returncode, 2, done.stderr)
        self.assertIn('Outside the current automatic local-profile route', done.stdout)
        self.assertEqual(manifest(), before)

    def test_shareable_observations_preserve_out_of_scope_inventory(self):
        self.populate()
        for path in self.folder.glob('DIAG*.TXT'):
            path.write_text(path.read_text().replace('220410de', '248210de'), encoding='ascii')
        report = self.result()
        self.assertEqual(report['format'], 2)
        self.assertEqual(report['status'], 'outside_scope')
        observation = report['observations'][0]
        self.assertTrue(observation['available'])
        self.assertEqual(observation['nvidia_display_count'], 1)
        self.assertEqual(observation['other_display_count'], 0)
        self.assertEqual(observation['display_controllers'][0]['pci_id'], '248210de')
        self.assertEqual(observation['display_controllers'][0]['location']['bus'], 0x2b)
        self.assertEqual(observation['edid_active_open_count'], 0)

    def test_unobserved_is_not_zero_and_malformed_graph_is_not_absence(self):
        text = self.populate()
        path = self.folder / 'DIAG00.TXT'
        path.write_text(re.sub(r'^mmio .*\n', '', text, flags=re.M), encoding='ascii')
        observation = self.result()['observations'][0]
        self.assertIsNone(observation['chip_id'])
        self.assertIsNone(observation['device_memory_size_mib'])
        self.assertEqual(observation['register_snapshot'], 'not_recorded')
        path.write_text(text.replace('value=0x00006000', 'value=0x00000000'), encoding='ascii')
        self.assertEqual(self.result()['observations'][0]['device_memory_size_mib'], 0)
        path.write_text(text.replace('capture-complete=1', 'capture-complete=0'), encoding='ascii')
        self.assertEqual(self.result()['observations'][0], {'capture': 'DIAG00.TXT', 'available': False})

    def test_missing_or_incomplete_driver_inventory_is_unknown(self):
        text = self.populate()
        path = self.folder / 'DIAG00.TXT'
        for change in (text.replace('driver-bindings=00000003\n', ''),
                       text.replace('driver-bindings=00000003', 'driver-bindings=00000004'),
                       text.replace('binding handle=0x4', 'binding broken=0x4')):
            path.write_text(change, encoding='ascii')
            observation = self.result()['observations'][0]
            self.assertIsNone(observation['firmware_bindings'])
            self.assertIsNone(observation['firmware_bindings_truncated'])


if __name__ == '__main__':
    unittest.main()
