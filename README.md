# DESCRIPTION
This repository contains source code of a program which should be ran as the
DISPATCHER system process of [Rose WM](https://github.com/everard/rosewm)
Wayland Compositor.

This program is controlled by IPC commands which have the following format
(hexadecimal).

```
BB 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00
```

Each IPC command starts with one meaningful byte (denoted *BB*) which is
followed by 63 zero-valued bytes. The meaning of the first byte is described in
the following table.

| FIRST BYTE | DESCRIPTION                                          |
|------------|------------------------------------------------------|
| 0x00       | Requests display of command prompt (normal).         |
| 0x01       | Requests display of command prompt (privileged).     |
| 0x02       | Requests reload of the database of executable files. |

# COMPILATION
To compile the program, run:
```
make
```

Note: C++20 capable compiler is required. Uses coroutines with ASIO.

To copy the program to the `/usr/local/bin/` directory, run:
```
sudo make install
```

To remove the program from the `/usr/local/bin/` directory, run:
```
sudo make uninstall
```

Build system uses `pkg-config` to obtain compiler and linker flags for
dependencies.

Dependencies:
 * asio
 * freetype2
 * fribidi
 * SDL2

# LICENSE
Copyright Nezametdinov E. Ildus 2023.

Distributed under the GNU General Public License, Version 3.
(See accompanying file LICENSE_GPL_3_0.txt or copy at
https://www.gnu.org/licenses/gpl-3.0.txt)
