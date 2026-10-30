"""Read-only, offline checks for the automatic local-profile route.

Reads existing DIAGnn.TXT / IMGnn.BIN / PROM.BIN captures. Never executes a
capture, external tool, probe or unlock, and never generates a profile.
This is not a hardware compatibility certificate or a write-build gate.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import struct
import sys

# Also protect direct invocation without -I/-B: importing the shared parsers
# must not create bytecode files in the source tree.
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('compatibility_profile', HERE / 'make-profile.py')
mp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mp)
DIAG_LIMIT = 8 * 1024 * 1024
TOTAL_LIMIT = 64 * 1024 * 1024
CHECKER_VERSION = 1

# Messages are deliberately allowlisted. Parser exceptions can contain capture
# content, addresses and private filenames; none are echoed into a report.
MESSAGES = {
    'candidate': ('Local-profile candidate; further validation required.', '자동 로컬 프로필 후보입니다. 추가 검증이 필요합니다.'),
    'needs_data': ('More capture data is required.', '진단 자료가 더 필요합니다.'),
    'outside_scope': ('Outside the current automatic local-profile route.', '현재 자동 로컬 프로필 처리 범위 밖입니다.'),
    'invalid_data': ('Capture data is invalid or inconsistent.', '진단 자료에 오류나 불일치가 있습니다.'),
    'error': ('The check could not finish.', '검사를 완료하지 못했습니다.'),
    'scope': ('Checks the automatic local-profile route only; existing fixed profiles require separate review.', '자동 로컬 프로필 경로만 검사합니다. 기존 고정 프로필의 적용 여부는 별도 확인이 필요합니다.'),
    'limits': ('No hardware/provenance, separate cold boots, FWSEC parsing, host tests or Windows recovery verified. No unlock or write eligibility granted.', '실기기·자료 출처·서로 다른 완전 종료 부팅·FWSEC 파싱·호스트 시험·Windows 복구는 검증하지 않았습니다. 언락 실행이나 쓰기 빌드를 허용하는 결과가 아닙니다.'),
    'next_candidate': ('Continue with docs/NEW-MACHINE.en.md: profile generation and required host tests, then separate supervised hardware validation.', 'docs/NEW-MACHINE.en.md의 프로필 생성과 필수 호스트 시험을 진행하세요. 실제 장비 검증은 별도 단계입니다.'),
    'next_data': ('Follow docs/NEW-MACHINE.en.md to obtain the missing captures/files. This checker does not collect them or change boot settings.', 'docs/NEW-MACHINE.en.md에서 누락 자료 수집 절차를 확인하세요. 이 검사기는 자료를 수집하거나 부팅 설정을 변경하지 않습니다.'),
    'next_review': ('See docs/COMPATIBILITY.en.md and docs/NEW-MACHINE.en.md for exact configurations and capture review. Do not force another profile.', 'docs/COMPATIBILITY.ko.md와 docs/NEW-MACHINE.en.md에서 정확한 구성과 자료 검토 절차를 확인하세요. 다른 프로필을 강제로 적용하지 마세요.'),
    'no_captures': ('No DIAGnn.TXT captures found in the selected folder.', '선택한 폴더에 DIAGnn.TXT 진단 파일이 없습니다.'),
    'capture_count': ('At least the required number of agreeing captures is needed; filenames do not prove separate cold boots.', '필요한 횟수의 일치하는 캡처가 필요합니다. 파일 이름만으로 서로 다른 완전 종료 부팅을 증명할 수는 없습니다.'),
    'input_limit': ('Input exceeds the file-size, file-count or total-read limit.', '입력이 파일 크기·파일 수·전체 읽기 제한을 초과했습니다.'),
    'input_unreadable': ('The folder or a required regular file could not be read.', '폴더나 필요한 일반 파일을 읽을 수 없습니다.'),
    'invalid_graph': ('Incomplete, malformed or ambiguous diagnostic ownership graph.', '진단 소유권 그래프가 불완전하거나 형식이 잘못됐거나 모호합니다.'),
    'gpu_scope': ('The captured NVIDIA display configuration is outside the reviewed automatic profile envelope; this does not mean all project routes are unsupported.', '캡처된 NVIDIA 디스플레이 구성이 검증된 자동 프로필 범위 밖입니다. 프로젝트의 모든 경로에서 미지원이라는 뜻은 아닙니다.'),
    'topology_scope': ('The console, PCI path or driver ownership layout is not accepted by the current generator.', '콘솔·PCI 경로·드라이버 소유 구조가 현재 생성기의 허용 범위와 다릅니다.'),
    'driver_scope': ('The firmware driver identity or code is outside the reviewed local-profile envelope.', '펌웨어 드라이버 종류나 코드가 검증된 로컬 프로필 범위 밖입니다.'),
    'attributes_missing': ('A successful PCI attribute observation is missing; use captures from the current probe.', '정상적인 PCI 속성 기록이 없습니다. 현재 진단 도구로 수집한 자료가 필요합니다.'),
    'attributes_mismatch': ('PCI attributes disagree with the command register; release would be refused.', 'PCI 속성과 명령 레지스터가 일치하지 않아 해제 절차가 거부되는 상태입니다.'),
    'cold_state': ('The captured GPU state is not accepted as power-on state.', '캡처된 GPU 상태가 허용된 전원 초기 상태와 다릅니다.'),
    'deep_capture_missing': ('Required GPU register observations are missing or failed.', '필수 GPU 레지스터 기록이 없거나 읽기에 실패했습니다.'),
    'observations_invalid': ('The observations failed the shared profile checks; review the original capture locally.', '관측값이 공통 프로필 검사를 통과하지 못했습니다. 원본 자료를 로컬에서 검토하세요.'),
    'observations_pass': ('Capture observations pass the shared structural/profile checks.', '캡처 관측값이 공통 구조·프로필 검사를 통과했습니다.'),
    'captures_disagree': ('Captures disagree on profile constants, PCI configuration or display inventory.', '캡처 사이의 프로필 상수·PCI 구성·디스플레이 장치 정보가 일치하지 않습니다.'),
    'missing_images': ('Required IMGnn.BIN firmware driver dumps or dump references are missing.', '필요한 IMGnn.BIN 펌웨어 드라이버 덤프나 덤프 참조가 없습니다.'),
    'images_invalid': ('Firmware driver dumps failed loaded-image bounds, code or hash checks.', '펌웨어 드라이버 덤프가 이미지 범위·코드·해시 검사를 통과하지 못했습니다.'),
    'images_pass': ('Latest capture driver code passes the shared loaded-image and reviewed nvgop hash checks.', '마지막 캡처의 드라이버 코드가 공통 이미지 검사와 검증된 nvgop 해시 검사를 통과했습니다.'),
    'missing_prom': ('PROM.BIN is missing.', 'PROM.BIN이 없습니다.'),
    'prom_invalid': ('PROM.BIN size/hash does not match the agreeing capture observations.', 'PROM.BIN의 크기·해시가 일치하는 캡처 기록과 다릅니다.'),
    'prom_pass': ('PROM.BIN size/hash matches; FWSEC parsing is still pending.', 'PROM.BIN 크기·해시가 일치합니다. FWSEC 파싱은 아직 수행하지 않았습니다.'),
    'tool_error': ('The checker or its installed envelope is invalid; check the source installation.', '검사기나 설치된 허용 범위 파일에 오류가 있습니다. 소스 설치 상태를 확인하세요.'),
}


class InputLimit(ValueError):
    pass


class Snapshot:
    def __init__(self, name, data):
        self.name, self.data = name, data

    def read_bytes(self):
        return self.data


class Inputs:
    """Bound every read and retain immutable bytes for repeated parser calls."""
    def __init__(self, folder):
        self.folder, self.cache, self.total = folder, {}, 0

    def read(self, name, limit):
        if name not in self.cache:
            path = self.folder / name
            if not stat.S_ISREG(path.stat().st_mode):
                raise OSError('not a regular file')
            with path.open('rb') as stream:
                if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                    raise OSError('not a regular file')
                data = stream.read(min(limit, TOTAL_LIMIT - self.total) + 1)
            if len(data) > limit or self.total + len(data) > TOTAL_LIMIT:
                raise InputLimit()
            self.total += len(data)
            self.cache[name] = Snapshot(name, data)
        return self.cache[name]

    def __truediv__(self, name):
        # Only the shared code_hash's fixed IMG filename may reach the filesystem.
        if not re.fullmatch(r'IMG[0-9]{2,20}\.BIN', name):
            raise InputLimit()
        return self.read(name, mp.IMAGE_MAX)


def validate_envelope(envelope):
    """Installation errors must not be blamed on the user's captures."""
    def hex_value(value, pattern):
        return isinstance(value, str) and re.fullmatch(pattern, value) is not None

    def require(condition):
        if not condition:
            raise ValueError('invalid installed envelope')

    try:
        require(type(envelope['min_captures']) is int and 2 <= envelope['min_captures'] <= 32)
        require(hex_value(envelope['class_revision'], r'[0-9a-f]{8}'))
        ids = envelope['probe_device_ids']
        require(isinstance(ids, list) and ids and all(hex_value(v, r'[0-9a-f]{8}') for v in ids))
        require(len(set(ids)) == len(ids))
        chips = envelope['chips']
        require(isinstance(chips, list) and chips)
        admitted_ids = set()
        for chip in chips:
            require(hex_value(chip['chip'], r'[0-9a-f]{8}') and isinstance(chip['family'], str))
            memory = chip['memory']
            require(isinstance(memory, list) and memory and all(hex_value(v, r'[0-9a-f]{1,8}') for v in memory))
            cards = chip['pci_memory']
            require(isinstance(cards, dict) and cards)
            require(all(card in ids for card in cards))
            for card, sizes in cards.items():
                require(card not in admitted_ids)
                admitted_ids.add(card)
                require(isinstance(sizes, list) and sizes and len(set(sizes)) == len(sizes))
                require(all(isinstance(size, str) and size in memory for size in sizes))
            require(set(memory) == {size for sizes in cards.values() for size in sizes})
        require(admitted_ids == set(ids))
        require(len({chip['chip'] for chip in chips}) == len(chips))
        drivers = envelope['nvgop']
        require(isinstance(drivers, list) and drivers)
        for driver in drivers:
            require(hex_value(driver['version'], r'[0-9a-f]{1,8}'))
            require(hex_value(driver['text_sha256'], r'[0-9a-f]{64}'))
        require(len({(int(d['version'], 16), d['text_sha256']) for d in drivers}) == len(drivers))
        for chip in chips:
            allowed = chip['nvgop']
            require(isinstance(allowed, list) and allowed)
            require(all(isinstance(driver, dict) and
                        any(driver.get('version') == known['version'] and driver.get('text_sha256') == known['text_sha256']
                            for known in drivers) for driver in allowed))
            require(len({(driver['version'], driver['text_sha256']) for driver in allowed}) == len(allowed))
            if 'pci_nvgop' in chip or chip['chip'] == 'b74000a1':
                try:
                    for card in chip['pci_memory']:
                        mp.card_nvgop(chip, int(card, 16))
                except mp.Refused:
                    raise ValueError('invalid installed envelope') from None
    except (KeyError, TypeError):
        raise ValueError('invalid installed envelope') from None


