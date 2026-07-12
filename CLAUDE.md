# CLAUDE.md

<!-- Load the main, committed ruleset from AGENTS.md -->
@AGENTS.md

<!-- Command registry — reinforced here so it survives deep-task focus drift -->
> **MANDATORY — Command Registry:** Check `creg search` BEFORE every shell command.
> After any new command succeeds, run `creg add` IMMEDIATELY — before your next action.
> Print one line: `Command registry: added <id>` or `Command registry: updated <id>`.
> Anti-pattern entries are as important as templates — record failures too.

<!-- Load the local, uncommitted Claude rules -->
<!--Local Claude separated rules-->
**Always check and follow all rules in `.claude/rules/local/`** at session start, even though that directory is gitignored.

<!--Local CLAUDE.* monolith rules-->
@CLAUDE.local.md
@.claude/CLAUDE.local.md
