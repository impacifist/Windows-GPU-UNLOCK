"""Symbolic console dependency model. Never calls firmware or grants admission."""
from dataclasses import dataclass


@dataclass(frozen=True)
class Open:
    source: str
    protocol: str
    agent: str
    controller: str
    attributes: int
    count: int = 1


EXPECTED = frozenset({
    Open('physical', 'gop', 'graphics_splitter', 'physical', 16),
    Open('physical', 'gop', 'graphics_splitter', 'virtual', 8),
    Open('virtual', 'gop', 'virtual_text', 'virtual', 16),
    Open('virtual', 'text', 'text_splitter', 'virtual', 16),
    Open('virtual', 'text', 'text_splitter', 'system_console', 8),
})
ORDER = ('text_splitter', 'virtual_text', 'graphics_splitter')


def replay(edges, order=ORDER, *, callbacks_succeeded=True, observed_after=None):
    """Require complete modeled edge removal, not merely a success callback.

    This models the selected GOP/text subgraph only. EDID, NVIDIA parent,
    indirect helper effects and console object lifetimes remain unqualified.
    observed_after can supply synthetic post-call edge snapshots for fault tests.
    No result from this function is hardware admission.
    """
    edges = list(edges)
    result = {'model_passed': False, 'write_eligible': False, 'unlock_proven': False,
              'hardware_accessed': False, 'remaining_qualification': [
                  'EDID consumer', 'NVIDIA parent', 'helper effects', 'console object lifetimes']}
    if len(edges) != len(EXPECTED) or frozenset(edges) != EXPECTED:
        return {**result, 'refusal': 'unexpected, missing or duplicate modeled ownership'}
    if tuple(order) != ORDER:
        return {**result, 'refusal': 'dependency order differs from the modeled order'}
    if not callbacks_succeeded:
        return {**result, 'refusal': 'callback failure'}
    if observed_after is not None and len(observed_after) != len(order):
        return {**result, 'refusal': 'incomplete post-call snapshots'}
    current = frozenset(edges)
    for index, agent in enumerate(order):
        expected = frozenset(e for e in current if e.agent != agent)
        snapshot = None if observed_after is None else list(observed_after[index])
        if snapshot is not None and len(snapshot) != len(set(snapshot)):
            return {**result, 'refusal': 'duplicate post-call ownership'}
        actual = expected if snapshot is None else frozenset(snapshot)
        if actual != expected:
            return {**result, 'refusal': 'post-call graph differs; callback success is insufficient'}
        current = actual
    return {**result, 'model_passed': not current, 'refusal': None,
            'interpretation': 'Selected edge-removal assumptions passed in a symbolic model only.'}
