# Contributing

If you want to contribute to OpenFusion's development, that's great!
We'd appreciate it if you already had some programming experience, as well as experience with software version control using git.

If you've never used git before, please take the time to research it yourself.
There is an abundance of online resources for getting started with git.

If, however, you have no experience programming, this type of project may not be the best one to start with.
OpenFusion is written in C++, which really isn't the best language to start with, as it has many extremely subtle pitfalls with potentially catastrophic consequences that are often very hard for an inexperienced programmer to detect and avoid.
This is compounded by the fact that this is a server, which means it's a long-running, multithreaded process exposed to the network, decoding an externally-imposed binary protocol -- written in C++.
A bad combination.

With that out of the way, the rest of this document will serve to address the matter of clean commits in Pull Requests.

## Repository cleanliness

The commit history in a git repository is important.
Unlike some other version control systems, git allows developers to destructively edit history so as to enjoy the benefits of both frequent "saves" (as in, having the ability to record corrections as soon as you make them) and clean commits which perfectly encapsulate a given change.
Each developer has their own preferred way of doing things, of course, but in general one commit should represent one functional change in the codebase as a whole.
Developers should be able to view a project's commit history and easily understand how the project has evolved over time.

Achieving this is often daunting for new users of git.
I know of two intermediate-level resources (that is, for after you've learned the basics of git) I can recommend for understanding how to properly manage your repository's history -- [think-like-a-git.net](http://think-like-a-git.net/) and [git-rebase.io](https://git-rebase.io/).
Both are pretty short reads and following them will get you up to speed with branches and the rebasing thereof.

I will now cover a few examples of the complications people have encountered contributing to this project, how to understand them, overcome them and henceforth avoid them.

## Dirty pull requests

Many Pull Requests OpenFusion receives fail to present a clean set of commits to merge.
These are generally either:

* Dozens of quick-fix commits the author made while working on their contribution
* Countless useless merge commits generated while trying to re-synchronize with the upstream repository

Few developers are fine with having their commit histories utterly destroyed by merging these changes in.
Many projects, when presented with such Pull Requests, will flat-out reject them or demand the author clean them up first before they can be accepted.

Cpunch, however, chooses to accept them anyway, but squashes them into the repository with the "rebase" merge strategy, instead of a regular merge.
Whereas a regular merge creates a "merge commit" which unites two branches together, a rebase instead *reconstructs* the commits from one branch onto the other, creating *different commits* with possibly the same contents.

If you read the above links, you'll note that this isn't exactly a perfect solution either, since rewriting history in public is usually a bad idea, but because these changes were originally only visible to the PR author, it is only they that will need to rebase their fork to re-sync with the upstream repository.
The obvious issue, then, is that the people submitting dirty PRs are the exact people who don't *know* how to rebase their fork to continue submitting their work cleanly.
So they end up creating countless merge commits when pulling upstream on top of their own incompatible histories, and then submitting those merge commits in their PRs and the cycle continues.

## The details

A git commit is uniquely identified by its SHA1 hash.
Its hash is generated from its contents, as well as the hash(es) of its parent commit(s).
(Most commits have one parent. Merge commits almost always have two, but octopus merges can have any number of parents.)
That means that even if two commits are exactly the same in terms of content, they're not the same commit if their parent commits differ (ex. if they've been rebased).
So if you keep issuing `git pull`s after upstream has merged a rebased version of your PR, you will never re-synchronize with it, and will instead construct an alternate history polluted by pointless merge commits.

## The solution

If you already have a messed-up fork and you have no changes on it that you're afraid to lose, the solution is simple:

* Ensure your `upstream` remote is up to date with `git fetch upstream`
* Make sure you're on master (`git branch master`)
* Set your master branch to the latest commit with `git reset --hard upstream/master`
* Propagate the change to your GitHub fork with `git push -f origin master`

And you're good to go.
If you do have some committed changes that haven't yet been merged upstream, you should probably save them on another branch (called "backup" or something) with `git checkout -b backup`.

If you do end up messing something up, don't worry, it most likely isn't really lost and `git reflog` is your friend.
(You can checkout an arbitrary commit, and make it into its own branch with `git checkout -b BRANCH` or set a pre-exisitng branch to it with `git reset --hard COMMIT`)

## Avoiding the problem

When working on a changeset you want to submit back upstream, don't do it on the main branch.
Create a work branch just for your changeset with `git checkout -b work`.
That way you can always keep master in sync with upstream with `git pull --ff-only upstream master`.
(`--ff-only` can be left out. If you find that git want you to make a merge commit, just back out of it by saving an empty commit message, then fix whatever the cause was.)

* If upstream gets new changes before you've had a chance to submit yours, just update master, create a new branch from your work branch and rebase your new work branch on master (such that your up-to-date changeset is now on the new work branch) and submit that one for your PR
* If you end up making a few ugly fixup commits, use `git rebase --interactive` to clean them up (on a new branch) before submitting your changeset
* If you get told to change something in the PR before it's merged, but after you've pushed it to your GitHub fork, rebase your changes locally, then force-push them onto the your fork's PR branch with `git push -f origin work1` or so

Creating new branches for the rebase isn't strictly necessary since you can always return a branch to its previous state with `git reflog`.

For moving uncommited changes around between branches, `git stash` is a real blessing.
