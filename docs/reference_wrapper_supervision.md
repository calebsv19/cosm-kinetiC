# Plain reference wrapper lifecycle migration

Fifty-five plain reference wrappers now execute their command using the anchored
CFD supervisor frozen inside each source packet. Adding its digest to SOURCES
creates a new source identity rather than altering any historical run/receipt.
The exact family list is config/reference_supervision_families.json. The prior
inventory found 194 wrappers with the old loop: 139 factor-building wrappers
remain outside this migration and require their separate compiler/receipt audit.

Solver argument expressions, environment expressions and RSS/wall cap expressions
were compared with the pre-edit source AST for every migrated family and are
unchanged. Source snapshots and the exact comparison report are retained in the
validation packet. Numerical gate logic remains in each wrapper. No production
numerical campaign or physical accuracy gate was executed by this migration.

Combined stdout/stderr stays chronological in the existing case.log file. The
shared helper supports explicit cwd/environment and create-only combined-log
publication; combined descriptor bytes are counted once for its sampled cap.
Direct result status is preserved for numerical/nonzero exits. Resource-cap
failures and supervision holds are distinct in receipts; missing command results
remain unknown rather than fabricated. Limited terminal/reaping scope and false
complete-descendant verification propagate into each reference receipt.

Existing source/cache admission assertions now use explicit require calls, so
python -O cannot remove them. Case names are checked at the public run function
before allocation. New receipts are create-only. A discovered cached-acceptance
bug is fixed: a zero-exit receipt with rejected diagnostic acceptance remains
rejected on reread instead of becoming successful merely because the process
returned zero. The consistency wrapper also requires a clean stop reason on
cached reads. Historical files were neither rewritten nor adopted as disposable.

Four wrapper control tests passed through actual control-only Make. They execute
110 fresh synthetic solver cases across all 55 families (success and nonzero),
then cached readbacks, and verify frozen helper identity, combined diagnostics and
unchanged receipts. They also exercise invalid names across every family,
tampered cached command admission under -O, and zero-exit rejected-diagnostic
cache behavior. Eight CFD supervision and five retained-compiler tests also
passed, including actual Clang. These are 17 distinct test methods, not a
repository-wide acceptance or numerical proof. Initial failed fixture/import
runs are retained alongside the final passing logs.

Root/record JSON admission, complete writer/descendant ownership, factor-library
transactions, aggregate/hard quotas and archive-backed retirement remain open.
Most legacy reference outputs still live under historical build subtrees; the
Main Edit clean inventory holds unknown/retained content, but canonical still
requires separately reviewed adoption. Complete descendant termination remains
unverified. No commit, installation, release, remote transfer or pruning occurred.
This new validation packet is outside the frozen prepared backup cutoff.
