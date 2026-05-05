# GitHub Token Setup Guide

This document explains how to set up the required GitHub Personal Access Tokens (PATs) for the automated workflows in this repository.

## Overview

The `ask-master` repository has two automated workflows that require GitHub tokens:

1. **Skill Sync Workflow** — Syncs `skill/SKILL.md` to the `ask-master-skill` repository
2. **GoReleaser Workflow** — Builds binaries and publishes to Homebrew tap when you push a tag

Both workflows need separate fine-grained PATs with repository-level access to push to their respective repositories.

---

## Why Fine-Grained Tokens?

This guide uses **fine-grained personal access tokens** instead of classic tokens. They are more secure because they:
- Only work on specific repositories you choose
- Have limited, explicit permissions (e.g., just "Contents: write")
- Have built-in expiration dates (90 days max)
- Are easier to rotate and audit

---

## Token 1: SKILL_REPO_PAT (Skill Sync)

### What it does

When you push changes to `skill/SKILL.md` on the `master` branch, a GitHub Action automatically copies that file to the `mhrsntrk/ask-master-skill` repository.

### Step-by-step setup

#### Step 1: Generate the Fine-Grained PAT

1. Go to https://github.com/settings/personal-access-tokens/new
2. Give it a descriptive name, e.g., `ask-master-skill-sync`
3. Set expiration (max 90 days — GitHub enforces this for fine-grained tokens)
4. Under **Resource owner**, select `mhrsntrk`
5. Under **Repository access**, select **"Only select repositories"** and choose:
   - `mhrsntrk/ask-master-skill`
6. Under **Repository permissions**, expand **"Contents"** and set it to:
   - ✅ **Read and write** — to push the updated `SKILL.md`
7. Under **Account permissions**, leave everything at **"No access"**
8. Scroll down and click **"Generate token"**
9. **COPY THE TOKEN IMMEDIATELY** — you won't see it again

#### Step 2: Add it as a repository secret

1. Go to https://github.com/mhrsntrk/ask-master/settings/secrets/actions
2. Click **"New repository secret"**
3. Fill in:
   - **Name**: `SKILL_REPO_PAT`
   - **Value**: paste the fine-grained token you just copied
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

#### Step 1: Generate the Fine-Grained PAT

1. Go to https://github.com/settings/personal-access-tokens/new
2. Give it a descriptive name, e.g., `ask-master-homebrew-release`
3. Set expiration (max 90 days)
4. Under **Resource owner**, select `mhrsntrk`
5. Under **Repository access**, select **"Only select repositories"** and choose:
   - `mhrsntrk/ask-master`
   - `mhrsntrk/homebrew-ask-master`
6. Under **Repository permissions**, set the following:
   - **Contents**: ✅ **Read and write** — to read release notes and push Homebrew formula
   - **Metadata**: ✅ **Read-only** (GitHub selects this automatically)
7. Under **Account permissions**, leave everything at **"No access"**
8. Scroll down and click **"Generate token"**
9. **COPY THE TOKEN IMMEDIATELY**

#### Step 2: Add it as a repository secret

1. Go to https://github.com/mhrsntrk/ask-master/settings/secrets/actions
2. Click **"New repository secret"**
3. Fill in:
   - **Name**: `HOMEBREW_GITHUB_API_TOKEN`
   - **Value**: paste the fine-grained token you just copied
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

## Fine-Grained Token Summary

| Token | Target Repos | Required Permission |
|-------|--------------|---------------------|
| **SKILL_REPO_PAT** | `mhrsntrk/ask-master-skill` | Contents: **Read and write** |
| **HOMEBREW_GITHUB_API_TOKEN** | `mhrsntrk/ask-master` + `mhrsntrk/homebrew-ask-master` | Contents: **Read and write** |

**Never grant account-level permissions.** Keep everything at "No access" except the specific repository permissions listed above.

---

## Troubleshooting

