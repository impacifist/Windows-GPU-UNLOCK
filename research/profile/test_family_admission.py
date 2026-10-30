"""Exact family admission on synthetic captures; never hardware evidence."""
import copy
import importlib.util
import itertools
import json
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('family_topology', HERE / 'test_local_topology.py')
data = importlib.util.module_from_spec(spec)
spec.loader.exec_module(data)
spec = importlib.util.spec_from_file_location('family_checker', HERE / 'check-compatibility.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)
ENVELOPE = json.loads((HERE / 'proven.json').read_text())


class FamilyAdmissionTests(unittest.TestCase):
    def test_exact_card_chip_memory_tuples(self):
        accepted = {(0x220410de, 0xb72000a1, 0x6000), (0x220410de, 0xb72000a1, 0x5ff4),
                    (0x220310de, 0xb72000a1, 0x6000), (0x220310de, 0xb72000a1, 0x5ff4),
                    (0x248710de, 0xb74000a1, 0x3000), (0x250410de, 0xb76000a1, 0x3000),
                    (0x248410de, 0xb74000a1, 0x2000), (0x24c910de, 0xb74000a1, 0x2000), (0x220a10de, 0xb72000a1, 0x3000)}
        for board, chip, memory in itertools.product(
                (0x220410de, 0x220310de, 0x248710de, 0x250410de, 0x248410de, 0x24c910de, 0x248210de, 0x220a10de, 0x221610de, 0x999910de),
                (0xb72000a1, 0xb74000a1, 0xb76000a1, 0xb76000a2), (0x6000, 0x5ff4, 0x3000, 0x2800, 0x2000)):
            text = data.synthetic_capture(board=board, chip=chip, memory=memory)
            with self.subTest(board=board, chip=chip, memory=memory):
                if (board, chip, memory) in accepted:
                    values = dict(data.analyze(text, ENVELOPE)['defines'])
                    self.assertEqual(values['GU_CHIP_ID'], f'0x{chip:08x}U')
                else:
                    with self.assertRaises(data.mp.Refused):
                        data.analyze(text, ENVELOPE)

    def test_new_families_across_nonaggregate_topologies(self):
        for (board, chip), bus, igpu, no_edid, children in itertools.product(
                ((0x248710de, 0xb74000a1), (0x250410de, 0xb76000a1)),
                (1, 43, 255), (False, True), (False, True), (1, 2)):
            with self.subTest(board=board, bus=bus, igpu=igpu, no_edid=no_edid, children=children):
                result = data.analyze(data.synthetic_capture(board=board, chip=chip, memory=0x3000,
                                      bus=bus, igpu=igpu, no_edid=no_edid, children=children), ENVELOPE)
                self.assertEqual(result['no_edid_driver'], no_edid)

    def test_driver_code_is_bound_to_family(self):
        # Hashing is stubbed to isolate the admission decision, not prove driver bytes.
        cases=((0x220410de,0xb72000a1,0x6000,('60008','60009','60010','6000a')),
               (0x248710de,0xb74000a1,0x3000,('60010',)),
               (0x250410de,0xb76000a1,0x3000,('60010',)),
               (0x248410de,0xb74000a1,0x2000,('60009',)),
               (0x24c910de,0xb74000a1,0x2000,('60015',)),
               (0x220a10de,0xb72000a1,0x3000,('60010',)))
        for board,chip,memory,versions in cases:
            for version_code, hash_code in itertools.product(ENVELOPE['nvgop'], repeat=2):
                code={'version':version_code['version'],'text_sha256':hash_code['text_sha256']}
                text = data.synthetic_capture(board=board, chip=chip,
                                              memory=memory)
                text = re.sub(r'version=0x600(?:09|10|15)', 'version=0x'+code['version'], text)
                with tempfile.TemporaryDirectory() as folder:
                    diag=Path(folder)/'DIAG00.TXT';diag.write_text(text)
                    with patch.object(data.mp, 'code_hash', return_value={'sha256':code['text_sha256'], 'bytes':0x500}):
                        known=any(code['version']==item['version'] and code['text_sha256']==item['text_sha256']
                                  for item in ENVELOPE['nvgop'])
                        if known and code['version'] in versions:
                            data.mp.analyze(diag, Path(folder), ENVELOPE)
                        else:
                            with self.assertRaisesRegex(data.mp.Refused, 'not released cleanly'):
                                data.mp.analyze(diag, Path(folder), ENVELOPE)

    def test_installed_scope_rejects_missing_or_contradictory_relations(self):
        checker.validate_envelope(ENVELOPE)
        for change in ('missing_cards','cross_family','bad_memory','missing_driver','foreign_driver'):
            envelope=copy.deepcopy(ENVELOPE)
            chip=envelope['chips'][1]
            if change=='missing_cards':del chip['pci_memory']
            elif change=='cross_family':chip['pci_memory']['220410de']=['3000']
            elif change=='bad_memory':chip['pci_memory']['248710de']=['6000']
            elif change=='missing_driver':del chip['nvgop']
            else:chip['nvgop']=[{'version':'60010','text_sha256':'0'*64}]
            with self.subTest(change=change), self.assertRaises(ValueError):
                checker.validate_envelope(envelope)


if __name__ == '__main__':
    unittest.main()
