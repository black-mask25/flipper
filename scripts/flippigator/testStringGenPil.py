from PIL import Image, ImageDraw, ImageFont

text = "BN0"
# font = ImageFont.load("./flippigator/fonts/helvB08.pil")
font = ImageFont.truetype("./flippigator/fonts/haxrcorp_4089.ttf", size=16)
# font = ImageFont.load_path("FuBar-Bold.pil")  # searches for a bitmap font along the Python path.
img = Image.new("L", font.getsize(text), 255)
dctx = ImageDraw.Draw(img)
dctx.text((0, 0), text, font=font)
del dctx
# img = img.resize((img.width * 8, img.height * 8))
img.show()
