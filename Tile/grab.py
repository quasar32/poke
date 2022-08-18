import subprocess

for i in range(0, 17 + 1):
        subprocess.run(
            [
                "a", 
                "../PokeGrab/tiles/tile_set_%02d.bmp" % i,   
                "Tiles/TileData%02d" % i,
                "TileData"
            ]
        )

