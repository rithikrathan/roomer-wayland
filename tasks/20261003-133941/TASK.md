# fill for polygon and ngons

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: no

Fixed polygon and N-gon fill rendering:
- Fixed stroke_toggle_fill_last() to toggle g_state->shape_filled directly for active and upcoming shapes.
- Polygon ear-clipping and N-gon fans render double-sided triangles to prevent Raylib/OpenGL backface culling issues.
- Added live fill preview during active polygon vertex placement.
