# Carbon parity workflow

## Repository facts to preserve

The supplied source checkout is the sibling `JavaMinecraft/decompiled` tree. Its `version.json` currently declares Java Edition `26.3`, protocol `777`, and world version `5023`; the sibling `26.3.jar` is a separate bytecode artifact. Hash the exact sources compiled or copied into a harness, plus the jar and relevant class entries when available. The jar hash alone does not prove that a decompiled method matches the binary. Use source methods when testing source-level ports, but check the actual binary for the affected class when precise behavior diverges or source decompilation may have altered expressions. Prefer the binary's observed behavior for a claim about the shipped Java 26.3 implementation and document both artifacts and the discrepancy.

`Carbon/PORT_STATUS.md` describes a native core library and a limited playable client slice. It also records specific unimplemented areas and explicitly distinguishes native assertions, a source-derived scheduled-tick model, and hidden-window CPU timings from full Java parity, real Minecraft class execution, and visible FPS. Update status notes only with the exact scope and evidence established by the work.

`tests/run_all.py` builds Carbon plus registered component drivers and runs the RNG comparison and `*_parity.py` scripts. It invokes all `build_*.cmd` scripts sequentially because they share the build directory. Inspect the affected build script and tests before running the full entry point; some renderer tests open WGL windows and produce captures. Use the project’s documented headless diagnostics path for noninteractive checks. Do not launch the interactive game as a test harness unless the user requested interactive validation.

When a relevant vanilla class is self-contained in the supplied jar, a small isolated Java driver can use `javac -cp "<versioned-jar>"` and `java -cp "<temp-classes>;<versioned-jar>" Driver` to call it directly; this need not bootstrap the full client. If the class needs more classes, add only unmodified classes from the same jar and document any dependency substitutions. `javap -classpath "<versioned-jar>" -c -p <fully.qualified.Class>` can help inspect bytecode but is not runtime differential evidence. Record JDK/JRE versions, jar and class-entry hashes, driver sources, and output comparator. Keep the driver in a temp/build folder, not in the game archive.

Some existing parity scripts hardcode a machine-specific JDK path as a fallback; prefer PATH discovery or explicit configuration in new reusable tooling. Build scripts may also use fixed Visual Studio paths. Preserve the user's active executables and unsaved work: use isolated output paths when practical, check for executable locks before rebuild, and build an alternate named executable if the default is in use. Never kill a user's game or editor to free a file.

## Evidence inventory

Run `scripts/parity_tools.py inventory --workspace <Carbon-root>` for the current declarative test inventory and hashes. The classifications in `evidence_inventory.json` are reviewed descriptions, not machine-verified proofs. Check every harness's actual compilation list, source extraction, oracle, dependencies, and output comparison before relying on a classification. Correct the manifest if repository tests change.

At minimum, report which evidence tier a result uses:

- **Unmodified supplied classes:** exact upstream Java source files are compiled unchanged; document all stubs and confirm they only replace unrelated dependencies.
- **Extracted methods:** tested Java methods are copied mechanically from supplied source; preserve extraction provenance and verify their text/source hashes.
- **Source-adapted oracle:** the source implementation is transformed to run in a JVM; record every adaptation and dependency substitution.
- **Handwritten source-derived model:** Java code re-expresses behavior from source. This checks a second implementation, not the Minecraft classes themselves.
- **Native assertions:** C++ tests exercise source-inspired expectations. Useful regression/edge checks, but not differential evidence.
- **Visual or performance check:** names the compared rendering stages and measured quantity; it does not silently imply simulation or general parity.

If an apparent mismatch comes from a handwritten/adapted oracle, validate the expected result from independent supplied Java methods/classes before editing Carbon. If dependency-complete Java execution is impractical, keep the uncertainty explicit and create a stronger oracle as scope allows. Do not trust filenames or a green test status as evidence-tier metadata.

## Fix and coverage loop

1. Pin version and provenance, inspect current implementation and tests, and define a bounded behavior slice.
2. Write or select deterministic cases that distinguish competing semantics. Include boundary values, negative/extreme values, ordering, and round trips where relevant. Make random seeds and action traces explicit.
3. Compare the same cases with a vanilla oracle. Emit stable case IDs and serialize all relevant outputs. For ordered stateful behavior, preserve action and output order.
4. Minimize the first mismatch. Check IEEE-754 bit patterns, signed zero and NaN policy when relevant; Java overflow, UTF-16 indexing/hashing, and Java collection identity/order where relevant. Do not normalize away a behavior difference without a contract-based reason.
5. Fix Carbon only after establishing the independent expected behavior. Add the minimized case as a regression, retaining source provenance and the declared comparison rule.
6. Re-run the narrow harness and suitable broader checks. Record untested dependencies, unsupported behavior, and remaining gaps. A feature gap is not permission to implement an approximate whole Minecraft subsystem.

