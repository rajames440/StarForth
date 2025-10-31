# StarForth PR Workflow Guide

**For Developers:** How to contribute to StarForth through the PR workflow

---

## Quick Summary

```
Your Branch
    ↓ Create PR against devl
Your PR
    ↓ Review (optional) + Merge
devl Branch
    ↓ GitHub Actions + Jenkins auto-trigger
Pipeline: devL → Test → Qual
    ↓ (if all pass)
qual Branch
    ↓ PM approves release
prod Branch → master Branch (Canonical Source)
```

---

## Step-by-Step: Creating & Merging a PR

### 1. Create a Feature Branch

```bash
git checkout devl
git pull origin devl
git checkout -b feature/my-awesome-feature
```

**Branch naming conventions:**
- `feature/add-new-word` - New feature
- `fix/memory-leak-bug` - Bug fix
- `docs/update-readme` - Documentation
- `refactor/vm-simplification` - Code refactoring
- `test/add-edge-cases` - New tests
- `perf/optimize-interpreter` - Performance improvement

### 2. Make Your Changes

```bash
# Edit files
vim src/vm.c

# Run tests locally (optional, but recommended)
make test

# Commit with clear messages
git add src/vm.c
git commit -m "feat: Add new MYWORD to arithmetic_words module

This implements the MYWORD operation for...
- Details of what changed
- Why it matters
- Related issue (if any)
"
```

**Commit message guidelines:**
- Start with type: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `perf:`
- Keep first line under 50 characters
- Explain the "why", not the "what"
- Reference issues if applicable: `Closes #123`

### 3. Push to Remote

```bash
git push origin feature/my-awesome-feature
```

GitHub will detect your new branch and suggest creating a PR.

### 4. Create Pull Request on GitHub

1. Go to https://github.com/rajames440/StarForth
2. Click "Pull requests" tab
3. Click "New pull request"
4. **Base:** `devl` | **Compare:** `feature/my-awesome-feature`
5. Fill in PR details:

```markdown
## Description
Brief description of what this PR does.

## Type of Change
- [ ] New feature (non-breaking)
- [ ] Bug fix
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Refactoring

## Related Issue
Closes #123 (if applicable)

## Testing
- [ ] Ran `make test` locally - all passed
- [ ] Added new tests for this feature
- [ ] Verified existing tests still pass

## Checklist
- [ ] Code follows project style (ANSI C99)
- [ ] No compiler warnings (`-Wall -Werror`)
- [ ] Comments added for non-obvious logic
- [ ] CLAUDE.md updated if needed
```

6. Click "Create pull request"

### 5. Wait for Automated Checks

GitHub will run:
- ✅ Status checks (Jenkins must pass - not yet, requires manual config)
- ✅ Code review (optional - you can self-review)

**What happens if tests fail:**
- Red ✗ badge shows on PR
- Check "Checks" tab to see Jenkins logs
- Fix the issue locally
- Push again: `git push origin feature/my-awesome-feature`
- PR auto-updates

### 6. Review (Optional)

**You can self-review your own PR:**

1. Click "Files changed" tab
2. Review your own code
3. Add comments if needed
4. Click "Approve" (optional)

**Document important decisions:**
- Why you made this change
- Any trade-offs considered
- Performance impacts
- Security considerations

### 7. Merge the PR

1. Click "Merge pull request" button
2. GitHub shows: "✅ All checks passed"
3. Choose merge type:
   - **Create a merge commit** (default, recommended)
   - Squash and merge (collapses all commits)
   - Rebase and merge (linear history)
4. Click "Confirm merge"

---

## What Happens After Merge

### Automatic Pipeline Execution

When you merge a PR to `devl`:

**Immediately:**
1. ✅ `pr-trigger-devl.yml` posts comment with timeline
2. ✅ Jenkins detects `devl` push via webhook

**~5 minutes (devL stage):**
3. Jenkins runs: Build + Smoke test
4. If PASS → auto-merge `devl` → `test`
5. If FAIL → pipeline stops, you're notified

