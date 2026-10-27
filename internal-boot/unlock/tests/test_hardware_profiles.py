"""Check reviewed machine profiles stay wired and mutually exclusive."""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
CLANG = sys.argv[1] if len(sys.argv) > 1 else 'clang'
BASE = [
    CLANG, '-std=c11', '-E', '-dM', '-x', 'c', '-',
    '-I', str(ROOT / 'compat'),
    '-I', str(REPO / 'deps/edk2/MdePkg/Include'),
    '-I', str(REPO / 'deps/edk2/MdePkg/Include/X64'),
]

PROFILES = {
    'b650m-3090fe': {
        'define': 'GU_PROFILE_B650M_3090FE',
        'name': 'ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE',
        'board': '0x220410deU',
        'subsystem': '0x145410deU',
        'chip': '0xb72000a1U',
        'memory': '0x6000U',
        'prom': '8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4',
        'aggregate': '0',
        'topo': '0',
    },
    'b660m-3070ti': {
        'define': 'GU_PROFILE_B660M_3070TI',
        'name': 'ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI',
        'board': '0x248210deU',
        'subsystem': '0x50521462U',
        'chip': '0xb74000a1U',
        'memory': '0x2000U',
        'prom': 'f429aa1d5f0f18d11ed9a19503683a674fe3541ae77e959df8e732652c2b4df0',
        'aggregate': '1',
        'topo': '0',
    },
    'b760m-3080lhr': {
        'define': 'GU_PROFILE_B760M_3080LHR',
        'name': 'MAXSUN-MS-TERMINATOR-B760M-D4-H7.8G-RTX3080LHR',
        'board': '0x221610deU',
        'subsystem': '0x20007377U',
        'chip': '0xb72000a1U',
        'memory': '0x2800U',
        'prom': '88a6bce1bd790d3e4c542cccafbbd88a7ef73e2e0bef34310d7e8ed780e56d25',
        'aggregate': '1',
        'topo': '1',
    },
}


def macro_map(defines):
    result = subprocess.run(
        BASE + ['-D' + define for define in defines],
        cwd=ROOT, input='#include "port.h"\n', text=True, capture_output=True,
    )
    if result.returncode != 0:
        raise AssertionError((defines, result.stderr))
    macros = {}
    for line in result.stdout.splitlines():
        match = re.match(r'#define\s+(\w+)\s+(.*)$', line)
        if match:
            macros[match.group(1)] = match.group(2).strip()
    return macros


def quoted(value):
    return '"' + value + '"'


for profile, expected in PROFILES.items():
    macros = macro_map([expected['define']])
    assert macros['GU_PROFILE_NAME'] == quoted(expected['name']), (profile, macros.get('GU_PROFILE_NAME'))
    for key, macro in (
        ('board', 'GU_BOARD_ID'),
        ('subsystem', 'GU_BOARD_SUBSYSTEM'),
        ('chip', 'GU_CHIP_ID'),
        ('memory', 'GU_DEVICE_MEMORY_SIZE'),
    ):
        assert macros[macro] == expected[key], (profile, key, macros.get(macro))
    assert macros['GU_PROM_SHA256'] == quoted(expected['prom']), (profile, 'prom')
    assert macros['GU_CONSOLE_AGGREGATE'] == expected['aggregate'], (profile, 'aggregate')
    assert macros['GU_TOPO_B760M'] == expected['topo'], (profile, 'topo')

default = macro_map([])
assert default['GU_PROFILE_NAME'] == quoted(PROFILES['b650m-3090fe']['name']), default.get('GU_PROFILE_NAME')

for first, second in (
    ('b650m-3090fe', 'b660m-3070ti'),
    ('b650m-3090fe', 'b760m-3080lhr'),
    ('b660m-3070ti', 'b760m-3080lhr'),
):
    result = subprocess.run(
        BASE + ['-D' + PROFILES[first]['define'], '-D' + PROFILES[second]['define']],
        cwd=ROOT, input='#include "port.h"\n', text=True, capture_output=True,
    )
    assert result.returncode != 0 and 'Select exactly one machine profile' in result.stderr, (first, second, result.stderr)

for relative, required in (
    ('build.ps1', 'GU_PROFILE_B760M_3080LHR'),
    ('../Configure.ps1', 'b760m-3080lhr'),
    ('../install/headless-package.ps1', 'MAXSUN-MS-TERMINATOR-B760M-D4-H7.8G-RTX3080LHR'),
    ('../../research/headless/build-unlock.ps1', 'GU_PROFILE_B760M_3080LHR'),
):
    text = (ROOT / relative).read_text(encoding='utf-8-sig')
    assert required in text, relative

print('PASS 3 machine profiles, default, mutual exclusion and script wiring')
