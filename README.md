# text_editor

A simple terminal-based text editor written in C.

This repository contains a small single-file terminal text editor (src/editor.c). The editor provides basic editing features such as opening and saving files, navigation, search, simple syntax highlighting for C and related file types, and line numbers.

## Demo

<video src="assests/demo2.mp4" controls width="960" playsinline>
  Your browser does not support the video tag.
</video>

Version
- 0.0.1 (see src/editor.c: version)

Features
- Open and edit text files in the terminal
- Save files to disk
- Line numbers with adjustable width
- Basic syntax highlighting for C, headers and plain text
- Search (forward/backward) with highlighted matches
- Common navigation keys: arrows, Home/End, PageUp/PageDown
- Simple undo-like protection: will warn on quit if file is modified

Build
Requires a POSIX-compatible system with a C compiler (gcc/clang).

Recommended compile command:

    gcc -o editor src/editor.c -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE

Notes:
- The code uses POSIX terminal/ioctl/getline APIs. Compile with -D_DEFAULT_SOURCE (or -D_GNU_SOURCE) if your system requires it for getline.
- If your compiler complains about feature-test macros, add -D_GNU_SOURCE or -D_DEFAULT_SOURCE.

Run

    ./editor [path/to/file]

If a filename is provided the editor will open that file; otherwise it starts with an empty buffer.

Keybindings / Controls
- CTRL-Q : Quit (will warn if there are unsaved changes; press repeatedly to force quit)
- CTRL-S : Save current file
- CTRL-F : Search (type query, press Enter; use arrow keys to navigate results)
- Enter  : Insert newline
- Backspace / DEL : Delete characters
- Arrow keys : Move cursor
- PageUp / PageDown : Scroll by a page
- Home / End : Move to start/end of line
- Typing : Insert characters

Behavior notes
- When saving, if the editor was started without a filename it will prompt for a "Save as" name.
- The editor maintains a modified flag; attempting to quit with unsaved changes requires multiple CTRL-Q presses as a safeguard.
- Basic syntax highlighting is enabled for files with extensions like .c .h .cpp .txt and for files that match known patterns. The highlighter recognizes numbers, strings, comments and keywords (see src/editor.c for details).

Terminal compatibility
- The editor uses ANSI escape sequences and raw terminal mode. It should work in most modern Unix-like terminal emulators (xterm, gnome-terminal, iTerm2, etc.).
- Do not run inside terminals that don't support ANSI/VT100 escapes.

Development / Contributing
- This is a small single-file C project. Contributions, bug reports and suggestions are welcome — open an issue or submit a pull request.
- When making changes, keep terminal behavior and portability in mind. Test on a typical Linux environment.

Attribution
- This project follows the spirit of small tutorial editors (for example "kilo") and is intended for learning and lightweight editing in the terminal.

License
This project is provided under the MIT License. See the LICENSE file for details (or add one if desired).

