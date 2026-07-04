<div align="center">
  <h1>roomer</h1>
  <h3>zoomer application for linux</h3>
</div>

[![License](https://img.shields.io/github/license/rithikrathan/roomer-wayland)](LICENSE)

## Demo

![Demo](https://raw.githubusercontent.com/rithikrathan/roomer-wayland/master/assets/demo.gif)

## Usage

```sh
Usage:
  grim - | ./roomer [options]                        Roomer Mode
  ./roomer [options] < image.[png|jpg|webp|bmp]      Image Viewer Mode
Options:
  -h,             --help                                     Show this message and exit.
  -v,             --version                                  Show version and exit.
  -sd <path>,     --screenshot-dir <path>                    Folder to save screenshots in.
  -ms <float>,    --monitor-scaling <float>                  Compositor monitor scaling (default 1).
  -bg <rgba hex>, --background <rgba hex>                    Background color.

```

All defaults can be changed, if there is a need for adding more options, please open an issue.

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

## Keybindings

| Input                  | Action                                                  |
| ---------------------- | ------------------------------------------------------- |
| `1`                    | Pen                                                     |
| `2`                    | Eraser                                                  |
| `3`                    | Highlighter                                             |
| `B`                    | Toggle Blackboard                                       |
| `C`                    | Toggle Toolbox                                          |
| `F`                    | Toggle Flashlight                                       |
| `H`                    | Toggle Keybindings help                                 |
| `X`                    | Swap colors                                             |
| `0`                    | Reset view                                              |
| `ESC` / `Q`           | Quit                                                    |
| Left Mouse Drag        | Pan                                                     |
| Right Mouse Drag       | Draw                                                    |
| Mouse Wheel            | Zoom In / Out                                           |
| Shift + Mouse Wheel    | Fine zoom (3× slower)                                   |
| Mouse Wheel (flashlight)| Change flashlight radius                               |

## Installation

Build it from source:

```sh
git clone https://github.com/rithikrathan/roomer-wayland.git
cd roomer-wayland
make build
```

## Dependencies

- glfw
- grim (for taking the screenshot, any other screenshot tool that can output to stdout works)
- wl-copy (optional, for screenshots to clipboard)

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
