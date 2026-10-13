all: compile run

compile:
	gcc main.c -o app -lgdi32 -lmsimg32 -mwindows

run: app.exe
	./app.exe