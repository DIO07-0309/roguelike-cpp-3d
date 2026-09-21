"""
Generate pixel-art skill icons for roguelike_cpp
Style: Dark fantasy pixel art with glow effects
Size: 32x32 pixels
"""

from PIL import Image, ImageDraw
import os

OUTPUT_DIR = r"C:\Demo\roguelike_cpp\assets\icons\skills"
SIZE = 32

def create_fire_icon():
    """Create fire element skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (255, 100, 30, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Main flame shape (pixel art)
    flame_pixels = [
        (15, 5), (16, 5),
        (14, 6), (15, 6), (16, 6), (17, 6),
        (13, 7), (14, 7), (15, 7), (16, 7), (17, 7), (18, 7),
        (12, 8), (13, 8), (14, 8), (15, 8), (16, 8), (17, 8), (18, 8), (19, 8),
        (11, 9), (12, 9), (13, 9), (14, 9), (15, 9), (16, 9), (17, 9), (18, 9), (19, 9), (20, 9),
        (10, 10), (11, 10), (12, 10), (13, 10), (14, 10), (15, 10), (16, 10), (17, 10), (18, 10), (19, 10), (20, 10), (21, 10),
        (9, 11), (10, 11), (11, 11), (12, 11), (13, 11), (14, 11), (15, 11), (16, 11), (17, 11), (18, 11), (19, 11), (20, 11), (21, 11), (22, 11),
        (8, 12), (9, 12), (10, 12), (11, 12), (12, 12), (13, 12), (14, 12), (15, 12), (16, 12), (17, 12), (18, 12), (19, 12), (20, 12), (21, 12), (22, 12), (23, 12),
        (8, 13), (9, 13), (10, 13), (11, 13), (12, 13), (13, 13), (14, 13), (15, 13), (16, 13), (17, 13), (18, 13), (19, 13), (20, 13), (21, 13), (22, 13), (23, 13),
        (9, 14), (10, 14), (11, 14), (12, 14), (13, 14), (14, 14), (15, 14), (16, 14), (17, 14), (18, 14), (19, 14), (20, 14), (21, 14), (22, 14),
        (10, 15), (11, 15), (12, 15), (13, 15), (14, 15), (15, 15), (16, 15), (17, 15), (18, 15), (19, 15), (20, 15), (21, 15),
        (11, 16), (12, 16), (13, 16), (14, 16), (15, 16), (16, 16), (17, 16), (18, 16), (19, 16), (20, 16),
        (12, 17), (13, 17), (14, 17), (15, 17), (16, 17), (17, 17), (18, 17), (19, 17),
        (13, 18), (14, 18), (15, 18), (16, 18), (17, 18), (18, 18),
        (14, 19), (15, 19), (16, 19), (17, 19),
        (15, 20), (16, 20),
    ]
    
    # Fire gradient colors
    for x, y in flame_pixels:
        # Gradient based on height
        if y < 12:
            color = (255, 220, 50, 255)  # Yellow core
        elif y < 16:
            color = (255, 150, 30, 255)  # Orange middle
        else:
            color = (220, 60, 20, 255)   # Red base
        draw.point((x, y), fill=color)
    
    # Highlight pixels
    for x, y in [(15, 10), (16, 11), (15, 12), (14, 13)]:
        draw.point((x, y), fill=(255, 255, 200, 255))
    
    return img

def create_ice_icon():
    """Create ice element skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (100, 180, 255, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Snowflake pattern
    center = (15, 15)
    
    # Main snowflake arms
    for angle in [0, 60, 120, 180, 240, 300]:
        import math
        rad = math.radians(angle)
        for dist in range(3, 12):
            x = int(center[0] + dist * math.cos(rad))
            y = int(center[1] + dist * math.sin(rad))
            if 0 <= x < SIZE and 0 <= y < SIZE:
                # Branch pixels
                for offset in [0, 1, -1]:
                    bx = x + int(offset * math.cos(rad + math.pi/4))
                    by = y + int(offset * math.sin(rad + math.pi/4))
                    if 0 <= bx < SIZE and 0 <= by < SIZE:
                        draw.point((bx, by), fill=(150, 220, 255, 255))
    
    # Center crystal
    for dx in range(-2, 3):
        for dy in range(-2, 3):
            if abs(dx) + abs(dy) <= 2:
                draw.point((center[0]+dx, center[1]+dy), fill=(200, 240, 255, 255))
    
    # Highlight
    draw.point((14, 14), fill=(255, 255, 255, 255))
    
    return img

def create_poison_icon():
    """Create poison element skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (120, 220, 50, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Skull shape
    skull_pixels = [
        (12, 8), (13, 8), (14, 8), (15, 8), (16, 8), (17, 8), (18, 8), (19, 8),
        (11, 9), (12, 9), (13, 9), (14, 9), (15, 9), (16, 9), (17, 9), (18, 9), (19, 9), (20, 9),
        (10, 10), (11, 10), (12, 10), (13, 10), (14, 10), (15, 10), (16, 10), (17, 10), (18, 10), (19, 10), (20, 10), (21, 10),
        (10, 11), (11, 11), (12, 11), (13, 11), (14, 11), (15, 11), (16, 11), (17, 11), (18, 11), (19, 11), (20, 11), (21, 11),
        (11, 12), (12, 12), (13, 12), (14, 12), (15, 12), (16, 12), (17, 12), (18, 12), (19, 12), (20, 12),
        (12, 13), (13, 13), (14, 13), (15, 13), (16, 13), (17, 13), (18, 13), (19, 13),
        (13, 14), (14, 14), (15, 14), (16, 14), (17, 14), (18, 14),
    ]
    
    for x, y in skull_pixels:
        draw.point((x, y), fill=(180, 220, 50, 255))
    
    # Eye sockets
    draw.rectangle([13, 10, 14, 11], fill=(30, 50, 20, 255))
    draw.rectangle([17, 10, 18, 11], fill=(30, 50, 20, 255))
    
    # Teeth
    for x in range(13, 19):
        draw.point((x, 14), fill=(220, 255, 100, 255))
    
    # Drip effect
    for y in range(15, 20):
        draw.point((15, y), fill=(150, 200, 40, 255))
        draw.point((17, y), fill=(150, 200, 40, 255))
    
    return img

def create_physical_icon():
    """Create physical element skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (200, 200, 220, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Sword shape
    sword_pixels = [
        (15, 4), (16, 4),
        (15, 5), (16, 5),
        (15, 6), (16, 6),
        (15, 7), (16, 7),
        (15, 8), (16, 8),
        (15, 9), (16, 9),
        (15, 10), (16, 10),
        (15, 11), (16, 11),
        (15, 12), (16, 12),
        (15, 13), (16, 13),
        (14, 14), (15, 14), (16, 14), (17, 14),
        (13, 15), (14, 15), (15, 15), (16, 15), (17, 15), (18, 15),
        (14, 16), (15, 16), (16, 16), (17, 16),
        (15, 17), (16, 17),
        (15, 18), (16, 18),
        (15, 19), (16, 19),
    ]
    
    for x, y in sword_pixels:
        # Blade gradient
        if y < 12:
            color = (220, 220, 240, 255)
        elif y < 15:
            color = (180, 180, 200, 255)
        else:
            color = (140, 140, 160, 255)
        draw.point((x, y), fill=color)
    
    # Hilt
    draw.rectangle([13, 15, 18, 16], fill=(100, 60, 30, 255))
    draw.rectangle([14, 14, 17, 14], fill=(80, 50, 20, 255))
    
    # Highlight
    draw.point((15, 6), fill=(255, 255, 255, 255))
    
    return img

def create_lightning_icon():
    """Create lightning/storm skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (255, 255, 100, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Lightning bolt
    bolt_pixels = [
        (16, 4), (17, 4),
        (15, 5), (16, 5), (17, 5),
        (14, 6), (15, 6), (16, 6), (17, 6),
        (13, 7), (14, 7), (15, 7), (16, 7),
        (14, 8), (15, 8), (16, 8),
        (15, 9), (16, 9),
        (16, 10), (17, 10),
        (17, 11), (18, 11),
        (18, 12), (19, 12),
        (17, 13), (18, 13), (19, 13),
        (16, 14), (17, 14), (18, 14),
        (15, 15), (16, 15), (17, 15),
        (14, 16), (15, 16), (16, 16),
        (13, 17), (14, 17), (15, 17),
        (12, 18), (13, 18), (14, 18),
        (11, 19), (12, 19), (13, 19),
    ]
    
    for x, y in bolt_pixels:
        # Gradient
        if y < 8:
            color = (255, 255, 200, 255)
        elif y < 14:
            color = (255, 220, 50, 255)
        else:
            color = (255, 180, 30, 255)
        draw.point((x, y), fill=color)
    
    # Sparkles
    for x, y in [(10, 8), (20, 10), (8, 14), (22, 16)]:
        draw.point((x, y), fill=(255, 255, 255, 255))
    
    return img

def create_shadow_icon():
    """Create shadow/dark skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (100, 50, 150, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Eye of darkness
    eye_pixels = [
        (8, 14), (9, 14), (10, 14), (11, 14), (12, 14), (13, 14), (14, 14), (15, 14), (16, 14), (17, 14), (18, 14), (19, 14), (20, 14), (21, 14), (22, 14), (23, 14),
        (7, 15), (8, 15), (9, 15), (10, 15), (11, 15), (12, 15), (13, 15), (14, 15), (15, 15), (16, 15), (17, 15), (18, 15), (19, 15), (20, 15), (21, 15), (22, 15), (23, 15), (24, 15),
        (6, 16), (7, 16), (8, 16), (9, 16), (10, 16), (11, 16), (12, 16), (13, 16), (14, 16), (15, 16), (16, 16), (17, 16), (18, 16), (19, 16), (20, 16), (21, 16), (22, 16), (23, 16), (24, 16), (25, 16),
        (7, 17), (8, 17), (9, 17), (10, 17), (11, 17), (12, 17), (13, 17), (14, 17), (15, 17), (16, 17), (17, 17), (18, 17), (19, 17), (20, 17), (21, 17), (22, 17), (23, 17), (24, 17),
        (8, 18), (9, 18), (10, 18), (11, 18), (12, 18), (13, 18), (14, 18), (15, 18), (16, 18), (17, 18), (18, 18), (19, 18), (20, 18), (21, 18), (22, 18), (23, 18),
    ]
    
    for x, y in eye_pixels:
        draw.point((x, y), fill=(80, 30, 120, 255))
    
    # Pupil
    for dx in range(-3, 4):
        for dy in range(-2, 3):
            if dx*dx/9 + dy*dy/4 <= 1:
                draw.point((15+dx, 16+dy), fill=(20, 5, 30, 255))
    
    # Highlight
    draw.point((13, 15), fill=(180, 100, 220, 255))
    
    return img

def create_blood_icon():
    """Create blood/life skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (220, 30, 30, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Blood drop
    drop_pixels = [
        (15, 6), (16, 6),
        (14, 7), (15, 7), (16, 7), (17, 7),
        (13, 8), (14, 8), (15, 8), (16, 8), (17, 8), (18, 8),
        (12, 9), (13, 9), (14, 9), (15, 9), (16, 9), (17, 9), (18, 9), (19, 9),
        (11, 10), (12, 10), (13, 10), (14, 10), (15, 10), (16, 10), (17, 10), (18, 10), (19, 10), (20, 10),
        (10, 11), (11, 11), (12, 11), (13, 11), (14, 11), (15, 11), (16, 11), (17, 11), (18, 11), (19, 11), (20, 11), (21, 11),
        (10, 12), (11, 12), (12, 12), (13, 12), (14, 12), (15, 12), (16, 12), (17, 12), (18, 12), (19, 12), (20, 12), (21, 12),
        (10, 13), (11, 13), (12, 13), (13, 13), (14, 13), (15, 13), (16, 13), (17, 13), (18, 13), (19, 13), (20, 13), (21, 13),
        (11, 14), (12, 14), (13, 14), (14, 14), (15, 14), (16, 14), (17, 14), (18, 14), (19, 14), (20, 14),
        (12, 15), (13, 15), (14, 15), (15, 15), (16, 15), (17, 15), (18, 15), (19, 15),
        (13, 16), (14, 16), (15, 16), (16, 16), (17, 16), (18, 16),
        (14, 17), (15, 17), (16, 17), (17, 17),
        (15, 18), (16, 18),
    ]
    
    for x, y in drop_pixels:
        # Gradient
        if y < 10:
            color = (220, 30, 30, 255)
        else:
            color = (180, 20, 20, 255)
        draw.point((x, y), fill=color)
    
    # Highlight
    draw.point((14, 9), fill=(255, 100, 100, 255))
    
    return img

def create_arcane_icon():
    """Create arcane/magic skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (180, 100, 255, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Magic orb
    for x in range(6, 26):
        for y in range(6, 26):
            dx = x - 15
            dy = y - 15
            dist = (dx*dx + dy*dy) ** 0.5
            if dist <= 10:
                # Gradient based on distance
                if dist < 3:
                    color = (220, 180, 255, 255)
                elif dist < 6:
                    color = (180, 120, 255, 255)
                elif dist < 9:
                    color = (140, 80, 220, 255)
                else:
                    color = (100, 50, 180, 255)
                draw.point((x, y), fill=color)
    
    # Sparkles around orb
    for x, y in [(8, 10), (22, 10), (8, 20), (22, 20), (15, 5), (15, 25)]:
        draw.point((x, y), fill=(255, 255, 255, 255))
    
    return img

def create_shield_icon():
    """Create shield/defense skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (100, 150, 200, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Shield shape
    shield_pixels = []
    for y in range(6, 22):
        for x in range(8, 24):
            # Shield outline
            if y < 8:
                continue
            if y > 20:
                continue
            # Shield body
            if y < 12:
                if 10 <= x <= 22:
                    shield_pixels.append((x, y))
            elif y < 16:
                if 9 <= x <= 23:
                    shield_pixels.append((x, y))
            else:
                if 10 <= x <= 22:
                    shield_pixels.append((x, y))
                if y >= 18:
                    if 12 <= x <= 20:
                        shield_pixels.append((x, y))
                if y >= 20:
                    if 14 <= x <= 18:
                        shield_pixels.append((x, y))
    
    for x, y in shield_pixels:
        # Gradient
        if y < 10:
            color = (150, 180, 220, 255)
        elif y < 16:
            color = (100, 140, 180, 255)
        else:
            color = (80, 110, 150, 255)
        draw.point((x, y), fill=color)
    
    # Shield emblem
    draw.rectangle([14, 10, 17, 14], fill=(180, 200, 230, 255))
    draw.point((15, 12), fill=(255, 255, 255, 255))
    
    return img

def main():
    """Generate all skill icons"""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    icons = {
        'skill_fire.png': create_fire_icon(),
        'skill_ice.png': create_ice_icon(),
        'skill_poison.png': create_poison_icon(),
        'skill_physical.png': create_physical_icon(),
        'skill_lightning.png': create_lightning_icon(),
        'skill_shadow.png': create_shadow_icon(),
        'skill_blood.png': create_blood_icon(),
        'skill_arcane.png': create_arcane_icon(),
        'skill_shield.png': create_shield_icon(),
    }
    
    for filename, img in icons.items():
        filepath = os.path.join(OUTPUT_DIR, filename)
        img.save(filepath)
        print(f"Generated: {filepath}")
    
    print(f"\nTotal: {len(icons)} icons generated in {OUTPUT_DIR}")

if __name__ == '__main__':
    main()
