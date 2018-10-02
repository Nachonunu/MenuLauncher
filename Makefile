CODE=main.cpp
FLAG=-static -mwindows
CPP=g++
NAME=MenuLauncher

Launcher: $(CODE)
	$(CPP) -g -Wall -O3 -std=c++11 $(FLAG) -I./ -o $(NAME) $(CODE)
	strip $(NAME).exe

