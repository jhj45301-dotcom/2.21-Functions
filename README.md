# 2.21 Functions

🔴 **GEODE BUTTON IS NOW IN SETTINGS!!!**

2.21 Functions is a fan-made Geode mod for Geometry Dash 2.2081 on
Windows and Android. It reconstructs selected interface elements that are
not available in this game version without changing Geometry Dash saves.

## Features

- Enables the original **The Map** entry in the Creator menu.
- Recreates the Keymaster dialogue and Tower unlock check.
- Provides a scrollable map assembled from the included map tiles.
- Displays the player's selected cube and colours on the map.
- Keeps **Versus** visible as an intentionally inactive placeholder.
- Moves the Geode entry from the main menu to Geometry Dash settings.

## Settings

- **Enable The Map** controls the restored map entry.
- **Enable Versus** controls the inactive Versus placeholder.
- **Enable Debug Map Button** exposes map and dialogue test buttons.

Debug controls are disabled by default.

## Building

The project uses CMake, C++20, Geode SDK 5.11.2, and the required
`geode.node-ids` dependency. Set `GEODE_SDK` to a configured SDK checkout,
then configure and build the project with CMake. The GitHub Actions workflow
is intentionally manual so unfinished commits do not create releases.

## Project status

Version `1.0.0` is the first public release candidate. The project is
unofficial and is not affiliated with RobTop Games or the Geode team.
