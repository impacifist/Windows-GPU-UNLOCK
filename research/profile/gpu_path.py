"""Bounded GPU display-path contract mirrored by unlock/gpu-path.h.

Structure alone never establishes PCI identity or write eligibility. Discovery
must bind the exact ancestry to an independently verified target PCI handle.
"""
MAX_PCI_NODES=16
MAX_DISPLAY_BYTES=12+6*MAX_PCI_NODES+8+4


def decode_gpu_display_path(path,*,allow_trailing=False):
    """Decode an exact capture path, or a C-style read bound for comparison tests."""
    def refuse():
        raise ValueError('GPU display path is outside the bounded ACPI/PCI/ADR contract')
    if not isinstance(path,bytes) or len(path)<30 or (not allow_trailing and len(path)>MAX_DISPLAY_BYTES):
        refuse()
    if path[:6]!=bytes.fromhex('02010c00d041') or path[6] not in (3,8) or path[7]!=0x0a:
        refuse()
    at=12
    limit=min(len(path),MAX_DISPLAY_BYTES)
    nodes=[]
    while limit-at>=4 and path[at:at+2]==b'\x01\x01':
        if len(nodes)==MAX_PCI_NODES or limit-at<6 or path[at+2:at+4]!=b'\x06\x00' or path[at+4]>7 or path[at+5]>31:
            refuse()
        nodes.append((path[at+5],path[at+4]));at+=6
    if not nodes or nodes[-1]!=(0,0) or limit-at<12 or (not allow_trailing and len(path)-at!=12) or \
            path[at:at+4]!=b'\x02\x03\x08\x00' or path[at+8:at+12]!=b'\x7f\xff\x04\x00':
        refuse()
    return {'controller_bytes':at,'path_bytes':at+12,'pci_nodes':nodes,
            'root_hid':int.from_bytes(path[4:8],'little'),'root_uid':int.from_bytes(path[8:12],'little')}


def same_gpu_display_route(left,right):
    a=decode_gpu_display_path(left);b=decode_gpu_display_path(right)
    return a['controller_bytes']==b['controller_bytes'] and \
        left[:a['controller_bytes']+4]==right[:b['controller_bytes']+4]
