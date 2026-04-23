# aawm
Assemble a Window Manager: a window manager with code reusability in mind.

## Status

The project is in the experimentation phase. Don't expect too much :)

## Building

Ensure that the needed libraries are installed.

On Debian-based systems, these are `libxcb1-dev`, `libxcb-shape0-dev`, `libxcb-render0-dev`, `libxcb-xinput-dev`, `libxcb-xfixes0-dev` and `libxcb-icccm4-dev`.

      make

## Running

You probably don't want to run it as your main window manager (yet). Either:

  * start another X session from a console: `X :1`
  * or start a nested server: `Xephyr :1`, or `Xnest :1`

While it isn't supported yet, you can test multiple screens with `Xephyr -screen 1024x768 -screen 1024x768 :1` or `Xnest -geometry 1024x768 -scrns 2 :1`

Then set the DISPLAY environment variable and run the program:

      export DISPLAY=:1
      ${BUILD_DIR}/aawm
