gcc -LSDL -ISDL -lSDLFontCache_Windows -c src/GeneralMain.c -o GeneralMain.o
ar rcs libGeneralGood.a GeneralMain.o