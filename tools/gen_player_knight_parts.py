import argparse
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PALETTE = {
    ".": (0, 0, 0, 0),
    "o": (63, 38, 49, 255),
    "s": (70, 66, 84, 255),
    "m": (99, 111, 133, 255),
    "l": (149, 166, 183, 255),
    "h": (205, 220, 222, 255),
    "r": (154, 48, 62, 255),
    "R": (204, 69, 73, 255),
    "p": (235, 113, 98, 255),
    "b": (99, 49, 57, 255),
    "g": (175, 129, 88, 255),
}
PART_ROWS = {
    "head": [
        "......oooooooo........",
        ".....oRRRRRRro........",
        "....orRRpRRRRro.......",
        "...orRRpRRRRrro.......",
        "..orRrRroossssooooo...",
        ".oossssoosllllhhlllo..",
        "oosmmllhhllhhllhhlllo.",
        "osmmlhhllhllhlllhhlllo",
        "osmllhhllhhllhhllhhllo",
        "osmmllooooooolhhhllloo",
        "osmmllosmmmolhhhlllloo",
        "osmmlllosmmmohlhlllloo",
        ".osmmlhloosmmmhlllloo.",
        ".osmmllhhlosmmmhllloo.",
        "..osmmllooosssssloo...",
        "..osmllllllllllllo....",
        "...ossmllllllllso.....",
        "...ossssssssssoo......",
        "....ossssssssso.......",
        ".....oooooooooo.......",
    ],
    "torso": [
        "ooooooooooo..ooooooooooooooo",
        "ossssssssso..ollhhlllhhhlo..",
        "osmmmmmso....omllhhlllllllso",
        ".orRromlhhllhllhlllhhlllso..",
        ".orRRrommmllhllhhlllhhhlmso.",
        "orRppRrommmlhhllhllllhhlmso.",
        "orRRRrrommmlhllhhllllhhlso..",
        ".orrrrommmmllhllhllllhlmmso.",
        ".osrommmmmlhhllhhllllhlmmso.",
        ".osrommmmmllhhllhhlllhhso...",
        ".osrommmmmmllhhllhlllhhso...",
        "..osmmmmmmllhllhhlllhhmso...",
        "..osmmmmmmmllhhllhhllmmso...",
        "...osllllooooooooolllso.....",
        "..obboggggggggggggggggobbbo.",
        "..obhgggggghgggggghgggghbo..",
        "..obbogggoggoggogggogggobo..",
        "....osmmmmmmmmmmmmmmmmso....",
        "....osssssssssssssssssso....",
        ".....oooooooooooooooooo.....",
    ],
    "arm": [
        "...ooooo...",
        "..olhhhlo..",
        ".ollllmmo..",
        ".ollmmmmo..",
        "..olmmso...",
        "...oRro....",
        "...orrso...",
        "...omso....",
        "...omso....",
        "...oso.....",
        "...ogo.....",
        "...ogo.....",
        "...oso.....",
        "...olmmo...",
        "...omlmo...",
        "...oooo....",
    ],
    "leg": [
        "....oooooooooooooo....",
        "....omllllllllllmmo...",
        "....omlllllllllllmmo..",
        "....omllllllllllllmmo.",
        "....omllllllllllllmso.",
        "....osmmmmmmmmmmmso...",
        "....osmmmmmmmmmmmo....",
        "....osmmmmmmmmmo......",
        "....osmmmmmmo.........",
        "....osmmmmso..........",
        "....osmmso............",
        "....ommmo.............",
        "....ogmmo.............",
        "....ogggoo............",
        "....ohhhhhhoollo......",
        "....osssssssssso......",
        "....osssssssssso......",
        "....osssssssssbo......",
        "....osssssssssso......",
        "....oooooooooooooo....",
    ],
    "weapon": [
        "..oo..", ".oggo.", ".oooo.", ".orro.", ".ogro.",
        ".orro.", ".ogro.", ".orro.", "oooooo", "oghhgo",
        "ooomoo", ".olmo.", ".ohmo.", ".ohmo.", ".ohmo.",
        ".olmo.", ".ohmo.", ".ohmo.", ".ohmo.", ".olmo.",
        ".ohmo.", ".olmo.", "..omo.", "..oo..",
    ],
}
SIZES = {"head": (22, 20), "torso": (28, 20), "arm": (11, 16),
         "leg": (22, 20), "weapon": (6, 24)}


def make_part(name):
    width, height = SIZES[name]
    rows = PART_ROWS[name]
    if len(rows) != height or any(len(row) != width for row in rows):
        raise ValueError(f"Invalid pixel grid: {name}")
    image = Image.new("RGBA", (width, height))
    image.putdata([PALETTE[pixel] for row in rows for pixel in row])
    return image


def generate(output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    for name in SIZES:
        image = make_part(name)
        destination = output_dir / f"player_part_{name}.png"
        image.save(destination)
        print(f"wrote {destination} {image.size}")


def main():
    parser = argparse.ArgumentParser(description="Draw red-crest pixel knight parts.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "reports" / "player_knight_parts")
    options = parser.parse_args()
    generate(options.output_dir)


if __name__ == "__main__":
    main()
