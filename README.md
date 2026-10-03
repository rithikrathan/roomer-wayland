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
- **Pen, Eraser, Highlighter** – three core drawing tools with adjustable stroke and eraser sizes
- **Persistent Cursor Size Indicator** – real-time visual indicator under the cursor matching active tool size
- **Zero-Jitter Smooth Rendering** – direct floating-point subpixel rendering for buttery smooth 120 FPS zoom & pan
- **Black Board** – infinite canvas overlay with dot grid for sketching (`B`)
- **Toolbox** – draggable GUI popup with tool selection, size slider, zoom slider, color pickers, clear & fit buttons (`C`)
- **Flashlight** – shader-based spotlight with smooth animations and adjustable radius (`F`)
- **Color Picker & Quick Swap** – pick colors via `yad` from the toolbox or swap primary/secondary colors (`X`)
- **Keymaps Overlay** – in-app keybinding reference (`H`)
- **Reset View & Fit** – reset zoom/pan/annotations (`0`) or fit image to screen (`A`)
- **Bezier Spline Smoothing** – midpoint quadratic bezier curves for smooth freehand strokes
- **Composited Highlighter** – premultiplied alpha blending via offscreen target for overlap-free highlighting
- **Tablet Support** – evdev stylus support with pressure sensitivity and barrel buttons

## Keybindings

| Input                  | Action                                                  |
| ---------------------- | ------------------------------------------------------- |
| `1`                    | Pen                                                     |
| `2`                    | Eraser                                                  |
| `3`                    | Highlighter                                             |
| `+` / `-`              | Increase / decrease brush or eraser size                |
| `B`                    | Toggle Blackboard                                       |
| `C`                    | Toggle Toolbox                                          |
| `F`                    | Toggle Flashlight                                       |
| `H`                    | Toggle Keybindings help overlay                         |
| `X`                    | Swap primary and secondary colors                       |
| `A`                    | Fit image to screen                                     |
| `0`                    | Reset view & clear annotations                          |
| `ESC` / `Q`            | Hide window (daemon mode) or Quit (standalone)          |
| Left Mouse Drag        | Pan                                                     |
| Right Mouse Drag       | Draw                                                    |
| Mouse Wheel            | Zoom In / Out                                           |
| Shift + Mouse Wheel    | Fine zoom (3× slower)                                   |
| Mouse Wheel (flashlight)| Change flashlight radius                               |
| Pen                    | Draw (with pressure)                                    |
| Pen Barrel Button 1    | Pan                                                     |
| Pen Barrel Button 2    | Zoom (anchor-distance)                                  |
| Ctrl + Pen Touch       | Zoom (anchor-distance, alternative)                     |

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