**~20 minutes (test stage):**
6. Jenkins runs: Full test suite + benchmarks
7. If PASS → auto-merge `test` → `qual`
8. If FAIL → pipeline stops, you're notified

**~25 minutes (qual stage):**
9. Jenkins runs: Formal verification + CAPAs
10. If PASS → qual branch ready for release
11. If FAIL → pipeline stops, you're notified

**Total: ~50 minutes** if all stages pass

---

## Monitoring Your PR

### In GitHub

1. Go to your PR
2. Scroll to bottom to see comments:
   - Phase 1 comment: "✅ DevL Pipeline Starting"
   - Periodic Phase 2 comments: Pipeline progression
3. Click "Checks" tab to see Jenkins job status
4. Click Jenkins job link to view full logs

### In Jenkins (Optional)

If you want to watch the pipeline in real-time:
1. Go to https://your-jenkins-url/job/starforth-devl/
2. Find the build that matches your PR's commit
3. Click build number to see real-time logs
4. Refresh page to see updates

### Notification Options

GitHub can email you when:
- PR is merged
- Status checks pass/fail
- PR has comments

Configure in: Settings → Notifications

---

## If Pipeline Fails

### Debugging Steps

1. **Find which stage failed:**
   - Check PR comments for pipeline progress
   - Look at "Checks" tab to see which Jenkins job failed

2. **View Jenkins logs:**
   - Click the failing Jenkins job link
   - Scroll to "Console Output"
   - Find the error message
   - Note the line number or error details

3. **Fix locally:**
   ```bash
   # Your branch is still available
   git checkout feature/my-awesome-feature

   # Make the fix based on Jenkins error
   vim src/file.c

   # Test locally
   make test

   # Commit and push
   git add src/file.c
   git commit -m "fix: Address Jenkins test failure"
   git push origin feature/my-awesome-feature
   ```

4. **Pipeline auto-retriggers:**
   - Your PR now shows new checks
   - Jenkins auto-starts (via webhook)
   - Same 50-minute process repeats

### Common Failure Reasons

| Error | Likely Cause | Fix |
|-------|--------------|-----|
| Compilation error | Syntax error in C code | Check compiler output, fix syntax |
| Test failure | Logic error or missing test case | Run `make test` locally, debug |
| Compiler warnings | `-Wall -Werror` rejects warnings | Fix all warnings (no logging errors ignored) |
| Memory error | Stack overflow or memory leak | Run with Valgrind: `valgrind ./build/starforth` |
| Benchmark regression | Performance degraded | Check optimization flags, profile with `make pgo` |

---

## Release Process (For PM)

**You (as developer) don't do this, but it's helpful to understand:**

1. **Once qual passes:** PM is notified
2. **PM approves release:**
   - Go to Actions → "PM Release Approval Gate"
   - Input release version (e.g., `2.0.1`)
   - Click "Run workflow"
3. **Jenkins prod job runs:**
   - Versions artifacts
   - Creates git tag: `v2.0.1`
   - Merges prod → master
4. **Result:**
   - master branch is updated with release
   - Release artifacts are available
   - Developers can check out master to get latest code

---

## Best Practices

### 1. Keep PRs Focused
- One feature or fix per PR
- Easier to review
- Easier to revert if needed
- Faster to merge

### 2. Test Locally Before Pushing
```bash
make clean
make all
make test
```

### 3. Commit Often, Push Often
- Don't wait until feature is "done"
- Small commits are easier to understand
- CI can catch issues early

### 4. Write Meaningful Commit Messages
Bad:
```
fix bug
update code
stuff
```

Good:
```
fix: Prevent stack underflow in DROP word

The DROP word was not checking if the data stack
had at least one element before popping. This could
cause a segfault if DROP was called on empty stack.

Added bounds check: return error if stack_ptr < 1
```

