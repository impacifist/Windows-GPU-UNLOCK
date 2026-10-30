"""Turn a real successful diagnostic into an ignored host replay fixture.

No firmware or GPU access. Actual addresses are replaced by ordinal handles.
The source log stays private/local; it is not copied into tracked source.
"""
import hashlib
import importlib.util
import re
import sys
import uuid
from pathlib import Path

source = Path(sys.argv[1])
single = '--single-rtx' in sys.argv[2:]
aggregate = '--aggregate' in sys.argv[2:]
if aggregate and not single:
    raise ValueError('--aggregate requires --single-rtx')
def option(name, default):
    """--name=value override; defaults are the original B650M/3090 FE capture."""
    for arg in sys.argv[2:]:
        if arg.startswith(f'--{name}='):
            return arg.split('=', 1)[1]
    return default
rtx_id = option('rtx-id', '220410de')
amd_id = option('amd-id', '164e1002')
rtx_fb = option('rtx-fb', '0x000000fcc0000000')
raw = source.read_bytes()
text = raw.decode().replace('\r\n', '\n')
graph_spec = importlib.util.spec_from_file_location('fixture_capture_graph', Path(__file__).resolve().parents[2] / 'research/profile/capture_graph.py')
graph = importlib.util.module_from_spec(graph_spec)
graph_spec.loader.exec_module(graph)
parsed = graph.parse_handles(text)
blocks = {hex(h): block['raw'] for h, block in parsed.items()}
indices = {h: i + 1 for i, h in enumerate(blocks)}
edges = []
for handle, block in parsed.items():
    for protocol, agent, controller, attrs, count in block['edges']:
        if attrs not in (1, 2, 4):
            edges.append((indices[hex(handle)], indices[hex(agent)], indices[hex(controller)] if controller else 0, uuid.UUID(protocol), attrs, count))
def one(predicate):
    values = [h for h, block in blocks.items() if predicate(block)]
    if len(values) != 1:
        raise ValueError('Expected exactly one role')
    return indices[values[0]]
no_igpu = '--no-igpu' in sys.argv[2:]
if no_igpu and not single:
    raise ValueError('--no-igpu is only for the single-RTX console replay')
if no_igpu:
    for block in blocks.values():
        config = re.search(r'^pci config-dwords=([^\n]+)$', block, re.M)
        if config:
            words = [int(word, 16) for word in config[1].split()]
            if words[0] & 0xffff != 0x10de and words[2] >> 24 == 3:
                raise ValueError('--no-igpu disagrees with the display inventory')
# This firmware binds no EDID/mouse driver to the RTX display handles (GU_NO_EDID_DRIVER).
no_edid_driver = '--no-edid-driver' in sys.argv[2:]
if aggregate and no_edid_driver:
    raise ValueError('Aggregate replay requires an EDID driver')
if no_edid_driver and not single:
    raise ValueError('--no-edid-driver is only for the single-RTX console replay')
EDID_DISCOVERED = '1c0c34f6-d380-41fa-a049-8ad06c1a66aa'
if no_edid_driver and any(str(e[3]) == EDID_DISCOVERED for e in edges):
    raise ValueError('--no-edid-driver was given but the graph has an active EdidDiscovered owner')
roles = {
    'RtxPci': one(lambda b: f'pci config-dwords={rtx_id}' in b),
    # Without an integrated GPU the replay has no second adapter. The legacy two-adapter test
    # code that test_r2.c includes still names this handle, so give it an ordinal that exists in
    # no graph block; the single-RTX scenarios never reach it.
    'AmdPci': len(blocks) + 1 if no_igpu else one(lambda b: f'pci config-dwords={amd_id}' in b),
    'Rtx': one(lambda b: f'gop framebuffer={rtx_fb}' in b),
    'Amd': 0 if single else one(lambda b: 'gop framebuffer=0x000000fce0000000' in b),
}
if single:
    child_records = [e for e in edges if e[0] == roles['RtxPci'] and str(e[3]) == '09576e91-6d3f-11d2-8e39-00a0c969723b' and e[4] == 8 and e[2] != roles['Rtx']]
    # Only non-aggregate firmware without an EDID driver admits up to three
    # siblings. Preserve the legacy exact-two-child option when not disabled.
    child_limit = 3 if no_edid_driver else 1
    if len(child_records) > child_limit or len({record[2] for record in child_records}) != len(child_records) or \
            ('--no-edid-child' not in sys.argv[2:] and len(child_records) != 1):
        raise ValueError('Unexpected additional RTX display child count')
    if child_records:
        roles['EdidChild'] = child_records[0][2]
    else:
        roles['EdidChild'] = 0  # No physical second child; tests must not invent one.
