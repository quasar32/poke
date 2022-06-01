import os
from collections import OrderedDict

source_dict = {}
header_dict = {}
flat_dict = {}
object_dict = OrderedDict()

def recurse(func):
    dir_list = os.listdir('.')
    for name in dir_list:
        if os.path.isdir(name):
            os.chdir(name)
            recurse(func)
            os.chdir('..')
        else:
            func(name)

def extract_lib(line):
    start = line.find('"')
    end = line.find('"', start + 1)
    return line[start + 1:end]

def extract_all_libs(path):
    with open(path, 'r') as f:
        lines = f.readlines()
    depend = {
        extract_lib(line) 
        for line in lines 
        if line.startswith('#include "')
    }
    return depend

def extract_libs_from(path):
    if path.endswith('.c'):
        source_dict[path] = extract_all_libs(path)
    elif path.endswith('.h'):
        header_dict[path] = extract_all_libs(path)

def create_flat_dict():
    print(source_dict)
    for k, v in source_dict.items():
        flat_dict[k] = v 
        for e in v:
            flat_dict[k] = flat_dict[k].union(header_dict[e])
    return flat_dict

def create_object_dict():
    for source_path, header_path in flat_dict.items():
        object_path = source_path.replace('.c', '.o')
        object_dict[object_path] = {source_path}.union(flat_dict[source_path])

def create_makefile():
    with open('makefile', 'w') as f:
        f.write('') 
        f.write('CPPFLAGS = -Wall -g -O3\n')
        f.write('OBJFILES = ' + ' '.join(object_dict.keys()) + '\n')
        f.write('LINKFLAGS = -mconsole -mwindows\n\n')
        f.write('output: $(OBJFILES)\n')
        f.write('\tgcc $(OBJFILES) $(LINKFLAGS)\n')
        for object_path in object_dict.keys():
            source_path = object_path.replace('.o', '.c')
            header_paths = list(flat_dict[source_path])
            header_paths.sort()
            depend = source_path + ' ' + ' '.join(header_paths) 
            f.write('\n')
            f.write(object_path + ": " + depend + '\n')
            f.write('\tgcc -c ' + source_path + ' $(CPPFLAGS)\n')
        f.write('\nclean: \n\tdel *.o\n')

recurse(extract_libs_from)
create_flat_dict()
create_object_dict()
create_makefile()

