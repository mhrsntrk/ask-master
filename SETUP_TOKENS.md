# GitHub Token Setup Guide

This document explains how to set up the required GitHub Personal Access Tokens (PATs) for the automated workflows in this repository.

## Overview

The `ask-master` repository has two automated workflows that require GitHub tokens:

1. **Skill Sync Workflow** — Syncs `skill/SKILL.md` to the `ask-master-skill` repository
2. **GoReleaser Workflow** — Builds binaries and publishes to Homebrew tap when you push a tag

Both workflows need separate PATs with `repo` scope to push to their respective repositories.

---

## Token 1: SKILL_REPO_PAT (Skill Sync)

### What it does

When you push changes to `skill/SKILL.md` on the `master` branch, a GitHub Action automatically copies that file to the `mhrsntrk/ask-master-skill` repository.

### Step-by-step setup

#### Step 1: Generate the PAT

1. Go to https://github.com/settings/tokens
2. Click **"Generate new token (classic)"** (not fine-grained)
3. Give it a descriptive name, e.g., `ask-master-skill-sync`
4. Set expiration (recommend: 90 days or 1 year)
5. Under **scopes**, check:
   - ✅ `repo` — Full control of private repositories
     - This automatically grants `repo:status`, `repo_deployment`, `public_repo`, `repo:invite`, `security_events`
6. Scroll down and click **"Generate token"**
7. **COPY THE TOKEN IMMEDIATELY** — you won't see it again

#### Step 2: Add it as a repository secret

1. Go to https://github.com/mhrsntrk/ask-master/settings/secrets/actions
2. Click **"New repository secret"**
3. Fill in:
   - **Name**: `SKILL_REPO_PAT`
   - **Value**: paste the token you just copied
4. Click **"Add secret"**

#### Step 3: Test it

Make a small edit to `skill/SKILL.md` and push it to `master`:

```bash
git add skill/SKILL.md
git commit -m "test: verify skill sync"
git push origin master
```

Then check the Actions tab: https://github.com/mhrsntrk/ask-master/actions

You should see the "Sync Skill to Skill Repo" workflow run. If it succeeds, check https://github.com/mhrsntrk/ask-master-skill to confirm the `SKILL.md` was updated.

---

## Token 2: HOMEBREW_GITHUB_API_TOKEN (GoReleaser / Homebrew)

### What it does

When you push a Git tag (e.g., `v1.0.0`), GoReleaser builds binaries for all platforms and publishes a Homebrew formula to the `mhrsntrk/homebrew-ask-master` repository. This allows users to install with:

```bash
brew install mhrsntrk/ask-master/ask-master
```

### Step-by-step setup

#### Step 1: Generate the PAT

1. Go to https://github.com/settings/tokens
2. Click **"Generate new token (classic)"**
3. Give it a descriptive name, e.g., `ask-master-homebrew-release`
4. Set expiration (recommend: 90 days or 1 year)
5. Under **scopes**, check:
   - ✅ `repo` — Full control of private repositories
6. Scroll down and click **"Generate token"**
7. **COPY THE TOKEN IMMEDIATELY**

#### Step 2: Add it as a repository secret

1. Go to https://github.com/mhrsntrk/ask-master/settings/secrets/actions
2. Click **"New repository secret"**
3. Fill in:
   - **Name**: `HOMEBREW_GITHUB_API_TOKEN`
   - **Value**: paste the token you just copied
4. Click **"Add secret"**

#### Step 3: Install GoReleaser (local)

```bash
brew install goreleaser/tap/goreleaser
```

Or download from: https://goreleaser.com/install/

#### Step 4: Test with a dry-run

First, test locally without publishing:

```bash
cd /path/to/ask-master
goreleaser release --snapshot --clean
```

This builds everything locally without pushing. Check `dist/` directory for the artifacts.

#### Step 5: Publish your first release

1. Make sure you have committed everything you want in the release
2. Create and push a tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

3. Go to https://github.com/mhrsntrk/ask-master/actions and watch the release workflow run
4. After it completes, check:
   - The release page: https://github.com/mhrsntrk/ask-master/releases
   - The Homebrew tap: https://github.com/mhrsntrk/homebrew-ask-master (look in `Formula/`)

#### Step 6: Verify the Homebrew tap works

```bash
brew update
brew install mhrsntrk/ask-master/ask-master
ask-master --version
```

---

## Troubleshooting

### "Resource not accessible by personal access token"

The PAT likely expired or doesn't have `repo` scope. Generate a new one.

### "Could not resolve to a Repository"

Make sure the repository names in the workflow files match exactly:
- For skill sync: `mhrsntrk/ask-master-skill`
- For homebrew: `mhrsntrk/homebrew-ask-master`

### Workflow runs but skill repo doesn't update

Check the Action logs. Common issues:
- The PAT doesn't have write access to the skill repo
- The branch name is `master` not `main` (the workflow only triggers on `master` or `main` pushes to `skill/SKILL.md`)

### GoReleaser fails with "already exists"

If you re-run a release for an existing tag, GoReleaser might fail because the GitHub Release already has assets. Either delete the release and re-tag, or increment the version.

### Homebrew formula doesn't appear

Check that:
1. The `HOMEBREW_GITHUB_API_TOKEN` secret is set correctly
2. The `homebrew-ask-master` repo exists and is public
3. The GoReleaser workflow completed successfully

---

## Security Notes

- **Never commit tokens to git** — always use repository secrets
- **Use classic PATs** — the workflows expect `repo` scope which is simpler with classic tokens
- **Rotate tokens regularly** — set expiration dates and regenerate before they expire
- **Scope the tokens** — these tokens only need `repo` access, don't grant unnecessary scopes
- **Monitor usage** — check your token activity at https://github.com/settings/tokens periodically

---

## Quick Reference

| Token | Workflow | Repo | Scope | URL |
|-------|----------|------|-------|-----|
| `SKILL_REPO_PAT` | Sync Skill | `mhrsntrk/ask-master-skill` | `repo` | Settings → Secrets → Actions |
| `HOMEBREW_GITHUB_API_TOKEN` | GoReleaser | `mhrsntrk/homebrew-ask-master` | `repo` | Settings → Secrets → Actions |

| Secret URL | Location |
|------------|----------|
| Token generation | https://github.com/settings/tokens |
| Add secret to ask-master | https://github.com/mhrsntrk/ask-master/settings/secrets/actions |
| View workflow runs | https://github.com/mhrsntrk/ask-master/actions |
| Check skill repo | https://github.com/mhrsntrk/ask-master-skill |
| Check homebrew tap | https://github.com/mhrsntrk/homebrew-ask-master |

---

## After Setup Checklist

- [ ] Generated `SKILL_REPO_PAT` with `repo` scope
- [ ] Added `SKILL_REPO_PAT` to `ask-master` repo secrets
- [ ] Generated `HOMEBREW_GITHUB_API_TOKEN` with `repo` scope
- [ ] Added `HOMEBREW_GITHUB_API_TOKEN` to `ask-master` repo secrets
- [ ] Installed GoReleaser locally (`brew install goreleaser/tap/goreleaser`)
- [ ] Tested skill sync with a dummy commit to `skill/SKILL.md`
- [ ] Tested GoReleaser with `goreleaser release --snapshot --clean`
- [ ] Published first release with `git tag v1.0.0 && git push origin v1.0.0`
- [ ] Verified `brew install mhrsntrk/ask-master/ask-master` works