spec = importlib.util.spec_from_file_location('console_fixture_profile', Path(__file__).resolve().parents[2] / 'research/profile/make-profile.py')
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)
if aggregate:
    # Use the same strict graph-role inspection as admission. Fixture generation
    # is data preparation, never write eligibility or hardware evidence.
    original = {index: int(handle, 16) for handle, index in indices.items()}
    observed_bindings = profile.parse_bindings(text.splitlines())
    topology = profile.aggregate_roles(text, parsed, original[roles['RtxPci']], original[roles['Rtx']], observed_bindings)
    if topology is None:
        raise ValueError('Expected an aggregate ownership graph')
    roles['Agg'] = indices[hex(topology['aggregate'])]
console_owner = roles['Agg'] if aggregate else roles['Rtx']
for role, protocol, controller in [
    ('OutAgent', '387477c2-69c7-11d2-8e39-00a0c969723b', console_owner),
    ('GfxAgent', '9042a9de-23dc-4a38-96fb-7aded080516a', console_owner),
    ('NvAgent', '4cf5b200-68b8-4ca5-9eec-b23e3f50029a', roles['RtxPci']),
    ('EdidAgent', EDID_DISCOVERED, roles['Rtx']),
] + ([('ComboAgent', '9042a9de-23dc-4a38-96fb-7aded080516a', roles['Rtx'])] if aggregate else []):
    if no_edid_driver and role == 'EdidAgent':
        continue
    records = [e for e in edges if e[0] == controller and str(e[3]) == protocol and e[4] == 16]
    if len(records) != 1:
        raise ValueError('Expected a unique consumer')
    roles[role] = records[0][1]
    block = blocks[next(h for h, i in indices.items() if i == roles[role])]
    if aggregate:
        observed = re.findall(r'^driver image=0x([0-9a-f]+) binding=0x([0-9a-f]+)$', block, re.M)
        binding = profile.binding_for(observed_bindings, original[roles[role]])
        if len(observed) != 1 or tuple(int(value, 16) for value in observed[0]) != (binding['image'], binding['handle']):
            raise ValueError('Aggregate graph and binding image records disagree')
    roles[role.removesuffix('Agent') + 'Image'] = indices[hex(int(re.search(r'driver image=(0x[0-9a-f]+)', block)[1], 16))]
console = re.search(r'console out=(0x[0-9a-f]+) error=(0x[0-9a-f]+) input=(0x[0-9a-f]+)', text)
for role, value in zip(['Console', 'Error', 'Input'], console.groups()):
    roles[role] = indices[hex(int(value, 16))]
lines = [f'/* Local diagnostic sha256 {hashlib.sha256(raw).hexdigest()} */', f'#define FX_HANDLES {len(blocks)}',
         f'#define FX_NO_EDID_DRIVER {1 if no_edid_driver else 0}',
         f'#define FX_EXTRA_DISPLAY_CHILD {1 if roles.get("EdidChild") else 0}']
