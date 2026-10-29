"""Host-only lifecycle design model. No EFI adapter or runtime authority."""
from dataclasses import dataclass, replace


@dataclass(frozen=True)
class Snapshot:
    generation: int
    config: tuple
    attributes: int
    supported: int
    bdf: tuple
    edges: frozenset
    keyboard: str
    boot_image: str
    owner_fingerprint: tuple
    bar_descriptor: tuple


# Abstract fixture relationships, deliberately not an OEM graph/profile.
STAGES = (
    frozenset({'owner', 'gop-child', 'text', 'graphics'}),
    frozenset({'owner', 'gop-child', 'graphics'}),
    frozenset({'owner', 'gop-child'}),
    frozenset(),
)
OWNER = (0x24, 18432, 0x7c4,
         '6978c561cfc119be29c3832d47fc51e471b02e86dc84b3a4eac6524a65de502c')


def fixture():
    """Synthetic configuration, never substituted for a real capture."""
    config = [0]*64
    config[0], config[1], config[2], config[4], config[11] = (
        0x278210de, 0x00100007, 0x030000a1, 0xfb000000, 0x51321462)
    return Snapshot(0, tuple(config), 0x718, 0x1b77b, (0, 11, 0, 0),
                    STAGES[0], 'synthetic-keyboard', 'synthetic-boot-hash', OWNER,
                    (32, 0xfb000000, 0xfbffffff, 0x1000000, 0, 0))


def predicted_snapshot(before, stage):
    """Generate synthetic expectations only; not a hardware postcondition."""
    config = list(before.config)
    attributes = before.attributes
    if stage == 3:
        config[1] &= ~7
        attributes = 0
    elif stage == 4:
        config[1] = (config[1] & ~7) | 3
        attributes = 0x300
    return replace(before, generation=stage, config=tuple(config),
                   attributes=attributes, edges=STAGES[min(stage, 3)])


def initial_ok(s):
    return (len(s.config) == 64 and s.config[0] == 0x278210de and
            s.config[11] == 0x51321462 and s.config[2] == 0x030000a1 and
            s.config[1] & 0xffff == 7 and s.config[4] == 0xfb000000 and
            s.attributes == 0x718 and s.supported == 0x1b77b and
            s.bdf == (0, 11, 0, 0) and s.edges == STAGES[0] and
            bool(s.keyboard) and bool(s.boot_image) and s.owner_fingerprint == OWNER and
            s.bar_descriptor == (32, 0xfb000000, 0xfbffffff, 0x1000000, 0, 0))


def run_simulation(ops):
    """Invoke only caller-supplied host mocks; there is no hardware adapter.

    A real release requires a separate implementation/review. No result here
    permits Windows or unlock, even when the predicted sequence matches.
    """
    result = dict(simulation_only=True, runtime_authorized=False,
                  windows_authorized=False, unlock_authorized=False,
                  mutation_attempted=False, actions=[], outcome='preflight-refused')
    try:
        before = ops.capture()
    except Exception:
        result['outcome'] = 'initial-capture-exception-no-attempt'
        return result
    if not initial_ok(before):
        return result
    previous = before
    for stage, action in enumerate(('detach-text', 'detach-graphics', 'detach-owner',
                                    'enable-original-io-mem-only'), 1):
        result['mutation_attempted'] = True
        result['actions'].append(action)
        # Mark the attempt before the callback: errors may follow partial change.
        try:
            success = ops.attempt(action, 0x300 if stage == 4 else 0)
        except Exception:
            result['outcome'] = 'callback-exception-state-unknown'
            return result
        if not success:
            result['outcome'] = 'callback-failed-state-unknown'
            return result
        try:
            now = ops.capture()
        except Exception:
            result['outcome'] = 'post-attempt-capture-exception-state-unknown'
            return result
        expected = predicted_snapshot(before, stage)
        if (now.generation <= previous.generation or now.config != expected.config or
            now.attributes != expected.attributes or now.supported != before.supported or
            now.bdf != before.bdf or now.edges != expected.edges or
            now.keyboard != before.keyboard or now.boot_image != before.boot_image or
            now.owner_fingerprint != before.owner_fingerprint or now.bar_descriptor != before.bar_descriptor):
            result['outcome'] = 'postcondition-refused-no-retry'
            return result
        previous = now
    result['outcome'] = 'synthetic-decode-transition-matched-runtime-evidence-missing'
    return result
