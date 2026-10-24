#!/usr/bin/env bash
# Merges a new upstream 7-Zip release (a tag of the "upstream" remote,
# ip7z/7zip) into main, then does the steps after the merge: regenerates the
# upstream source lists, sets the version of the next port release (TAG.1)
# in the AUR packages and the AppStream metadata, runs check.sh and prints
# what is left to do by hand. It commits nothing after the merge commit.
# See linux/docs/UPSTREAM.md.
#
# Usage: linux/tools/upstream_sync.sh [options] [TAG]
#   TAG            the release to merge (default: the newest upstream tag)
#   --continue     only the steps after the merge, once merge conflicts are
#                  resolved and the merge is committed
#   --no-wayland   passed to check.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

TAG=""
CONTINUE=0
CHECK_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --continue) CONTINUE=1 ;;
    --no-wayland) CHECK_ARGS+=(--no-wayland) ;;
    -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
    -*) echo "upstream_sync.sh: unknown option $1" >&2; exit 2 ;;
    *) TAG="$1" ;;
  esac
  shift
done

step() { printf '\n==> %s\n' "$*"; }
die() { echo "upstream_sync.sh: $*" >&2; exit 1; }

# upstream release tags are "26.05"; the port's own are "v26.05.1"
RELEASE_TAGS='[0-9]*.[0-9]*'

# The last upstream release merged into HEAD.
merged_release() {
  git describe --tags --abbrev=0 --match "$RELEASE_TAGS" HEAD
}

[[ "$(git branch --show-current)" == main ]] || die "not on main"
git diff --quiet && git diff --cached --quiet || die "uncommitted changes"

if [[ $CONTINUE -eq 0 ]]; then
  step "Fetch upstream"
  git fetch upstream --tags
  if [[ -z "$TAG" ]]; then
    TAG="$(git tag -l "$RELEASE_TAGS" --sort=-v:refname | head -n 1)"
  fi
  git rev-parse -q --verify "refs/tags/$TAG" >/dev/null || die "no tag $TAG"
  CURRENT="$(merged_release)"
  if git merge-base --is-ancestor "refs/tags/$TAG" HEAD; then
    echo "$TAG is merged already (last merged release: $CURRENT)"
    exit 0
  fi
  git merge-base --is-ancestor "refs/tags/$CURRENT" "refs/tags/$TAG" \
    || die "$TAG does not follow $CURRENT"

  step "Merge 7-Zip $TAG (after $CURRENT)"
  if ! git merge --no-ff --no-edit -m "Merge upstream 7-Zip $TAG" "refs/tags/$TAG"; then
    echo
    echo "Conflicts in:"
    git diff --name-only --diff-filter=U | sed 's/^/  /'
    echo
    echo "Resolve them (keep the Linux edits small, see the upstream patch"
    echo "policy in linux/docs/PLAN.md), commit the merge, then run:"
    echo "  $0 --continue"
    exit 1
  fi
fi

TAG="$(merged_release)"
VERSION="$TAG.1"

step "Upstream source lists"
python3 -I linux/tools/upstream_sources.py

step "Version of the next port release: $VERSION"
for pkgbuild in linux/packaging/aur/*/PKGBUILD; do
  sed -i -e "s/^pkgver=.*/pkgver=$VERSION/" -e "s/^pkgrel=.*/pkgrel=1/" "$pkgbuild"
done
METAINFO=linux/packaging/pl.mikigal.SevenZip.metainfo.xml
if ! grep -q "<release version=\"$VERSION\"" "$METAINFO"; then
  sed -i "s|^\(\s*\)<releases>|&\n\1  <release version=\"$VERSION\" date=\"$(date +%F)\"/>|" "$METAINFO"
fi

step "Check"
CHECK_OK=1
linux/tools/check.sh "${CHECK_ARGS[@]}" || CHECK_OK=0

step "Upstream edits since 7-Zip $TAG (linux/docs/PLAN.md, Appendix B)"
git diff --stat "refs/tags/$TAG" HEAD -- C CPP Asm DOC
git log --oneline --no-merges "refs/tags/$TAG..HEAD" -- C CPP Asm DOC

step "Left to do"
cat <<EOF
- Look at the changes ($(git status --short | wc -l) files) and commit them:
  the source lists, and the version $VERSION (set the release date in the
  AppStream metadata when releasing, see linux/docs/RELEASE.md).
- Check that each upstream edit above is still needed and still small.
- The Windows release of 7-Zip $TAG (ask before downloading it): extract it
  to build/windows-release/$TAG, configure Z7_WINDOWS_RELEASE with it, run
  the rcgen_windows_release test and look at the Wine reference pictures
  (linux/README.md, "Windows reference pictures (Wine)").
- Replace linux/lang/*.txt with the Lang folder of that release and update
  linux/lang/README.md.
- Read the upstream history (DOC/src-history.txt) for new dialogs, options
  and features that the Linux port should follow.
EOF
if [[ $CHECK_OK -eq 0 ]]; then
  echo
  die "check.sh failed: fix the build and the tests first"
fi