if single:
    if any(record[1]!=roles['NvAgent'] or record[5]!=1 for record in child_records):
        raise ValueError('Additional display child has an unreviewed NVIDIA owner/count')
    if no_edid_driver and any(EDID_DISCOVERED not in parsed[int(handle,16)]['protocols']
            for handle,index in indices.items() if index in [roles['Rtx']]+[record[2] for record in child_records]):
        raise ValueError('Every no-EDID display child requires its captured EdidDiscovered interface')
    lines.append(f'#define FX_EXTRA_DISPLAY_CHILD_COUNT {len(child_records)}')
    lines.append('#define FX_EXTRA_CHILD_HANDLES {'+(','.join(f'(EFI_HANDLE){record[2]}' for record in child_records) or 'NULL')+'}')
primary_key=next(handle for handle,index in indices.items() if index==roles['Rtx'])
primary_block=blocks[primary_key]
primary_path=profile.path_of(primary_block)
if profile.PATH not in parsed[int(primary_key,16)]['protocols']:
    raise ValueError('Replay requires the complete primary display path')
primary_layout=profile.gpu_display_path(primary_path)
controller_bytes=primary_layout['controller_bytes']
pci_block=blocks[next(handle for handle,index in indices.items() if index==roles['RtxPci'])]
parent_path=profile.path_of(pci_block)
if parent_path is None or len(parent_path)!=controller_bytes+4 or parent_path[-4:]!=bytes.fromhex('7fff0400') or parent_path[:-4]!=primary_path[:controller_bytes]:
    raise ValueError('Replay primary path does not belong to the PCI controller')
lines.append(f'#define FX_DISPLAY_PATH_BYTES {primary_layout["path_bytes"]}')
lines.append(f'#define FX_CONTROLLER_BYTES {controller_bytes}')
lines.append('#define FX_PRIMARY_PATH_BYTES '+','.join(str(byte) for byte in primary_path))
if single:
    locations=re.findall(r'^pci location-status=0x([0-9a-f]{1,16}) segment=0x([0-9a-f]{1,16}) bus=0x([0-9a-f]{1,16}) device=0x([0-9a-f]{1,16}) function=0x([0-9a-f]{1,16})$',pci_block,re.M)
    if len(locations)!=1 or int(locations[0][0],16):raise ValueError('Replay requires a successful PCI location observation')
    segment,bus,device,function=(int(value,16) for value in locations[0][1:])
    if segment>0xffff or not 1<=bus<=255 or device or function:raise ValueError('Replay PCI location is outside the target scope')
    for name,value in zip(('SEGMENT','BUS','DEVICE','FUNCTION'),locations[0][1:]):
        lines.append(f'#define FX_PCI_{name} 0x{int(value,16):x}ULL')
