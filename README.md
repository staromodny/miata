# Miata
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/github/license/staromodny/windower)](https://github.com/staromodny/windower/blob/master/LICENSE)
[![GitHub issues](https://img.shields.io/github/issues/staromodny/miata?color=green)](https://github.com/staromodny/miata/issues)

Free and open-source radar-hack software for Counter-Strike 2.

## Compiling

### Prerequisites
- [CMake 3.25](https://cmake.org/cmake/help/latest/release/3.25.html) or newer
- MSVC compiler
- Windows SDK

### Building
1. Clone the repository:
   ```
   git clone https://github.com/v3ctra/miata.git
   cd miata
   ```
2. Configure the project:
   ```
   cmake -S . -B build -A x64
   ```
3. Build the project:
   ```
   cmake --build build --config Release
   ```

## Example Config
If config.txt doesn't exist - cheat will use default values, defined in entrypoint.
```
radar.pos.x=5        # x position of radar on the screen
radar.pos.y=5        # y position of radar on the screen
radar.size.w=128     # width of the radar's window
radar.size.h=128     # height of the radar's window
cheat.window.hide=0  # Determines if window should be hidden on the screenshots and recordings
cheat.window.vsync=0 # Determines if window should have vertical sync enabled
```

## Updating
Cheat will perform an attempt to read file called "offsets.txt". The contents of the file should look like:
```
offsets.client.dwLocalPlayerPawn=29302688
offsets.client.dwEntityList=30500696
offsets.client.dwViewAngles=31712480
offsets.client.m_hPlayerPawn=2300
offsets.client.m_iHealth=844
offsets.client.m_iTeamNum=1003
offsets.client.m_vOldOrigin=5560
```
Always up to date offsets can be found [HERE](https://github.com/a2x/cs2-dumper/blob/main/output/offsets.json). and [HERE](https://github.com/a2x/cs2-dumper/blob/main/output/client_dll.json).

## Contribution
Contributions via Pull Requests are welcome :)