def observation_reason(error):
    message = str(error).lower()
    for needles, state, code in [
        (('outside the reviewed deep-probe', 'not hardware-proven', 'no hardware-proven', 'class/revision'), 'outside_scope', 'gpu_scope'),
        (('no pciio attribute', 'reading the pciio attributes failed'), 'needs_data', 'attributes_missing'),
        (('disagree with the pci command',), 'invalid_data', 'attributes_mismatch'),
        (('missing a cold-state',), 'needs_data', 'deep_capture_missing'),
        (('power-on state', 'memory decode must'), 'invalid_data', 'cold_state'),
        (('image file', 'not boot-services', 'driver does not match'), 'outside_scope', 'driver_scope'),
        (('conout', 'unreviewed', 'integrated gpu', 'gop child', 'only gop', 'by_driver', 'no-edid', 'display child', 'the rtx must'), 'outside_scope', 'topology_scope'),
    ]:
        if any(needle in message for needle in needles):
            return state, code
    return 'invalid_data', 'observations_invalid'


def shared_observations(name, text, blocks):
    """Typed model/topology observations, never addresses or raw capture text."""
    adapters = []
    for block in blocks.values():
        cfg = mp.pci_config(block['raw'])
        if not cfg or cfg[2] >> 24 != 3:
            continue
        locations = re.findall(r'^pci location-status=.*$', block['raw'], re.M)
        row = re.fullmatch(r'pci location-status=0x([0-9a-f]{1,16}) segment=0x([0-9a-f]{1,16}) bus=0x([0-9a-f]{1,16}) device=0x([0-9a-f]{1,16}) function=0x([0-9a-f]{1,16})', locations[0]) if len(locations) == 1 else None
        location = None
        if row:
            status, segment, bus, device, function = (int(v, 16) for v in row.groups())
            if status == 0 and segment <= 0xffff and bus <= 255 and device <= 31 and function <= 7:
                location = dict(segment=segment, bus=bus, device=device, function=function)
        adapters.append({'pci_id': f'{cfg[0]:08x}', 'subsystem_id': f'{cfg[11]:08x}',
                         'class_revision': f'{cfg[2]:08x}', 'location': location})
    adapters.sort(key=lambda adapter: json.dumps(adapter, sort_keys=True))
    edges = [edge for block in blocks.values() for edge in block['edges'] if edge[0] == mp.EDID_DISCOVERED]
    result = {'capture': name, 'available': True,
              'display_controller_count': len(adapters),
              'nvidia_display_count': sum(a['pci_id'].endswith('10de') for a in adapters),
              'other_display_count': sum(not a['pci_id'].endswith('10de') for a in adapters),
              'display_controllers': adapters[:16], 'display_controllers_truncated': len(adapters) > 16,
              'gop_handle_count': sum(mp.GOP in block['protocols'] for block in blocks.values()),
              'edid_by_driver_open_count': sum(edge[3] == mp.BY_DRIVER for edge in edges),
              'edid_active_open_count': sum(edge[3] not in (1, 2, 4) for edge in edges),
              'chip_id': None, 'device_memory_size_mib': None,
              'register_snapshot': 'not_recorded', 'firmware_bindings': None,
              'firmware_bindings_truncated': None}
    try:
        values, statuses = mp.probe_mmio(text)
        result['register_snapshot'] = ('complete' if all(offset in values for offset in mp.COLD)
                                       else 'partial' if statuses else 'not_recorded')
        if 0 in values:
            result['chip_id'] = f'{values[0]:08x}'
        result['device_memory_size_mib'] = values.get(0x1183a4)
    except mp.Refused:
        result['register_snapshot'] = 'invalid'
    known = {('guid', mp.OUT_FILE): 'console_text', ('guid', mp.GFX_FILE): 'console_gfx',
             ('guid', mp.MOUSE_FILE): 'edid', ('name', mp.NVGOP_NAME): 'nvgop'}
    try:
        bindings = mp.parse_bindings(text.splitlines())
        declared = re.findall(r'^driver-bindings=([0-9a-f]{1,8})$', text, re.M)
        headers = [line for line in text.splitlines() if line.startswith('binding')]
        if (len(declared) != 1 or int(declared[0], 16) != len(bindings) or
                len(bindings) != len(headers) or len({b['handle'] for b in bindings}) != len(bindings) or
                any(b['file'] is None for b in bindings)):
            return result
        known_bindings = sorted([
            {'role': known[b['file']],
             'version': f'{b["version"]:08x}' if 0 <= b['version'] <= 0xffffffff else None,
             'image_bytes': b.get('size') if 0 <= b.get('size', -1) <= mp.IMAGE_MAX else None}
            for b in bindings if b['file'] in known
        ], key=lambda binding: json.dumps(binding, sort_keys=True))
        result['firmware_bindings'] = known_bindings[:16]
        result['firmware_bindings_truncated'] = len(known_bindings) > 16
    except (ValueError, KeyError, TypeError):
        # Inventory still has value when a binding/path observation is malformed.
        pass
    return result


