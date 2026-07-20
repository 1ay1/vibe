# Releasing the native packages

Publishing to PyPI / npm / RubyGems / crates.io is **automated** by
[`.github/workflows/release.yml`](../../.github/workflows/release.yml).

## To cut a release

```sh
# 1. bump the version everywhere it's asserted (vibe.h, manifests, docs...),
#    then re-vendor so the packages ship the new header:
bindings/native/vendor.sh

# 2. commit, tag, push the tag:
git commit -am "release: v1.2.0"
git tag v1.2.0
git push origin main v1.2.0
```

Pushing the `v*` tag triggers the workflow, which for each package **re-vendors
`vibe.h`, builds, runs the package's own test, and only then publishes**. Jobs
are independent: a failure/misconfig in one registry doesn't block the others.
You can also run it on demand from the **Actions** tab (`workflow_dispatch`).

## One-time setup (you must do this once)

The workflow needs credentials for each registry.

### PyPI — Trusted Publishing (no secret)
1. Go to <https://pypi.org/manage/account/publishing/>.
2. Add a *pending publisher*: project `vibe-lang`, owner `1ay1`, repo `vibe`,
   workflow `release.yml`, environment `pypi`.
3. In GitHub repo **Settings → Environments**, create an environment named
   `pypi`. Done — OIDC handles auth, no token to store.

### npm, RubyGems, crates.io — API tokens as repo secrets
In **Settings → Secrets and variables → Actions**, add:

| Secret | Where to get it |
|--------|-----------------|
| `NPM_TOKEN` | npmjs.com → Access Tokens → *Automation* |
| `RUBYGEMS_API_KEY` | rubygems.org → Settings → API Keys (scope: push) |
| `CARGO_REGISTRY_TOKEN` | crates.io → Account Settings → API Tokens |

## First publish only

A registry name must be free the first time. If a name is taken, rename in the
package manifest (`pyproject.toml` / `package.json` / `*.gemspec` /
`Cargo.toml`), re-vendor, and re-tag. Current names:

| Registry | Name |
|----------|------|
| PyPI | `vibe-lang` |
| npm | `vibe-native` |
| RubyGems | `vibe-lang` |
| crates.io | `vibe-sys` |

Check availability: `pip index versions vibe-lang` · `npm view vibe-native` ·
`gem list -r vibe-lang` · `cargo search vibe-sys`.
