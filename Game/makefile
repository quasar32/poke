CPPFLAGS = -Wall -g -O3
OBJFILES = audio.o buffer.o frame.o input.o intro.o inventory.o main.o options.o procs.o render.o save.o state.o stb_vorbis.o text.o timer.o win32.o window_task.o world.o
LINKFLAGS = -mconsole -mwindows

output: $(OBJFILES)
	gcc $(OBJFILES) $(LINKFLAGS)

audio.o: audio.c audio.h scalar.h stb_vorbis.h win32.h
	gcc -c audio.c $(CPPFLAGS)

buffer.o: buffer.c buffer.h inventory.h scalar.h world.h
	gcc -c buffer.c $(CPPFLAGS)

frame.o: frame.c coord.h frame.h input.h render.h timer.h win32.h world.h
	gcc -c frame.c $(CPPFLAGS)

input.o: input.c input.h win32.h
	gcc -c input.c $(CPPFLAGS)

intro.o: intro.c audio.h coord.h frame.h input.h intro.h inventory.h render.h save.h text.h window_task.h world.h
	gcc -c intro.c $(CPPFLAGS)

inventory.o: inventory.c coord.h input.h inventory.h scalar.h text.h window_task.h
	gcc -c inventory.c $(CPPFLAGS)

main.o: main.c audio.h coord.h frame.h input.h intro.h inventory.h options.h procs.h render.h save.h scalar.h state.h text.h win32.h window_task.h world.h
	gcc -c main.c $(CPPFLAGS)

options.o: options.c audio.h coord.h frame.h input.h inventory.h options.h render.h scalar.h text.h window_task.h
	gcc -c options.c $(CPPFLAGS)

procs.o: procs.c procs.h
	gcc -c procs.c $(CPPFLAGS)

render.o: render.c buffer.h coord.h inventory.h render.h scalar.h text.h window_task.h world.h
	gcc -c render.c $(CPPFLAGS)

save.o: save.c buffer.h coord.h inventory.h options.h save.h scalar.h text.h timer.h window_task.h world.h
	gcc -c save.c $(CPPFLAGS)

state.o: state.c state.h
	gcc -c state.c $(CPPFLAGS)

stb_vorbis.o: stb_vorbis.c stb_vorbis.h
	gcc -c stb_vorbis.c $(CPPFLAGS)

text.o: text.c audio.h coord.h frame.h input.h inventory.h options.h render.h save.h scalar.h text.h window_task.h world.h
	gcc -c text.c $(CPPFLAGS)

timer.o: timer.c procs.h timer.h
	gcc -c timer.c $(CPPFLAGS)

win32.o: win32.c render.h win32.h
	gcc -c win32.c $(CPPFLAGS)

window_task.o: window_task.c coord.h inventory.h list.h options.h save.h text.h window_task.h world.h
	gcc -c window_task.c $(CPPFLAGS)

world.o: world.c audio.h buffer.h coord.h frame.h inventory.h render.h scalar.h text.h window_task.h world.h
	gcc -c world.c $(CPPFLAGS)

clean: 
	del *.o
