# Collection-Layout Auditor

`.agents/scripts/Test-CollectionLayout.ps1` owns the mechanical sweeps over SOA
collection headers. It audits only: it never writes, generates, or repairs
header text. Never reconstruct these operations inline.

## Sweeps

- every declared SOA column appears exactly once in the effective `Members()`
  tuple, in both directions;
- `SharedMembers()`/`ClientMembers()` partition that tuple;
- `SharedCrcMembers()` and `PersistentMembers()` stay subsets;
- no `BT_CLIENT`-guarded column sits in `SharedMembers()`;
- no build's accessor tuple drops a column that build declares while the other
  build's tuple lists it, checked per build only where that build's tuple
  membership resolves to declared columns; and
- every collection `kiVersion` declaration and `Frame::kiVersion` term resolve
  to each other, in both directions.

## Invocation

From the session worktree root:

```powershell
pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1
```

Pass `-Path` to narrow the sweep to specific headers or directories,
`;`-separated for more than one.

## Result

It prints one `broken-engine-collection-layout/v1` JSON object with
`schemaVersion`, `status`, `code`, `message`, and `violations`
(`items`, `totalCount`, `omittedCount`, `truncated`); each item carries `path`,
`line`, `collection`, `member`, `rule`, and `text`.

`status` `pass` (exit 0) means no violations; `failed` (exit 1) reports
violations with path, line, collection, member, and rule; `blocked` (exit 2)
means an accessor shape, tuple entry, guard form, or `Frame::kiVersion` sum the
parser could not resolve, which is never a pass; `error` (exit 1) means the run
itself failed, such as a missing or empty `-Path`.

The report is capped at 32 items and 8192 bytes, so `truncated` and
`omittedCount` can hide violations: rerun until `totalCount` is 0, or account
for `totalCount` and `omittedCount` before recording the violations as
addressed. Any violation, blocked, or error outcome blocks completion until it
is fixed or explicitly recorded.

Judgment stays with the caller: tuple position and wire order, CRC membership,
diagnostic membership, and intentional client-only exclusion.
