![](res/radiorave_logo.png)

[![AppVeyor](https://ci.appveyor.com/api/projects/status/github/OpenFusionProject/OpenFusion?svg=true)](https://ci.appveyor.com/project/OpenFusionProject/openfusion)
[![Discord](https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord)](https://discord.gg/DYavckB)

OpenFusion is a reverse-engineered server for FusionFall. It currently primarily targets version `beta-20100104` and has some support for version `beta-20100728` of the original game.

Further documentation pending.

## Usage

tl;dr:

1. Download the client+server bundle from [here](https://github.com/OpenFusionProject/OpenFusion/releases/download/1.2.1/OpenFusion.zip).
2. Run `FreeClient/installUnity.bat` once

From then on, any time you want to run the "game":

3. Run `OpenFusion/winfusion.exe` (optional if you're using the public server)
4. Run `FreeClient/OpenFusionClient.exe`

Currently the client by default connects to a public server hosted by Cake. Change the loginInfo.php to point to your own server if you want to host your own.

If you want, [compiled binaries (artifacts) for each new commit can be found on AppVeyor.](https://ci.appveyor.com/project/OpenFusionProject/openfusion)

For a more detailed overview of the game's architecture and how to configure it, read the following sections.

## Architecture

FusionFall consists of the following components:

* A web browser compatible with the old NPAPI plugin interface
* A web server that acts as a gateway for launching the game
* A custom version of the Unity Web Player, which gets loaded as an NPAPI plugin
* A `.unity3d` bundle that contains the game code and essential resources (loading screen, etc.)
* A login server that speaks the FusionFall network protocol over TCP
* A shard server that does the same on another port

The original game made use of the player's actual web browser to launch the game, but since then the NPAPI plugin interface the game relied on has been deprecated and is no longer available in most modern browsers. Both Retro and OpenFusion get around this issue by distributing an older version of Electron, a software package that is essentially a specialized web browser.

The browser/Electron client opens a web page with an `<embed>` tag of MIME type `application/vnd.unity`, where the `src` param is the address of the game's `.unity3d` entrypoint.

This triggers the browser to load an NPAPI plugin that handles this MIME type, the Unity Web Player, which the browser looks for in `C:\Users\USERNAME\AppData\LocalLow\Unity\WebPlayer`.
The Web Player was previously copied there by `installUnity.bat`.

Note that the version of the web player distributed with OpenFusion expects a standard `UnityWeb` magic number for all assets, instead of Retro's modified `streamed` magic number.
This will potentially become relevant later, as people start experimenting and mixing and matching versions.

The web player will execute the game code, which will request the following files from the server: `/assetInfo.php` and `/loginInfo.php`.

`FreeClient/resources/app/files/assetInfo.php` contains the address from which to fetch the rest of the game's assets (the "dongresources").
Normally those would be hosted on the same web server as the gateway, but the OpenFusion distribution (in it's default configuration) doesn't use a web server at all!
It loads the web pages locally using the `file://` schema, and fetches the game's assets from Turner's CDN (which is still hosting them to this day!).

`FreeClient/resources/app/files/loginInfo.php` contains the IP:port pair of the FusionFall login server, which the client will connect to. This login server drives the client while it's in the Character Selection menu, as well as Character Creation and the Tutorial.

When the player clicks "ENTER THE GAME" (or completes the tutorial), the login server sends it the address of the shard server, which the client will then connect to and remain connected to during gameplay.

## Configuration

You can change the ports the FusionFall server listens on in `Server/config.ini`. Make sure the login server port is in sync with `loginInfo.php`.
The shard port needs no such synchronization.
You can also configure the distance at which you'll be able to see other players, though by default it's already as high as you'll want it.

If you want to play with friends, you can change the IP in `loginInfo.php` to a login server hosted elsewhere.
This just works if you're all under the same LAN, but if you want to play over the internet you'll need to open a port, use a service like Hamachi or nGrok, or host the server on a VPS (just like any other gameserver).

If you're in a region in which Turner's CDN doesn't still have the game's assets cached, you won't be able to play the game in its default configuration.
You'll need to obtain the necessary assets elsewhere and set up your own local web server to host them, because unlike web browsers, the game itself cannot interpret the `file://` schema, and will thus need the assets hosted on an actual HTTP server.
Don't forget to point `assetInfo.php` to where you're hosting the assets and change the `src` param of both the `<embed>` tag and the `<object>` tag in `FreeClient/resources/app/files/index.html` to where you're hosting the `.unity3d` entrypoint.

If you change `loginInfo.php` or `assetInfo.php`, make sure not to put any newline characters (or any other whitespace) at the end of the file(s).
Some modern IDEs/text editors do this automatically. If all else fails, use Notepad.

## Compiling 

OpenFusion has one external dependency: SQLite. You can install it on Windows using `vcpkg`, and on Unix/Linux using your distribution's package manager. For a more indepth guide on how to set up vcpkg, [check this wiki page](https://github.com/OpenFusionProject/OpenFusion/wiki/Installing-SQLite-on-Windows-using-vcpkg).

You have two choices for compiling OpenFusion: the included Makefile and the included CMakeLists file.

### Makefile

A detailed compilation guide is available for Windows users in the wiki [using MinGW-w64 and MSYS2](https://github.com/OpenFusionProject/OpenFusion/wiki/Compilation-on-Windows). Otherwise, to compile it for the current platform you're on, just run `make` with the correct build tools installed (currently make and clang).

### CMake

A detailed guide is available [in the wiki](https://github.com/OpenFusionProject/OpenFusion/wiki/Compilation-with-CMake-or-Visual-Studio) for people using regular old CMake or the version of CMake that comes with Visual Studio. tl;dr: `cmake -B build`

## Contributing

If you'd like to contribute to this project, please read [CONTRIBUTING.md](CONTRIBUTING.md).

## Gameplay

The goal of the project is to faithfully recreate the game as it was at the time of the targeted build.
The server is not yet complete, however, and some functionality is still missing.

Because the server is still in development, ordinary players are allowed access to a few admin commands:

![](res/sane_upsell.png)

### Movement commands
* A `/speed` of around 2400 or 3000 is nice.
* A `/jump` of about 50 will send you soaring
* [This map](res/dong_number_map.png) (credit to Danny O) is useful for `/warp` coordinates.
* `/goto` is useful for more precise teleportation (ie. for getting into Infected Zones, etc.).

### Item commands
* `/itemN [type] [itemId] [amount]`
  (Refer to the [item list](https://docs.google.com/spreadsheets/d/1mpoJ9iTHl_xLI4wQ_9UvIDYNcsDYscdkyaGizs43TCg/))

### Nano commands
* `/nano [id] (1-36)`
* `/nano_equip [id] (1-36) [slot] (0-2)`
* `/nano_unequip [slot] (0-2)`
* `/nano_active [slot] (0-2)`

## Accounts

A basic account system has been added, when logging in if the username doesn't exist in the database, a new account with the provided password will be made and you'll be automatically logged in. Otherwise a login attempt will be made. A username must be between 4 and 32 characters, and a password must be between 8 and 32 characters otherwise the account will be rejected.
