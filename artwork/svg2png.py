import os
import os.path
import sys

# brew install imagemagick --with-librsvg
# this script
# iconutil -c icns Web-eID.iconset# 

def convert():
    for res in [1024, 512, 256, 128, 64, 48, 32, 24, 16]:
      for dpi in [72, 144]:
        os.system("convert -background none -density %dx%d -resize %dx%d %s %s" %(dpi, dpi, res, res, sys.argv[1], os.path.splitext(sys.argv[1])[0] + "_%dx%d%s" % (res, res, "@2x" if dpi == 144 else "") + ".png"))

convert()