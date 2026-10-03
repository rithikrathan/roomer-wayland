<div align="center">
  <h1>Roomer</h1>
  <h3>zoomer application for linux</h3>
</div>

## Demo

![Demo](https://raw.githubusercontent.com/rithikrathan/roomer-wayland/master/assets/demo.gif)

## Usage

```sh
Usage:
  roomer -d, --daemon                                Daemon Mode (instant startup)
  grim - | roomer [options]                          Roomer Mode (connects to daemon or runs standalone)
  roomer [options] < image.[png|jpg|webp|bmp]        Image Viewer Mode
  roomer -q, --quit                                  Stop running daemon
Options:
  -h,             --help                             Show this message and exit.
  -v,             --version                          Show version and exit.
  -d,             --daemon                           Run as background daemon (instant startup).
  -q,             --quit                             Stop running daemon.
                  --no-daemon                        Force standalone mode (ignore daemon).
  -ms <float>,    --monitor-scaling <float>          Compositor monitor scaling (default 1).
  -bg <rgba hex>, --background <rgba hex>            Background color.
  -t,             --transparent                      Transparent background.
```

### Instant Startup with Daemon Mode

Add `roomer --daemon` to your compositor autostart (e.g. in `hyprland.conf`: `exec-once = roomer --daemon`).
When running in daemon mode, Roomer pre-initializes its Wayland window, shaders, and fonts in the background with 0% idle CPU.
When your screenshot shortcut fires `grim - | roomer`, the image is passed over a local UNIX socket and the window opens **instantaneously (<10ms)**.
Pressing `ESC` or `Q` hides the window and resets annotations, keeping it warm for the next screenshot.

Because this is a native wayland window, positioning has to be done through the window manager.
In the case of Hyprland, this can be done with the following rules:

```
windowrule = float,     title:^roomer$
windowrule = monitor 1, title:^roomer$
windowrule = move 0 0,  title:^roomer$
windowrule = noanim,    title:^roomer$
```

You need to set `monitor` to your leftmost monitor, `hyprctl monitors` shows all monitors and their IDs.
When opening a file, the window title will be `roomer - image viewer`.

In case you are using monitor scaling, pass the same value to the `--monitor-scaling` option:

```
monitor=DP-1, 3840x2160@144, 0x0, 1.666667  =>  --monitor-scaling 1.666667
```

## Features

- **Daemon Mode (`--daemon`)** – instant startup (<10ms) via local UNIX socket; pre-warms window and shaders with 0% idle CPU
- **Full Drawing & Shape Suite**:
  - **Pen (`1`)** – freehand sketching with smooth midpoint quadratic bezier splines
  - **Highlighter (`Shift+1`)** – premultiplied alpha compositing for overlap-free marking
  - **Eraser (`2`)** – segment-distance swept hit-testing that deletes strokes, shapes, and badges on touch
  - **Straight Line (`3`)** – straight lines with round caps; `Shift+3` cycles Solid / Dashed / Dotted
  - **Arrow (`4`)** – directional arrows with proportional heads; `Shift+4` cycles Solid / Dashed / Dotted
  - **Triangle (`5`)** – equilateral/symmetric triangles; `Shift+5` cycles Solid / Dashed / Dotted
  - **Rectangle (`6`)** – bounding boxes; `Shift+6` cycles Solid / Dashed / Dotted
  - **Circle / Ellipse (`7`)** – smooth parametric circles; `Shift+7` cycles Solid / Dashed / Dotted
  - **Step Badge (`8`)** – stack-based auto-incrementing numbered badges (`①`, `②`...); `-` pops the last badge
  - **Text Tool (`9`)** – in-place multiline typing (`Enter` for newline, `Esc` to commit, click-away to start new)
  - **Table Tool (`0`)** – interactive tabular grids; `Arrow Keys` while dragging adjust rows and columns
- **Shape Customizations**:
  - **Styles** – Solid, Dashed, and Dotted for all shapes
  - **Dash Spacing** – adjust dash length and gap spacing with `[` and `]`
  - **Fill Mode (`Ctrl+F`)** – instantly toggle semi-transparent/solid fill on the last drawn shape
  - **Shift Drag Snapping** – hold Shift to constrain lines/arrows to 45°, rectangles to squares, and circles to 1:1
- **Toolbox (`C`)** – sleek 4-column GUI with direct buttons for all 11 tools, style toggles, fill button, size slider, zoom slider, color pickers, clear, and fit
- **Persistent Cursor Size Indicator** – dynamic visual outline under the cursor matching active tool size and color (shows next number for Step Badge, I-beam for Text)
- **Zero-Jitter Smooth Rendering** – direct floating-point subpixel rendering for buttery smooth 120 FPS zoom & pan
- **Black Board (`B`)** – infinite canvas overlay with dot grid for sketching
- **Flashlight (`F`)** – shader-based spotlight with smooth animations and adjustable radius
- **Color Picker & Quick Swap (`X`)** – pick colors via `yad` from the toolbox or swap primary/secondary colors
- **Keymaps Overlay (`H`)** – in-app keybinding reference

## Keybindings

| Key / Input             | Action                                                  |
| ----------------------- | ------------------------------------------------------- |
| `1`                     | Pen                                                     |
| `Shift + 1`             | Highlighter                                             |
| `2`                     | Eraser                                                  |
| `3`                     | Straight Line (press again or `Shift+3` to cycle style) |
| `4`                     | Arrow (press again or `Shift+4` to cycle style)         |
| `5`                     | Triangle (press again or `Shift+5` to cycle style)      |
| `6`                     | Rectangle (press again or `Shift+6` to cycle style)     |
| `7`                     | Circle (press again or `Shift+7` to cycle style)        |
| `8`                     | Step Badge (`①`, `②`...)                                |
| `-`                     | Pop last Step Badge and decrement counter               |
| `9`                     | Text Tool (type text; `Enter` = newline, `Esc` = done)  |
| `0`                     | Table Tool                                              |
| `Up` / `Down`           | Increase / decrease table rows (while dragging table)   |
| `Right` / `Left`        | Increase / decrease table cols (while dragging table)   |
| `Ctrl + F`              | Toggle Fill on last shape                               |
| `[` / `]`               | Decrease / increase dash length and spacing             |
| `+` / `-`               | Increase / decrease brush or eraser size                |
| `Shift` (during drag)   | Constrain 45° angle / square / uniform circle           |
| `B`                     | Toggle Blackboard                                       |
| `C`                     | Toggle Toolbox                                          |
| `F`                     | Toggle Flashlight                                       |
| `H`                     | Toggle Keybindings help overlay                         |
| `X`                     | Swap primary and secondary colors                       |
| `A`                     | Fit image to screen                                     |
| `Shift + 0` / `Ctrl + R`| Reset view & clear annotations                          |
| `ESC` / `Q`             | Hide window (daemon mode) or Quit (standalone)          |
| Left Mouse Drag         | Pan                                                     |
| Right Mouse Drag        | Draw                                                    |
| Mouse Wheel             | Zoom In / Out                                           |
| Shift + Mouse Wheel     | Fine zoom (3× slower)                                   |
| Mouse Wheel (flashlight)| Change flashlight radius                                |
| Pen                     | Draw (with pressure)                                    |
| Pen Barrel Button 1     | Pan                                                     |
| Pen Barrel Button 2     | Zoom (anchor-distance)                                  |
| Ctrl + Pen Touch        | Zoom (anchor-distance, alternative)                     |

## Installation

Build from source:

```sh
git clone https://github.com/rithikrathan/roomer-wayland.git
cd roomer-wayland
make build
```

## Dependencies

- glfw
- grim (for taking the screenshot, any other screenshot tool that can output to stdout works)
- wl-copy (optional, for screenshots to clipboard)
- yad (optional, for color picker)

## Development

```sh
make
```

## Credits

- Pen, Eraser, Trash, Maximize icons: [Feather Icons](https://feathericons.com/) (MIT)
- Highlighter icon: [game-icons.net](https://game-icons.net/) (CC BY 3.0)
- Font: [InconsolataLGCNerdFont](https://github.com/ryanoasis/nerd-fonts)

## References

- https://github.com/tsoding/boomer
