# terminal_minecraft

In linux:
- install libx11-dev if you are on Ubuntu with
    `sudo apt install libx11-dev`

- then compile with
    `gcc terminal_minecraft.c -lm -lX11`

Controls:
- Use arrow keys for moving
- Use w, a, s, d keys for changing view angles
- Use space for placing block
- Use shift + space for removing block

No collision detection etc.
