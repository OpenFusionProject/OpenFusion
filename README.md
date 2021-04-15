<p align="center"><img width="640" src="res/openfusion-hero.png" alt=""></p>

<p align="center">
    <a href="https://github.com/OpenFusionProject/OpenFusion/releases/latest"><img src="https://img.shields.io/github/v/release/OpenFusionProject/OpenFusion" alt="Current Release"></a>
    <a href="https://ci.appveyor.com/project/OpenFusionProject/openfusion"><img src="https://ci.appveyor.com/api/projects/status/github/OpenFusionProject/OpenFusion?svg=true" alt="AppVeyor"></a>
    <a href="https://discord.gg/DYavckB"><img src="https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord" alt="Discord"></a>
    <a href="https://github.com/OpenFusionProject/OpenFusion/blob/master/LICENSE.md"><img src="https://img.shields.io/github/license/OpenFusionProject/OpenFusion" alt="License"></a>
</p>

OpenFusion is a reverse-engineered server for FusionFall. It primarily targets versions `beta-20100104` and `beta-20111013` of the original game, with [limited support](https://github.com/OpenFusionProject/OpenFusion/wiki/FusionFall-Version-Support) for others.

## Usage

### Getting Started

1. Download the client from [here](https://github.com/OpenFusionProject/OpenFusion/releases/download/1.3/OpenFusionClient-1.3.zip).
2. Extract it to a folder of your choice. Note: if you are upgrading from an older version, it is preferable to start with a fresh folder rather than overwriting a previous install.
3. Run OpenFusionClient.exe - you will be given a choice between two public servers by default. Select the one you wish to play and click connect.
4. To create an account, simply enter the details you wish to use at the login screen then click Log In. Do *not* click register, as this will just lead to a blank screen.
5. Make a new character, and enjoy the game! Your progress will be saved automatically, and you can resume playing by entering the login details you used in step 4.

### Hosting a server

1. Grab `OpenFusionServer-1.3-original.zip` or `OpenFusionServer-1.3-academy.zip` from [here](https://github.com/OpenFusionProject/OpenFusion/releases/tag/1.3).
2. Extract it to a folder of your choice, then run `winfusion.exe` (Windows) or `fusion` (Linux) to start the server.
3. Add a new server to the client's list: the default port is 23000, so the full IP with default settings would be 127.0.0.1:23000.
4. Once you've added the server to the list, connect to it and log in. If you're having trouble with this, refer to steps 4 and 5 from the previous section.

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

This triggers the browser to load an NPAPI plugin that handles this MIME type, the Unity Web Player, which the browser looks for in `C:\Users\%USERNAME%\AppData\LocalLow\Unity\WebPlayer`.
The Web Player was previously copied there by `installUnity.bat`.

Note that the version of the web player distributed with OpenFusion expects a standard `UnityWeb` magic number for all assets, instead of Retro's modified `streamed` magic number.
This will potentially become relevant later, as people start experimenting and mixing and matching versions.

The web player will execute the game code, which will request the following files from the server: `/assetInfo.php` and `/loginInfo.php`.

`/assetInfo.php` contains the address from which to fetch the rest of the game's assets (the "dongresources").
Normally those would be hosted on the same web server as the gateway, but the OpenFusion distribution (in it's default configuration) doesn't use a web server at all!
It loads the web pages locally using the `file://` schema, and fetches the game's assets from Turner's CDN (which is still hosting them to this day!).

`/loginInfo.php` contains the IP:port pair of the FusionFall login server, which the client will connect to. This login server drives the client while it's in the Character Selection menu, as well as Character Creation and the Tutorial.

When the player clicks "ENTER THE GAME" (or completes the tutorial), the login server sends it the address of the shard server, which the client will then connect to and remain connected to during gameplay.

## Configuration

You can change the ports the FusionFall server listens on in `config.ini`. Make sure the login server port is in sync with what you enter into the client's server list - the shard port needs no such synchronization.

This config file also has several other options you can tweak, including log verbosity, database saving interval, default account/permission level, and more. See the comments within [the config file itself](https://github.com/OpenFusionProject/OpenFusion/blob/master/config.ini) for more details.

If you want to play with friends, simply enter the login server details into the `Add Server` dialogue in OpenFusionClient.
This just works if you're all under the same LAN, but if you want to play over the internet you'll need to open a port, use a service like Hamachi or nGrok, or host the server on a VPS (just like any other gameserver).

## Compiling 

OpenFusion has one external dependency: SQLite. You can install it on Windows using `vcpkg`, and on Unix/Linux using your distribution's package manager. For a more indepth guide on how to set up vcpkg, [read this guide on the wiki](https://github.com/OpenFusionProject/OpenFusion/wiki/Installing-SQLite-on-Windows-using-vcpkg).

You have two choices for compiling OpenFusion: the included Makefile and the included CMakeLists file.

### Makefile

A detailed compilation guide is available for Windows users in the wiki [using MinGW-w64 and MSYS2](https://github.com/OpenFusionProject/OpenFusion/wiki/Compilation-on-Windows). Otherwise, to compile it for the current platform you're on, just run `make` with the correct build tools installed (currently make and clang).

### CMake

A detailed guide is available [on the wiki](https://github.com/OpenFusionProject/OpenFusion/wiki/Compilation-with-CMake-or-Visual-Studio) for people using regular old CMake or the version of CMake that comes with Visual Studio. tl;dr: `cmake -B build`

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

### A full list of commands can be found [here](https://github.com/OpenFusionProject/OpenFusion/wiki/Ingame-Command-list).
