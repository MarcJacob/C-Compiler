# AI Work Guidance — Next-Step Advising

This describes recurring abilities an AI model should offer on this project: acting as a guide/advisor for the developer's next move, and helping keep planning files up to date.

## When to use this

The developer asks something like "what should I work on next", "what's the next step", or "I have N hours, what should I focus on" — a request for direction, not for implementation.

## What the response should draw on, by default

- The overall plan in `management/dev-plan.md` (or whatever plan file is current) — to identify the next unstarted or partially-done step.
- The current actual state of the repo (read the code, don't assume) — to check what's already done versus what the plan says.
- The relevant `imprint.md` files, applying the usual priority rule (lower folder = higher priority).
- Any parameters the developer gives in the request: time budget, mood/energy, a specific area of interest, etc. These should shrink or redirect the scope of the suggested next step — e.g. a 1-hour budget means recommending a sub-slice of a plan step, not the whole step.

## What the response should look like

- Identify the natural next step or sub-step.
- Give a short set of best-practice recommendations relevant to that step (language idioms, this-kind-of-project conventions, scope-control advice), calibrated to the time/constraints given.
- Stay advisory: do not write production code as part of this, and do not silently expand scope beyond what was asked. Offer to help scope or document further, but only act on confirmation.

## Work Status Passes

A second ability: keeping planning files (primarily `management/dev-plan.md`, or whatever plan file is current) marked up to date with what's actually done, so the "next step" advice above stays accurate.

Marking convention: a plan item that is done gets `DONE` appended neatly at the end of its line (aligned with existing spacing conventions in the file, no emojis, no other decoration). Partially-done items are left unmarked, optionally with a short one-line note if it materially helps the next read of the plan.

There are two passes, differing in scope:

### Complete work status pass

Triggered by a request like "do a full status pass" or "go through the whole plan and update it."

- Read the entire plan file, top to bottom.
- Read the entire actual project state (source, headers, tests, management notes — don't assume from memory or from prior conversation).
- For every item in the plan, determine whether it is actually done, and mark accordingly (adding `DONE` where missing, removing it where a previously-marked item has since regressed or been reverted).
- Report a short summary of what changed after editing.

### Current work status pass

Triggered by a request like "update the plan for what I just did" or run implicitly right after finishing a scoped task together in conversation.

- Scope to whatever plan item(s) the developer is currently or was just working on — not the whole file.
- Check the current state of just that area of the project, and update only the relevant line(s).
- Report the specific line(s) changed.

Both passes are edits the developer should be able to review at a glance — keep diffs small and targeted, matching the plan file's existing formatting. When it is evident many things have changed, go over every relevant plan item one by one.
