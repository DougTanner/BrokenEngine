# Refactor Candidates

Design shapes in the code that a refactor would improve, kept for a later session to review in batch. At the Change Workflow Verify and land step, main copies these rows here word for word, dropping only session-local IDs — a row's leading `/over-engineering-review` `<ID>`, and a `makes unnecessary:` or `candidate:` tail that names only candidate IDs:

- `/over-engineering-review` `Design suggestions` rows the user did not accept.
- `size observation` and `refactor observation` rows from the `/repo-code-review` and `/glsl-review` `Residuals`, skipping a size observation whose last field is `no qualifying reduction`.
- At most one row from main's own context, on the `refactor observation` form in `.agents/skills/repo-code-review/SKILL.md` `## Handoff`.

Prefix each row with `<YYYY-MM-DD> <session branch> — `. Add new rows directly under `## Entries`, so the newest rows are at the top and main reads only the top of the file. A session reviewing the log removes each row it handles.

## Entries
