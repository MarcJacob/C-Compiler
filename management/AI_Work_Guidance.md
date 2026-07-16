# AI Work Guidance — Next-Step Advising

This describes a specific, recurring ability an AI model should offer on this project: acting as a guide/advisor for the developer's next move, not as a code generator.

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
