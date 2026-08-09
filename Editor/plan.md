# Verification of `currentView` declarations and layout fix

## `currentView` analysis
I searched the entire project for `ScreenView currentView` and found EXACTLY ONE global declaration at line 341. There are no local shadows or duplicates. I will explain this to the user. The reason `EDITOR` was printed was likely because a key like `Enter` or a mouse click triggered the transition to the Editor mode when the file was empty.

## Fix for Glitchy Sidebar
The root cause of the "still glitchy" layout is that the Home Screen rows print EXACTLY `screenCols` characters, followed by a `\n`. In standard Unix terminals, writing to the final column automatically wraps the cursor to the next line. When the explicit `\n` is then printed, it advances the cursor a SECOND time, creating double-spaced lines and destroying the grid layout.
I will change the `MBW` calculation so the total width of the Home Screen inner layout is `screenCols - 1` to prevent this terminal auto-wrap bug.
