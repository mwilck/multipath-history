# History of multipath-tools

*(This README file is about the history reconstruction. Find the original
**multipath-tools README** [here](README).)*

This repository contains the reconstructed early history of
[multipath-tools][1], covering the revision history before the author,
Christophe Varoqui, began tracking the code with git in 
[the upstream repository][2].
The tree was created by taking the historical tarballs from the 
[sourceware download site][3], iteratively copying the code, and trying to
split the 
changes between releases into meaningful commits, using the code itself and the
Changelog entries as guidance. The result is a history of
487 commits extracted from 60 tarballs between the initial release
made by Greg Kroah-Hartmann in his udev repository (0.0.1, Nov 12th, 2003) and
Christophe's first git import (0.4.5-pre2, May 1st, 2005).

**Note on authorship:** although Christophe is listed as the author of all the
commits, it'd be pure coincidence if he actually created none of them in exactly 
this form. I am responsible for every single commit in the "master" branch.
However, Christophe wrote the code, and I didn't want to pretend that I did. 
3rd party authors are mentioned in the commit messages if they were mentioned 
in the Changelog.

**Note on timestamps:** The commit dates are *fake*. I tried to guess the dates
roughly from the timestamps of the tar balls. I made no attempt at better
guesses.

**Note on tags:** The version number tags (e.g. 0.2.7) represent the
same content as the respective tarball from the Red Hat FTP site. In
particular in the early releases (before 0.1.0), Christophe named the
tar ball after the release he was __working on__, not the one just finished.
That explains whey the commit tagged 0.0.18 is called "multipath 017".

**Note on accuracy:** The tagged commits produce the same code
as in the respective tarballs. __Exception:__ some files which were obviously
packaged in the tarballs by mistake (e.g. generated object files, backups)
have been omitted.

**Note on commit ordering between releases:** I tried to create commits in the
order listed in the Changelog, but that wasn't always possible. When trying to
disentangle 
larger changes, I often isolated obvious changes first (e.g. file
removals or renames), even if they were not the first thing to happen after
the previous release.

## master branch

The [master branch][4] contains only the reconstructed commits of the history
until 0.4.5-pre2, and this README.

## modern branch

The ["modern" branch][5] was created by cherry-picking the "first-parent"
history from the main trunk of the upstream repository on top of the
reconstructed tree. This combines the
reconstruction work with the later code up to current releases, and provides an
almost complete view of the entire multipath-tools history. This allows
"git blame" to show lines from current and historic code in a single view.
It doesn't show commits from merged upstream branches, as this can't be done with
cherry-picking. The cherry-picked commits have new IDs, but the original
commit IDs from upstream are referenced in the commit messages.

[1]: http://christophe.varoqui.free.fr/
[2]: https://git.opensvc.com/multipath-tools/.git
[3]: http://www.sourceware.org/pub/dm/multipath-tools/
[4]: https://github.com/mwilck/multipath-history
[5]: https://github.com/mwilck/multipath-history/tree/modern