### 5. Link to Issues (if applicable)
In PR description:
```
Closes #42  (auto-closes issue when PR merges)
Fixes #42   (alternative)
Resolves #42 (alternative)
```

### 6. Use Kanban Board
- Create issue for feature (if not already created)
- Add issue to project
- PR will show progress automatically
- Easy to track what's in progress

### 7. Document Your Changes
- Update `CLAUDE.md` if you change build system
- Update `README.md` if you add user-facing features
- Add comments in code for non-obvious logic
- Update tests when you change functionality

---

## FAQ

### Q: Do I have to wait 50 minutes after every merge?
**A:** Yes, but the pipeline runs automatically. You don't have to watch it. Just merge, close your laptop, and check back later. The PR comments will keep you updated.

### Q: What if I need to add more commits while waiting?
**A:** Great! Just push new commits to your feature branch. The pipeline will auto-restart with all new commits.

### Q: Can I merge directly to test/qual/prod?
**A:** No - those branches are protected. All changes must come through devl first, via the pipeline.

### Q: What if the pipeline is taking too long?
**A:** Check Jenkins logs to see if it's actually running. Sometimes webhooks fail silently. You can manually trigger Jenkins if needed (contact maintainer).

### Q: Can I force-push after merge?
**A:** No - devl branch is protected against force-pushes. This prevents accidentally rewriting history. If you need to change something, create a new PR.

### Q: What if I disagree with a pipeline failure?
**A:** The pipeline failures are real (Jenkins doesn't lie). But you can:
1. Debug locally
2. Verify the failure with `make test`
3. Create an issue if it's a bug in the test suite
4. Fix your code and push again

### Q: Do I need to review governance documents?
**A:** Not for development. But if you're documenting system decisions, those docs should eventually be copied to the read-only governance repo.

### Q: Who approves the final release?
**A:** The PM (you in this case). The "PM Release Approval Gate" workflow requires manual dispatch - only a maintainer can trigger it.

---

## Quick Reference

| Task | Command |
|------|---------|
| Create feature branch | `git checkout -b feature/name && git push origin feature/name` |
| Push changes | `git add . && git commit -m "message" && git push` |
| Test locally | `make clean && make all && make test` |
| View build options | `make help` or `cat Makefile` |
| Run single test module | Edit test in `src/test_runner/modules/` and run `make test` |
| Check git log | `git log --oneline -10` |
| Force pull latest | `git fetch origin && git reset --hard origin/devl` |

---

## Getting Help

1. **Build fails locally?**
   - Check `CLAUDE.md` Build section
   - Try: `make clean && make all`
   - Check compiler flags match your system

2. **Tests fail?**
   - Read test failure output carefully
   - Check `CLAUDE.md` Testing section
   - Run tests with verbosity: `make test` (already shows details)

3. **CI/CD pipeline unclear?**
   - Read `GITHUB_ACTIONS_SETUP.md` (internal)
   - Check `BRANCH_PROTECTION_GUIDE.md` (why rules exist)
   - This document (how to use it)

4. **Jenkins configuration?**
   - Contact maintainer (you)
   - Jenkins not webhook-triggering? Check GitHub repo webhooks

5. **Governance question?**
   - Check `StarForth-Governance/` submodule
   - File issue on GitHub if unclear

---

## Summary

1. ✅ Create feature branch from `devl`
2. ✅ Make changes, commit, push
3. ✅ Create PR against `devl`
4. ✅ Optional: Self-review
5. ✅ Merge when ready
6. ✅ Watch pipeline auto-run (50 minutes)
7. ✅ Pipeline auto-merges through stages
8. ✅ PM approves release (manual)
9. ✅ Code ends up in master (canonical source)

---

**Happy coding! The pipeline handles the rest.** 🚀

---

**Last Updated:** October 30, 2025
**Audience:** StarForth Developers
**See Also:** `GITHUB_ACTIONS_SETUP.md`, `BRANCH_PROTECTION_GUIDE.md`, `CLAUDE.md`