gcc -LSDL -ISDL -lSDLFontCache_Windows -c src/GeneralMain.c -o GeneralMain.o
ar rcs libGeneralGoodWindows.a GeneralMain.o