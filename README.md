# aawm
Assemble a Window Manager: a window manager with code reusability in mind.

## Status

The project is in the experimentation phase. Don't expect too much :)

## Building

Ensure that the needed libraries are installed.

On Debian-based systems, these are `libxcb1-dev`, `libxcb-shape0-dev` and `libxcb-render0-dev`, `libxcb-xinput-dev`, `libxcb-xfixes0-dev`.

      make

## Running

You probably don't want to run it as your main window manager (yet). Either:

  * start another X session from a console: `X :1`
  * or start a nested server: `Xephyr :1`
  * Xnest isn't supported yet as Xnest doesn't provide the Render extension.

Then set the DISPLAY environment variable and run the program:

      export DISPLAY=:1
      ${BUILD_DIR}/aawm
