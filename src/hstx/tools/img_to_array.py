"""
USAGE:
python3 img_to_array.py <input_file> [width] [height] [out_dir]

EXAMPLES:
python3 img_to_array.py my_image.png                  # Defaults to 640x480, saves to ../assets
python3 img_to_array.py my_image.png 800 600          # Custom resolution
python3 img_to_array.py my_image.png 640 480 ./assets # Custom output directory
"""

import sys
import os
import re
from PIL import Image, ImageOps

def convert_image(image_path, width=640, height=480, output_dir=None):
    base_name = os.path.basename(image_path)
    raw_name, _ = os.path.splitext(base_name)
    
    # Sanitize the filename to create a valid C variable name
    array_name = re.sub(r'\W', '_', raw_name)
    if array_name[0].isdigit():
        array_name = "_" + array_name

    print(f"Converting: '{image_path}' -> {width}x{height} array: '{array_name}'")

    try:
        # Convert to RGB and letterbox the image to fit the target resolution
        img = Image.open(image_path).convert('RGB')
        img = ImageOps.pad(img, (width, height), color=(0, 0, 0))
    except Exception as e:
        print(f"Error: {e}")
        return

    # Generate C Header Boilerplate
    # Note: Using .data without 'const' means this array will be copied from 
    # Flash into the RP2350's SRAM at boot. Keep image sizes under ~300KB!
    c_code = f"""#ifndef _IMG_ASSET_SECTION
#define _IMG_ASSET_SECTION ".data"
#endif

#include <stdint.h>

/**
 * Auto-Generated DVI Asset
 * Resolution: {width}x{height}
 * Format: RGB332
 */
static uint8_t __attribute__((aligned(4), section(_IMG_ASSET_SECTION ".{array_name}"))) {array_name}[] = {{\n    """

    # Pack 24-bit RGB down to 8-bit RGB332 (3 bits Red, 3 bits Green, 2 bits Blue)
    pixels = list(img.getdata())
    hex_pixels = []
    for r, g, b in pixels:
        rgb332 = (((r >> 5) & 0x07) << 5) | (((g >> 5) & 0x07) << 2) | ((b >> 6) & 0x03)
        hex_pixels.append(f"0x{rgb332:02x}")

    # Format into a clean 16-column grid for the header file
    for i in range(0, len(hex_pixels), 16):
        c_code += ", ".join(hex_pixels[i:i+16])
        if i + 16 < len(hex_pixels):
            c_code += ",\n    "
    c_code += "\n};\n"

    # Handle output directory routing
    if output_dir is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        output_dir = os.path.join(script_dir, "..", "assets")
        
    os.makedirs(output_dir, exist_ok=True)
    out_path = os.path.join(output_dir, f"{array_name}.h")
    
    with open(out_path, "w") as f:
        f.write(c_code)
        
    print(f"Success! Saved to {out_path}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 img_to_array.py <input_file> [w] [h] [out_dir]")
        sys.exit(1)
        
    # Safely parse arguments with fallbacks
    in_file = sys.argv[1]
    target_w = int(sys.argv[2]) if len(sys.argv) > 2 else 640
    target_h = int(sys.argv[3]) if len(sys.argv) > 3 else 480
    out_path = sys.argv[4] if len(sys.argv) > 4 else None
    
    convert_image(in_file, target_w, target_h, out_path)