"""
Generate pixel-art skill icons for roguelike_cpp
Style: Dark fantasy pixel art with glow effects
Size: 32x32 pixels
"""

from PIL import Image, ImageDraw
import os

OUTPUT_DIR = r"C:\Demo\roguelike_cpp\assets\icons\skills"
SIZE = 32

def create_slash_icon():
    """Create slash/斩击 skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (200, 200, 220, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Diagonal slash line
    for i in range(20):
        x = 5 + i
        y = 25 - i
        if 0 <= x < SIZE and 0 <= y < SIZE:
            # Main slash
            draw.point((x, y), fill=(220, 220, 240, 255))
            draw.point((x+1, y), fill=(200, 200, 220, 255))
            draw.point((x, y+1), fill=(180, 180, 200, 255))
            # Highlight
            if i % 3 == 0:
                draw.point((x, y), fill=(255, 255, 255, 255))
    
    # Secondary slash (thinner)
    for i in range(15):
        x = 8 + i
        y = 23 - i
        if 0 <= x < SIZE and 0 <= y < SIZE:
            draw.point((x, y), fill=(150, 150, 170, 150))
    
    # Impact sparkles
    for x, y in [(20, 8), (10, 18), (18, 12)]:
        draw.point((x, y), fill=(255, 255, 255, 255))
        draw.point((x+1, y), fill=(200, 200, 220, 200))
        draw.point((x, y+1), fill=(200, 200, 220, 200))
    
    return img

def create_divine_judgment_icon():
    """Create divine judgment/神罚 skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (255, 220, 50, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Light beam (vertical)
    for y in range(4, 28):
        for x in range(13, 19):
            # Beam gradient
            if y < 10:
                color = (255, 255, 200, 255)
            elif y < 20:
                color = (255, 220, 50, 255)
            else:
                color = (255, 180, 30, 255)
            draw.point((x, y), fill=color)
    
    # Cross at top
    draw.rectangle([14, 5, 17, 12], fill=(255, 255, 220, 255))
    draw.rectangle([11, 7, 20, 10], fill=(255, 255, 220, 255))
    
    # Radiating light rays
    for angle in range(0, 360, 45):
        import math
        rad = math.radians(angle)
        for dist in range(8, 14):
            x = int(15 + dist * math.cos(rad))
            y = int(8 + dist * math.sin(rad))
            if 0 <= x < SIZE and 0 <= y < SIZE:
                draw.point((x, y), fill=(255, 240, 150, 150))
    
    # Sparkles
    for x, y in [(8, 15), (22, 15), (10, 22), (20, 22)]:
        draw.point((x, y), fill=(255, 255, 255, 255))
    
    return img

def create_time_stop_icon():
    """Create time stop/时停 skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (80, 150, 255, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Clock face
    for x in range(6, 26):
        for y in range(6, 26):
            dx = x - 15
            dy = y - 15
            dist = (dx*dx + dy*dy) ** 0.5
            if dist <= 10:
                # Clock background
                if dist < 8:
                    color = (20, 30, 60, 255)
                else:
                    color = (40, 60, 100, 255)
                draw.point((x, y), fill=color)
    
    # Clock border
    draw.ellipse([6, 6, 25, 25], outline=(100, 150, 220, 255))
    
    # Clock hands (frozen at 3:00)
    # Hour hand
    draw.line([(15, 15), (15, 9)], fill=(150, 200, 255, 255), width=2)
    # Minute hand
    draw.line([(15, 15), (21, 15)], fill=(150, 200, 255, 255), width=2)
    
    # Stop symbol (red square)
    draw.rectangle([12, 12, 18, 18], fill=(220, 50, 50, 255))
    
    # Stop symbol border
    draw.rectangle([12, 12, 18, 18], outline=(255, 100, 100, 255))
    
    # Frozen particles
    for x, y in [(8, 10), (22, 10), (8, 20), (22, 20)]:
        draw.point((x, y), fill=(100, 180, 255, 200))
    
    return img

def create_heal_icon():
    """Create heal/治愈 skill icon"""
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Outer glow
    for i in range(3):
        color = (100, 220, 100, 40 - i * 10)
        draw.ellipse([2+i, 2+i, 29-i, 29-i], outline=color)
    
    # Healing cross
    cross_pixels = []
    # Vertical bar
    for y in range(6, 26):
        for x in range(13, 19):
            cross_pixels.append((x, y))
    # Horizontal bar
    for x in range(6, 26):
        for y in range(13, 19):
            if (x, y) not in cross_pixels:
                cross_pixels.append((x, y))
    
    for x, y in cross_pixels:
        # Gradient based on distance from center
        dist = abs(x - 15) + abs(y - 15)
        if dist < 3:
            color = (180, 255, 180, 255)
        elif dist < 6:
            color = (120, 220, 120, 255)
        else:
            color = (80, 180, 80, 255)
        draw.point((x, y), fill=color)
    
    # Heart in center
    heart_pixels = [
        (14, 12), (15, 12), (16, 12), (17, 12),
        (13, 13), (14, 13), (15, 13), (16, 13), (17, 13), (18, 13),
        (13, 14), (14, 14), (15, 14), (16, 14), (17, 14), (18, 14),
        (14, 15), (15, 15), (16, 15), (17, 15),
        (15, 16), (16, 16),
    ]
    for x, y in heart_pixels:
        draw.point((x, y), fill=(255, 100, 100, 255))
    
    # Healing sparkles
    for x, y in [(8, 8), (24, 8), (8, 24), (24, 24)]:
        draw.point((x, y), fill=(200, 255, 200, 255))
    
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
        'skill_slash.png': create_slash_icon(),
        'skill_divine.png': create_divine_judgment_icon(),
        'skill_timestop.png': create_time_stop_icon(),
        'skill_heal.png': create_heal_icon(),
    }
    
    for filename, img in icons.items():
        filepath = os.path.join(OUTPUT_DIR, filename)
        img.save(filepath)
        print(f"Generated: {filepath}")
    
    print(f"\nTotal: {len(icons)} icons generated in {OUTPUT_DIR}")

if __name__ == '__main__':
    main()
