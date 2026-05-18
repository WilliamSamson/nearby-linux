from PIL import Image, ImageDraw

def make_squircle(img_path, output_path, radius=80):
    img = Image.open(img_path).convert("RGBA")
    size = img.size
    
    # Create mask
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle((0, 0, size[0], size[1]), radius=radius, fill=255)
    
    # Apply mask
    result = Image.new("RGBA", size, (0, 0, 0, 0))
    result.paste(img, (0, 0), mask=mask)
    
    result.save(output_path)

if __name__ == "__main__":
    make_squircle("/home/kayode-olalere/Downloads/quick_share.png", 
                  "/home/kayode-olalere/Codes/nearby-linux/app/ui-gtk/data/app-icon.png")