## Verified bug repair and pull request

When the user explicitly requests the verified-fix PR workflow, that request authorizes the scoped feature-branch commit, push, and PR creation described below; do not ask for a second confirmation. Honor earlier standing authorization in the current session as well. A request to audit, review, or reproduce only authorizes that investigation; report any verified bug without editing Carbon. A fix request without a PR request authorizes scoped local implementation and regression work, but not publication: finish validation and present the isolated branch diff before asking to commit, push, or open a PR.

1. Resolve the actual Carbon repository from the workspace's Git metadata (`git rev-parse`, `git remote -v`, and branch/status), then confirm the selected GitHub repository and intended base branch agree with that checkout. The canonical upstream is `https://github.com/Carbon-minecraft/Carbon`; treat it as an expected identity to verify, and follow an explicitly selected user fork/remote when applicable. For this Carbon project, target `alpha-test` by default: verify that branch exists on the selected remote and is the intended base instead of assuming GitHub's default branch. Honor an explicit user-selected base branch after verifying it exists. Do not assume the current directory name, a sibling folder, or an installed skill path identifies the repository. Inspect the target repository's contribution instructions and existing branches/PRs; avoid duplicate work. If repository identity, base, or write capability cannot be established, stop before committing or publishing and report the specific blocker.
2. Preserve user changes in the current checkout. Create a dedicated Git worktree or other isolated checkout from the confirmed base and a task-specific feature branch; do not repair on or push to the default/base branch. Keep the fix and regression limited to the verified behavior and necessary files.
3. Reproduce the minimized mismatch in the isolated checkout against the authoritative vanilla oracle, implement the Carbon fix, add the minimized regression, and run the affected harness plus suitable broader checks. Re-check evidence tier and provenance; a source model, adapter, or stale manifest entry does not establish a vanilla bug by itself. Keep the existing regression for the proven RNG decompiler discrepancy intact, and do not silently refresh pinned harness/source hashes when tests change; review any changed evidence labels explicitly.
4. Review the complete diff and staged file list before committing. Include only Carbon implementation, regression, and directly relevant project documentation. Treat the supplied JavaMinecraft checkout, decompiled source, versioned jar, assets, credentials/auth files, generated classes, captures, and build products as local reference/input material: read them in place and test against them, but never copy or commit them into the Carbon PR. Do not copy raw vendor code into the patch. Remove temporary outputs from the proposed diff and verify no secrets or unrelated user changes are staged.
5. Make a focused commit on the feature branch, push only that branch, and open one PR from that feature branch to `alpha-test` (or the verified explicit user-selected base) using the connected GitHub integration when it supports repository/PR creation. Discover the available connector and inspect its schema first; if it cannot create the PR, use an already authenticated GitHub CLI or existing Git remote credentials when available. Never print, request, or place tokens or credentials in command output, files, or the PR. Do not switch to a different account/repository or bypass a missing authorization; stop and report the blocker.
6. The PR description should state the verified behavior and scope, vanilla source/jar provenance and evidence tier, minimized reproduction/regression, commands and results for validation, and remaining coverage limits. Link the reproduction or relevant existing PR/issue when available. Confirm the PR URL, branch, base, commit, and CI state, and report these with the local worktree location. Do not merge the PR or push commits to the base branch.

Only create a PR for a mismatch independently confirmed as a Carbon defect and within the user's requested repair scope. For uncertain oracle behavior, a reference/decompiler discrepancy, an incomplete repro, a stale harness that has not been reviewed, or a mismatch caused by the test setup, improve or report the evidence first; do not publish a speculative fix.

## Domain-specific comparison

For pixels, lock resolution, GUI scale, camera and projection, GL backend, color space/format, capture point, and ROI. Separate geometry/texture regressions from lighting, blending, and driver variance; define a justified per-channel or perceptual tolerance and preserve raw captures with metadata.

For physics, compare a fixed 20 TPS tick schedule with identical initial state and ordered inputs. Capture position, velocity, collision/ground state, and relevant block shapes each tick. Do not infer whole-game parity from a movement demo.

For terrain, use the exact seed and generation settings and compare serialized chunks across the required density, aquifer, surface, and later stages. Noise samples or a flat preset alone do not establish generated-world parity.

For performance, identify CPU timing boundaries, GPU completion or lack thereof, present mode, scene, warm-up, hardware, and whether a visible game window was used. Report CPU render-through-swap, GPU elapsed time, and visible FPS as different measures.
