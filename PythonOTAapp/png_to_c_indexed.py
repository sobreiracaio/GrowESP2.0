from PIL import Image
import argparse
import os

# ==========================
# Conversão RGB888 → RGB565
# ==========================
def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

# ==========================
# Main
# ==========================
def main():

    parser = argparse.ArgumentParser(description="PNG → 4bit ou 8bit C array (ESP32)")
    parser.add_argument("input", help="Imagem de entrada (png/jpg)")
    parser.add_argument("--width", type=int, help="Nova largura")
    parser.add_argument("--height", type=int, help="Nova altura")
    parser.add_argument("--bits", type=int, choices=[4,8], default=4, help="4 ou 8 bits")
    parser.add_argument("--keep-aspect", action="store_true", help="Manter proporção")

    args = parser.parse_args()

    img = Image.open(args.input).convert("RGB")

    # ==========================
    # Redimensionamento
    # ==========================
    if args.width or args.height:

        if args.keep_aspect:
            img.thumbnail((args.width or img.width,
                           args.height or img.height),
                           Image.LANCZOS)
        else:
            img = img.resize((args.width or img.width,
                              args.height or img.height),
                              Image.LANCZOS)

    width, height = img.size
    colors = 16 if args.bits == 4 else 256

    img = img.quantize(colors=colors, method=Image.FASTOCTREE)
    palette = img.getpalette()[:colors * 3]
    indexed_pixels = list(img.getdata())

    # ==========================
    # Criar paleta RGB565
    # ==========================
    palette565 = []
    for i in range(colors):
        r = palette[i*3]
        g = palette[i*3 + 1]
        b = palette[i*3 + 2]
        palette565.append(rgb888_to_rgb565(r, g, b))

    name = os.path.splitext(os.path.basename(args.input))[0]
    output_file = f"{name}_{args.bits}bit.h"

    with open(output_file, "w") as f:

        f.write("#pragma once\n\n")
        f.write(f"#define {name.upper()}_WIDTH {width}\n")
        f.write(f"#define {name.upper()}_HEIGHT {height}\n\n")

        # Paleta
        f.write(f"const uint16_t {name}_palette[{colors}] PROGMEM = {{\n")
        for i, c in enumerate(palette565):
            f.write(f"0x{c:04X}, ")
            if (i+1) % 8 == 0:
                f.write("\n")
        f.write("\n};\n\n")

        # Dados
        if args.bits == 8:
            f.write(f"const uint8_t {name}_data[{width*height}] PROGMEM = {{\n")
            for i, px in enumerate(indexed_pixels):
                f.write(f"{px}, ")
                if (i+1) % 32 == 0:
                    f.write("\n")
            f.write("\n};\n")

        else:  # 4 bits
            packed = []
            for i in range(0, len(indexed_pixels), 2):
                p1 = indexed_pixels[i]
                p2 = indexed_pixels[i+1] if i+1 < len(indexed_pixels) else 0
                packed.append((p1 << 4) | p2)

            f.write(f"const uint8_t {name}_data[{len(packed)}] PROGMEM = {{\n")
            for i, px in enumerate(packed):
                f.write(f"0x{px:02X}, ")
                if (i+1) % 32 == 0:
                    f.write("\n")
            f.write("\n};\n")

    print(f"✔ Gerado: {output_file}")
    print(f"Tamanho imagem: {width}x{height}")
    print(f"Modo: {args.bits} bits")
    print(f"Pixels: {width*height}")

if __name__ == "__main__":
    main()