def check(folder, envelope):
    report = {'format': 2, 'checker_version': CHECKER_VERSION, 'envelope_sha256': None,
              'route': 'automatic_local_profile', 'status': 'candidate',
              'hardware_tested': False, 'write_eligible': False,
              'checks': [], 'capture_count': 0, 'observations': []}
    priority = {'candidate': 0, 'needs_data': 1, 'outside_scope': 2, 'invalid_data': 3, 'error': 4}

    def add(state, code, capture=None):
        entry = {'state': state, 'code': code}
        if capture is not None:
            entry['capture'] = capture
        report['checks'].append(entry)
        if priority[state] > priority[report['status']]:
            report['status'] = state

    try:
        validate_envelope(envelope)
    except ValueError:
        add('error', 'tool_error')
        return report
    report['envelope_sha256'] = hashlib.sha256(json.dumps(envelope, sort_keys=True).encode()).hexdigest()
    try:
        if not folder.is_dir():
            raise OSError('not a folder')
        diags = []
        for path in folder.iterdir():
            if re.fullmatch(r'DIAG[0-9]{2}\.TXT', path.name):
                diags.append(path.name)
                if len(diags) > 32:
                    raise InputLimit()
        diags.sort()
        report['capture_count'] = len(diags)
        if not diags:
            add('needs_data', 'no_captures')
            return report
        if len(diags) < envelope['min_captures']:
            add('needs_data', 'capture_count')
        inputs, observations = Inputs(folder), []
        for name in diags:
            diag = inputs.read(name, DIAG_LIMIT)
            text = diag.data.decode('utf-8-sig').replace('\r\n', '\n')
            try:
                blocks = mp.parse_handles(text)
            except mp.Refused:
                report['observations'].append({'capture': name, 'available': False})
                add('invalid_data', 'invalid_graph', name)
                continue
            report['observations'].append(shared_observations(name, text, blocks))
            nvidia = [cfg[0] for block in blocks.values()
                      if (cfg := mp.pci_config(block['raw'])) and cfg[0] & 0xffff == 0x10de and cfg[2] >> 24 == 3]
            if len(nvidia) != 1 or f'{nvidia[0]:08x}' not in envelope['probe_device_ids']:
                add('outside_scope', 'gpu_scope', name)
                continue
            try:
                result = mp.analyze(diag, None, envelope)
            except mp.Refused as error:
                state, code = observation_reason(error)
                add(state, code, name)
                continue
            observations.append(result)
            add('candidate', 'observations_pass', name)
        if len(observations) != len(diags):
            return report
        latest = observations[-1]
        for earlier in observations[:-1]:
            if any(earlier[key] != latest[key] for key in ('defines', 'config', 'igpu_id', 'no_edid_driver', 'child_paths')):
                add('invalid_data', 'captures_disagree')
                return report
        try:
            mp.analyze(inputs.cache[diags[-1]], inputs, envelope)
            add('candidate', 'images_pass')
        except FileNotFoundError:
            add('needs_data', 'missing_images')
        except mp.Refused as error:
            if 'has no image dump' in str(error):
                add('needs_data', 'missing_images')
            elif 'has not released cleanly' in str(error):
                add('outside_scope', 'driver_scope')
            else:
                add('invalid_data', 'images_invalid')
        try:
            prom = inputs.read('PROM.BIN', 1048576).data
            if len(prom) != 1048576 or hashlib.sha256(prom).hexdigest() != latest['prom_sha256']:
                add('invalid_data', 'prom_invalid')
            else:
                add('candidate', 'prom_pass')
        except FileNotFoundError:
            add('needs_data', 'missing_prom')
    except InputLimit:
        add('invalid_data', 'input_limit')
    except OSError:
        add('invalid_data', 'input_unreadable')
    except (UnicodeError, ValueError, KeyError, TypeError, struct.error):
        add('invalid_data', 'observations_invalid')
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path, help='folder containing existing DIAGnn.TXT captures')
    parser.add_argument('--lang', choices=('en', 'ko'), default='en')
    parser.add_argument('--json', action='store_true', help='print a privacy-limited JSON report to stdout')
    args = parser.parse_args(argv)
    try:
        envelope = json.loads((HERE / 'proven.json').read_text(encoding='utf-8'))
        report = check(args.captures, envelope)
    except (OSError, ValueError, KeyError, TypeError):
        report = {'format': 2, 'checker_version': CHECKER_VERSION, 'envelope_sha256': None,
                  'route': 'automatic_local_profile', 'status': 'error', 'observations': [],
                  'hardware_tested': False, 'write_eligible': False, 'capture_count': 0,
                  'checks': [{'state': 'error', 'code': 'tool_error'}]}
    next_code = ('next_candidate' if report['status'] == 'candidate' else
                 'next_data' if report['status'] == 'needs_data' else 'next_review')
    language = 1 if args.lang == 'ko' else 0
    report['summary'] = MESSAGES[report['status']][language]
    report['scope'] = MESSAGES['scope'][language]
    report['limitations'] = MESSAGES['limits'][language]
    report['next_step'] = MESSAGES[next_code][language]
    for entry in report['checks']:
        entry['message'] = MESSAGES[entry['code']][language]
    if args.json:
        print(json.dumps(report, ensure_ascii=True, indent=2))
    else:
        print(report['summary'])
        print(report['scope'])
        for entry in report['checks']:
            print(f"- [{entry['code']}] {entry['message']}")
        print(report['next_step'])
        print(report['limitations'])
    return 0 if report['status'] == 'candidate' else 3 if report['status'] == 'error' else 2


if __name__ == '__main__':
    sys.exit(main())