if aggregate:
    lines.append('#define FX_CONSOLE_AGGREGATE 1')
    patterns=[(r'^gop framebuffer=0x([0-9a-f]+) bytes=0x([0-9a-f]+) mode=0x([0-9a-f]+) max-mode=0x([0-9a-f]+)$',
               ('FRAMEBUFFER','FRAMEBUFFER_BYTES','MODE','MAX_MODE'),(64,64,32,32)),
              (r'^gop width=0x([0-9a-f]+) height=0x([0-9a-f]+) stride=0x([0-9a-f]+)$',
               ('WIDTH','HEIGHT','STRIDE'),(32,32,32)),
              (r'^gop pixel-format=0x([0-9a-f]+) info-bytes=0x([0-9a-f]+)$',
               ('PIXEL_FORMAT','INFO_BYTES'),(32,64))]
    for pattern,names,widths in patterns:
        observed=re.findall(pattern,primary_block,re.M)
        if len(observed)!=1:
            raise ValueError('Aggregate replay requires complete unambiguous GOP mode data; recapture with the current probe')
        for name,value,width in zip(names,observed[0],widths):
            number=int(value,16)
            if number>=(1<<width):raise ValueError('Aggregate GOP field is outside its integer range')
            lines.append(f'#define FX_RTX_{name} 0x{number:x}ULL')
    for role in ('Out','Gfx','Nv','Edid','Combo'):
        if '18a031ab-b443-4d1a-a5c0-0c09261e9f71' not in parsed[original[roles[role+'Agent']]]['protocols']:
            raise ValueError('Aggregate driver binding interface is missing')
        binding=profile.binding_for(observed_bindings,original[roles[role+'Agent']])
        if not 0<binding['size']<=profile.IMAGE_MAX or not 0<=binding['version']<=0xffffffff:
            raise ValueError('Aggregate driver metadata is outside replay limits')
        for name,value in (('IMAGE_SIZE',binding['size']),('STOP',binding['stop']-binding['base']),
                           ('VERSION',binding['version']),('CODE_TYPE',binding['code-type'])):
            lines.append(f'#define FX_{role.upper()}_{name} 0x{value:x}U')
        image_block=parsed[binding['image']]
        if '5b1b31a1-9562-11d2-8e3f-00a0c969723b' not in image_block['protocols']:
            raise ValueError('Aggregate driver image interface is missing')
        image_rows=re.findall(r'^image parent=0x([0-9a-f]{1,16}) device=0x([0-9a-f]{1,16}) base=0x([0-9a-f]{1,16}) size=0x([0-9a-f]{1,16}) code-type=0x([0-9a-f]{1,16})$',image_block['raw'],re.M)
        if len(image_rows)!=1:
            raise ValueError('Aggregate replay requires one loaded image observation')
        parent,device,base,size,code_type=(int(value,16) for value in image_rows[0])
        if (base,size,code_type)!=(binding['base'],binding['size'],binding['code-type']):
            raise ValueError('Aggregate image and binding metadata disagree')
        image_nodes=re.findall(r'^image-file-path-node raw=([0-9a-f]+)$',image_block['raw'],re.M)
        if image_nodes!=[node.hex() for node in binding['nodes']]:
            raise ValueError('Aggregate image file paths disagree')
        if device and hex(device) not in indices:
            raise ValueError('Aggregate image device handle is unresolved')
        if role=='Nv' and device!=original[roles['RtxPci']]:
            raise ValueError('Aggregate NVIDIA image belongs to another device')
        lines.append(f'#define FX_{role.upper()}_DEVICE_HANDLE ((EFI_HANDLE){indices[hex(device)] if device else 0})')
    rows=re.findall(r'^cfg dwords=([0-9a-f ]+)$',text,re.M)
    words=rows[0].split() if len(rows)==1 else []
    if len(words)!=64 or any(not re.fullmatch(r'[0-9a-f]{1,8}',word) for word in words):
        raise ValueError('Aggregate replay requires one full PCI snapshot')
    words=[int(word,16) for word in words]
    if words[:16]!=profile.pci_config(pci_block):
        raise ValueError('Aggregate graph and full PCI snapshots disagree')
    targets=re.findall(r'^probe-target seg=0x([0-9a-f]{1,16}) bus=0x([0-9a-f]{1,16}) dev=0x([0-9a-f]{1,16}) fun=0x([0-9a-f]{1,16})$',text,re.M)
    if len(targets)!=1 or tuple(int(value,16) for value in targets[0])!=tuple(int(value,16) for value in locations[0][1:]):
        raise ValueError('Aggregate graph and probe locations disagree')
    attributes=re.findall(r'^pci-attributes status=0x([0-9a-f]{1,16}) value=0x([0-9a-f]{1,16})$',text,re.M)
    if len(attributes)!=1 or int(attributes[0][0],16):raise ValueError('Aggregate replay requires a successful PCI attribute observation')
    lines.append(f'#define FX_PCI_ATTRIBUTES 0x{int(attributes[0][1],16):x}ULL')
    resources=re.findall(r'^pci-bar0-resource raw=([0-9a-f]+)$',text,re.M)
    if len(resources)!=1 or len(resources[0])!=96:
        raise ValueError('Aggregate replay requires one full BAR resource observation; recapture with the current probe')
    resource=bytes.fromhex(resources[0])
    lines.append('static const UINT8 FixtureBar0Resource[]={'+','.join(f'0x{byte:x}' for byte in resource)+'};')
    lines.append('static const UINT32 FixtureFullRtxConfig[64]={'+','.join(f'0x{word:x}U' for word in words)+'};')
    cold,_=profile.probe_mmio(text)
    if any(offset not in cold for offset in profile.COLD):
        raise ValueError('Aggregate replay requires all cold register observations')
    lines.append('static const UINT32 FixtureCold[9]={'+','.join(f'0x{cold[offset]:x}U' for offset in profile.COLD)+'};')
    lines.append('static const UINT32 FixtureColdOffsets[9]={'+','.join(f'0x{offset:x}U' for offset in profile.COLD)+'};')