### "Resource not accessible by personal access token"

The PAT likely expired, or the token doesn't have access to the target repository. Common causes:
- Token expired (fine-grained tokens max 90 days)
- Wrong repository selected during token creation
- Missing "Contents: Read and write" permission

**Fix:** Go to https://github.com/settings/personal-access-tokens, find the token, click **"Edit"**, verify the repositories and permissions, and regenerate if needed.

### "Could not resolve to a Repository"

The token doesn't have access to the repository it's trying to push to. Make sure:
- For skill sync: the token has access to `mhrsntrk/ask-master-skill`
- For homebrew: the token has access to `mhrsntrk/homebrew-ask-master`

### Workflow runs but skill repo doesn't update

Check the Action logs. Common issues:
- The PAT has read-only access instead of read-and-write
- The branch name is `master` not `main` (the workflow only triggers on `master` or `main` pushes to `skill/SKILL.md`)

### GoReleaser fails with "already exists"

If you re-run a release for an existing tag, GoReleaser might fail because the GitHub Release already has assets. Either delete the release and re-tag, or increment the version.

### Homebrew formula doesn't appear

Check that:
1. The `HOMEBREW_GITHUB_API_TOKEN` secret is set correctly
2. The token has access to both `ask-master` and `homebrew-ask-master`
3. The `homebrew-ask-master` repo exists and is public
4. The GoReleaser workflow completed successfully

---

## Security Notes

- **Never commit tokens to git** — always use repository secrets
- **Use fine-grained tokens** — they are scoped to specific repos and permissions
- **Rotate tokens regularly** — set expiration dates and regenerate before they expire (max 90 days for fine-grained)
- **Scope the tokens** — these tokens only need "Contents: Read and write" on specific repos
- **Monitor usage** — check your token activity at https://github.com/settings/personal-access-tokens periodically
- **Delete unused tokens** — if you stop using a workflow, delete its token

---

## Quick Reference

| Token | Workflow | Target Repo(s) | Permission | URL |
|-------|----------|----------------|------------|-----|
| `SKILL_REPO_PAT` | Sync Skill | `mhrsntrk/ask-master-skill` | Contents: Read and write | Settings → Secrets → Actions |
| `HOMEBREW_GITHUB_API_TOKEN` | GoReleaser | `mhrsntrk/ask-master` + `mhrsntrk/homebrew-ask-master` | Contents: Read and write | Settings → Secrets → Actions |

| Action | URL |
|--------|-----|
| Generate fine-grained token | https://github.com/settings/personal-access-tokens/new |
| Manage existing tokens | https://github.com/settings/personal-access-tokens |
| Add secret to ask-master | https://github.com/mhrsntrk/ask-master/settings/secrets/actions |
| View workflow runs | https://github.com/mhrsntrk/ask-master/actions |
| Check skill repo | https://github.com/mhrsntrk/ask-master-skill |
| Check homebrew tap | https://github.com/mhrsntrk/homebrew-ask-master |

---

## After Setup Checklist

- [ ] Generated `SKILL_REPO_PAT` (fine-grained, Contents: Read and write, repo: `ask-master-skill`)
- [ ] Added `SKILL_REPO_PAT` to `ask-master` repo secrets
- [ ] Generated `HOMEBREW_GITHUB_API_TOKEN` (fine-grained, Contents: Read and write, repos: `ask-master` + `homebrew-ask-master`)
- [ ] Added `HOMEBREW_GITHUB_API_TOKEN` to `ask-master` repo secrets
- [ ] Installed GoReleaser locally (`brew install goreleaser/tap/goreleaser`)
- [ ] Tested skill sync with a dummy commit to `skill/SKILL.md`
- [ ] Tested GoReleaser with `goreleaser release --snapshot --clean`
- [ ] Published first release with `git tag v1.0.0 && git push origin v1.0.0`
- [ ] Verified `brew install mhrsntrk/ask-master/ask-master` works
