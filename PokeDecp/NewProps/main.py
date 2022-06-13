import sys

with open(sys.argv[1], "rb") as f:
    prop_flags = f.read()

prop_enum = []
for i, e in enumerate(prop_flags):
    for j in range(1, 8):
        if e & (1 << j):
            prop_enum.append(j + 1)
            break
    else:
        prop_enum.append(e & 1)
    
with open(sys.argv[1], "wb") as f:
    f.write(bytes(prop_enum))