child_paths=[]
for child in ([record[2] for record in child_records] if single else []):
    child_key=next(h for h,index in indices.items() if index==child)
    child_block = blocks[child_key]
    if profile.GOP in parsed[int(child_key,16)]['protocols'] or profile.TEXT_OUT in parsed[int(child_key,16)]['protocols']:
        raise ValueError('Additional display child must not invent a physical GOP or text console')
    path_rows = re.findall(r'^path-node raw=([0-9a-f]+)$', child_block, re.M)
    child_path = b''.join(bytes.fromhex(row) for row in path_rows)
    child_layout=profile.gpu_display_path(child_path)
    if child_layout['controller_bytes']!=controller_bytes or child_path[:controller_bytes]!=primary_path[:controller_bytes]:
        raise ValueError('Additional display child has a different PCI route')
    if child_path==primary_path or child_path in child_paths:
        raise ValueError('Additional display children require unique paths')
    child_paths.append(child_path)
if single:
    lines.append('#define FX_EXTRA_CHILD_PATHS {'+(','.join('{'+','.join(str(byte) for byte in path)+'}' for path in child_paths) or '{0}')+'}')
if child_paths:
    lines.append('#define FX_EDID_CHILD_PATH_BYTES ' + ','.join(str(byte) for byte in child_paths[0]))
for role, value in roles.items():
    lines.append(f'#define FX_{role} ((EFI_HANDLE){value})')
lines.append('static const EDGE FixtureEdges[]={')
for h, a, c, g, attrs, count in edges:
    data = g.bytes
    guid = f'{{0x{g.time_low:x},0x{g.time_mid:x},0x{g.time_hi_version:x},{{' + ','.join(f'0x{x:x}' for x in data[8:]) + '}}'
    lines.append(f'{{(EFI_HANDLE){h},(EFI_HANDLE){a},(EFI_HANDLE){c},{guid},{attrs},{count}}},')
lines.append('};')
if aggregate:
    # Installed protocols remain present even when a consumer closes its open.
    # Keep their inventory separate from the active ownership edge snapshot.
    lines.append('static const struct {EFI_HANDLE Handle;EFI_GUID Guid;} FixtureInstalledProtocols[]={')
    for handle, block in parsed.items():
        for protocol in sorted(block['protocols']):
            g = uuid.UUID(protocol)
            guid = f'{{0x{g.time_low:x},0x{g.time_mid:x},0x{g.time_hi_version:x},{{' + ','.join(f'0x{x:x}' for x in g.bytes[8:]) + '}}'
            lines.append(f'{{(EFI_HANDLE){indices[hex(handle)]},{guid}}},')
    lines.append('};')
for role in ['Rtx', 'Amd']:
    if role == 'Amd' and no_igpu:
        lines.append('static const UINT32 FixtureAmdConfig[16]={0};')
        continue
    block = blocks[next(h for h, i in indices.items() if i == roles[role + 'Pci'])]
    cfg = re.search(r'pci config-dwords=([^\n]+)', block)[1].split()
    if len(cfg) != 16:
        raise ValueError('Incomplete PCI snapshot')
    lines.append('static const UINT32 Fixture' + role + 'Config[16]={' + ','.join('0x' + x for x in cfg) + '};')
default_target = Path(__file__).parent / ('build/r2/replay-fixture.h' if single else 'build/console/replay-fixture.h')
target = Path(option('output', str(default_target)))
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text('\n'.join(lines) + '\n')
print(f'Local replay fixture: {len(blocks)} handles, {len(edges)} active ownership records; ordinal addresses only')
