# Implementation Plan: Comprehensive Annotation & Shape Tool Suite

## 1. Architectural Overview & Objectives
Expand Roomer from a freehand drawing utility into a complete screen annotation and diagramming suite. This architecture introduces geometric shapes, procedural line styles (solid, dashed, dotted), stack-based step badges, multiline text annotations, dynamic tables, and contextual shape customization (fill toggle, fill opacity, two-tone colors), all while retaining 120 FPS subpixel rendering, daemon instant-launch capabilities, and Wayland compatibility.

---

## 2. Drawing Engine & Data Model Refactor

### Unified Shape Representation
- Extend the unified stroke data structure so a single stroke record can represent any annotation element:
  - Freehand curves with quadratic bezier splines
  - Straight lines and directional arrows
  - Geometric polygons (rectangles, triangles)
  - Curvilinear shapes (circles, ellipses)
  - Stacked numeric step badges
  - Multiline typographic text blocks
  - Configurable tabular grids
- Each record stores its geometric bounds (origin and terminus), stroke thickness, primary stroke color, fill state, fill color, fill opacity, stroke pattern (solid, dashed, dotted), pattern parameters (dash length and spacing), and tool-specific payloads (step count, text buffer, table row and column counts).

### Render Pipeline Enhancements
- Procedural dashed and dotted rasterization using vector distance parametrization along straight segments, arrow shafts, and polygon perimeters.
- Fill rendering with hardware blending:
  - Outline strokes rendered with clean antialiased caps and joins.
  - Interior fills rendered with customizable alpha transparency, allowing underlying screenshots to remain legible beneath colored annotations.
- Live interactive preview layer:
  - While dragging, the canvas renders an uncommitted preview of the active shape from the drag origin to the current cursor position.
  - Committing occurs on mouse button release, appending the finished record to the active layer's stroke buffer.

---

## 3. Tool Specifications & Behaviors

### Primary Tools & Keybind Layout
- **Pen (Key 1)**: Freehand sketching with bezier curve interpolation.
- **Highlighter (Shift + 1)**: Semitransparent additive marker rendered to an offscreen compositing target to prevent overlap darkening.
- **Eraser (Key 2)**: Swept segment-distance hit-testing that deletes whole strokes, shapes, or badges upon contact.
- **Straight Line (Key 3)**: Clean line with round endcaps. `Shift + 3` toggles dashed and dotted styles.
- **Arrow (Key 4)**: Directional vector with proportional arrowhead. `Shift + 4` toggles dashed and dotted shafts.
- **Triangle (Key 5)**: Three-point polygon aligned from drag bounding box. `Shift + 5` toggles dashed/dotted borders.
- **Rectangle (Key 6)**: Four-point bounding box with sharp or rounded corners. `Shift + 6` toggles dashed/dotted borders.
- **Circle / Ellipse (Key 7)**: Smooth parametric ellipse inscribed within the drag boundary. `Shift + 7` toggles dashed/dotted borders.
- **Step Badge (Key 8)**: Stack-based numbered badges (`①`, `②`, `③`...).
  - Each left click places the next consecutive number inside a circular badge.
  - Pressing the minus key (`-`) pops the most recent badge off the stack and decrements the counter.
- **Text Tool (Key 9)**: Multiline screen annotations.
  - Click anywhere on the canvas to open an active text cursor.
  - Enter key creates a new line.
  - Escape key finalizes and commits the text.
  - Clicking elsewhere commits the current text block and immediately initiates a new text cursor at the new position.
- **Table Tool (Key 0)**: Interactive tabular grids.
  - Click and drag defines the outer dimensions of the table.
  - While dragging, keyboard arrow keys adjust the grid density: Up/Down modifies row count, Left/Right modifies column count.
  - Releasing mouse commits the grid lines.

### Shape Styling & Fill Control
- Shapes default to clean outlines.
- Pressing `Ctrl + F` immediately toggles fill on the most recently placed shape, or toggles default fill mode for newly drawn shapes.
- Fill opacity is adjustable from subtle highlights (25%) to solid fills (100%).
- Primary color governs the border outline; secondary color can be used for the interior fill.

---

## 4. Toolbox Interface Expansion

### Primary Tool Palette
- The toolbox UI will be expanded with distinct graphical icon buttons for every tool:
  - Pen, Highlighter, Eraser
  - Line, Arrow, Triangle, Rectangle, Circle
  - Step Badge, Text Tool, Table Tool
  - Primary and secondary color swatches with quick swap button

### Contextual Tool Settings Bar
- When any shape or configurable tool is active, the toolbox reveals contextual controls:
  - Line style picker: Solid, Dashed, Dotted
  - Dash spacing and length adjustments
  - Fill mode switch (Outline vs Filled)
  - Fill opacity slider / preset selector
  - Table dimension counters (Rows and Columns)
  - Step badge counter display with Reset button

---

## 5. Implementation Milestones

1. **Milestone 1**: Refactor internal stroke data models and write procedural rendering routines for lines, arrows, triangles, rectangles, circles, dashed patterns, and dotted patterns.
2. **Milestone 2**: Implement the live interactive drag preview and `Ctrl + F` fill toggle logic with opacity blending.
3. **Milestone 3**: Build the stack-based Step Badge tool and interactive Table Grid tool.
4. **Milestone 4**: Build the multiline Text Tool with keyboard input handling, backspace, newlines, and click-away placement.
5. **Milestone 5**: Design and render toolbox buttons and contextual settings panels for shape customization.
6. **Milestone 6**: Update keybinding listeners, in-app keymaps help overlay (`H`), and user documentation in `README.md`.
