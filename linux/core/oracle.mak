# oracle.mak -- builds the upstream 7zz with the upstream makefile, as
# makefile.gcc in CPP/7zip/Bundles/Alone2 does (run it from there), and adds
# what that makefile lacks for incremental builds: dependencies on headers,
# so that an edit of an upstream header rebuilds the objects that include it.
# Pass CFLAGS_WARN_WALL with -MMD -MP.

include makefile.gcc

-include $(wildcard $(O)/*.d)
