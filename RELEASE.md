# EVI v0.2.18

## Balanced series and match panes

The main study view now opens with the **Series** and **Matches and instances** areas sharing the available vertical workspace equally.

### Changed

- The main horizontal splitter now starts at a true 50/50 height allocation after the window has completed its initial layout.
- The lower **Matches and instances** pane therefore remains visible when EVI starts, even on smaller window sizes.
- Splitter sash gravity is now 0.5 so both panes grow and shrink evenly when the main window is resized.
- The splitter remains fully user-adjustable with the mouse.
- A manually chosen splitter position is not forcibly reset during later window resizing.

All DICOM search, series filtering, header inspection, language, appearance, and export behavior remains unchanged.
