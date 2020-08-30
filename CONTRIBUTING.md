## Contribution

In order to contribute, you should do the following:

1. Fork this repository.
2. Clone your repository to your local system:
```
git clone https://github.com/<your_github_username>/OpenFusion.git
```
3. Add the original repository as a remote:
```
git remote add upstream https://github.com/OpenFusionProject/OpenFusion.git
git fetch upstream
```
4. Edit the files as you like!
5. Use `git add` and `git commit` commands to commit to your local repository. Smaller commits are easier to read.
6. If you're adding new source or header files, make sure you add the relative paths of those files to the `Makefile` under the correct category with a `\` character at the end. C++ sources, C++ header files, C sources, and C header files should all be put under different categories in the `Makefile`. Use `make` (or `mingw32-make.exe` for Windows MinGW) to make sure your code compiles.
7. Run `git pull upstream master` to make sure your changes don't create conflicts with this repository. If they do, resolve them.
8. Push your local changes to your fork of this repository.
9. Make a Pull Request to this repository from your repository.

Once your Pull Request is approved, it will be added to this repository.
