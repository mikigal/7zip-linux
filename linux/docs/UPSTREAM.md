# Merging a new upstream release

`main` is upstream 7-Zip (the `upstream` remote, `ip7z/7zip`) with its
release tags merged in, plus `linux/` and a few small edits of upstream
files (`PLAN.md`, "Upstream patch policy" and Appendix B). A new 7-Zip
release comes in as a merge of its tag (`26.05`, `26.06`, ...):

```sh
linux/tools/upstream_sync.sh               # the newest upstream release
linux/tools/upstream_sync.sh 26.06         # a given one
```

The script:

1. checks that `main` is checked out and has no uncommitted changes;
2. fetches the upstream tags and merges the release with a merge commit
   ("Merge upstream 7-Zip 26.06"); on conflicts it stops and lists them;
3. regenerates `linux/cmake/UpstreamSources.cmake`
   (`upstream_sources.py`);
4. sets the version of the next port release, `26.06.1`, in the AUR
   packages and the AppStream metadata;
5. runs `linux/tools/check.sh` (add `--no-wayland` for a quick run);
6. lists the upstream edits of the port since the new release, and what is
   left to do by hand.

After the merge commit it commits nothing: the source lists and the version
are left for review.

## Conflicts

`linux/` never conflicts; conflicts come from the upstream files that the
port edits (Appendix B). Resolve them with the upstream patch policy: keep
the Linux edits small and inside the file's own `#ifdef _WIN32` style, and
keep the upstream console `7zz` building. Commit the merge, then:

```sh
linux/tools/upstream_sync.sh --continue
```

## By hand, after the script

- Commit the source lists and the version (for example "linux: 7-Zip
  26.06"), after a full `check.sh`.
- Check that each upstream edit is still needed; drop the ones the new
  release makes unnecessary.
- Get the Windows release of the same version (the user approves the
  download), extract it to `build/windows-release/26.06`, configure
  `Z7_WINDOWS_RELEASE` with it and run `rcgen_windows_release` and the Wine
  reference pictures (`linux/README.md`). Changed dialogs need a look at
  the gallery and, after review, new golden images.
- Replace `linux/lang/*.txt` with the `Lang` folder of that release and
  update `linux/lang/README.md` (file name, URL, SHA-256).
- Read `DOC/src-history.txt` for new dialogs, options and features that the
  port should follow.
- Then make a release: `docs/RELEASE.md`.

The upstream tags are fetched into the local repository. Push only the
port's own release tags (`git push origin v26.06.1`), never `--tags`.
