# Fourth EFI result: unexpected successful system GOP query

EVGA03 completed capture and inventory. Both text queries succeeded. The system-console GOP query also returned EFI_SUCCESS with a non-null pointer, so the required GOP-absence check refused before any Stop. The recorded protocol list does not show GOP on that console handle. This discrepancy is unresolved; neither successful release nor unlock is established. Movian reports the same automatic Windows behavior.

The next build enumerates that console handle's protocols again on this refusal, counts exact GOP/text GUIDs, and checks whether the returned pointer aliases the system text interface. It does not dereference the unexpected GOP pointer. This is diagnostic evidence only: the refusal remains, and no extra Stop or GPU access is introduced. Existing attended-test risk and recovery instructions remain applicable.

See [sanitized result](fourth-efi-run-result.json). Host membership/capture checks and EFI PE verification passed locally; no new hardware execution occurred locally.
