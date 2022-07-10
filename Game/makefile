CPPFLAGS = -Wall -g -O3
OBJFILES = audio.o coord.o frame.o input.o inventory.o main.o object.o options.o procs.o read.o read_buffer.o render.o save.o state.o stb_vorbis.o str.o win32.o window_map.o window_task.o world.o write_buffer.o
LINKFLAGS = -mconsole -mwindows

output: $(OBJFILES)
	gcc $(OBJFILES) $(LINKFLAGS)

audio.o: audio.c audio.h container.h procs.h read.h stb_vorbis.h
	gcc -c audio.c $(CPPFLAGS)

coord.o: coord.c coord.h
	gcc -c coord.c $(CPPFLAGS)

frame.o: frame.c frame.h procs.h
	gcc -c frame.c $(CPPFLAGS)

input.o: input.c input.h procs.h win32.h
	gcc -c input.c $(CPPFLAGS)

inventory.o: inventory.c coord.h input.h inventory.h scalar.h window_map.h window_task.h
	gcc -c inventory.c $(CPPFLAGS)

main.o: main.c audio.h container.h coord.h frame.h input.h inventory.h object.h options.h pallete.h procs.h read.h read_buffer.h render.h save.h scalar.h state.h stb_vorbis.h str.h win32.h window_map.h window_task.h world.h write_buffer.h
	gcc -c main.c $(CPPFLAGS)

object.o: object.c coord.h object.h
	gcc -c object.c $(CPPFLAGS)

options.o: options.c audio.h input.h options.h render.h scalar.h state.h stb_vorbis.h window_map.h world.h
	gcc -c options.c $(CPPFLAGS)

procs.o: procs.c procs.h
	gcc -c procs.c $(CPPFLAGS)

read.o: read.c read.h
	gcc -c read.c $(CPPFLAGS)

read_buffer.o: read_buffer.c inventory.h read.h read_buffer.h scalar.h world.h
	gcc -c read_buffer.c $(CPPFLAGS)

render.o: render.c coord.h inventory.h read_buffer.h render.h scalar.h window_map.h window_task.h world.h
	gcc -c render.c $(CPPFLAGS)

save.o: save.c audio.h coord.h inventory.h options.h read.h read_buffer.h save.h scalar.h stb_vorbis.h window_map.h window_task.h world.h write_buffer.h
	gcc -c save.c $(CPPFLAGS)

state.o: state.c state.h
	gcc -c state.c $(CPPFLAGS)

stb_vorbis.o: stb_vorbis.c stb_vorbis.h
	gcc -c stb_vorbis.c $(CPPFLAGS)

str.o: str.c str.h
	gcc -c str.c $(CPPFLAGS)

win32.o: win32.c render.h win32.h world.h
	gcc -c win32.c $(CPPFLAGS)

window_map.o: window_map.c coord.h input.h inventory.h render.h scalar.h str.h window_map.h window_task.h world.h
	gcc -c window_map.c $(CPPFLAGS)

window_task.o: window_task.c container.h coord.h inventory.h list.h options.h save.h window_map.h window_task.h world.h
	gcc -c window_task.c $(CPPFLAGS)

world.o: world.c audio.h inventory.h object.h pallete.h read.h read_buffer.h render.h scalar.h stb_vorbis.h world.h
	gcc -c world.c $(CPPFLAGS)

write_buffer.o: write_buffer.c inventory.h world.h write_buffer.h
	gcc -c write_buffer.c $(CPPFLAGS)

clean: 
	del *.o
