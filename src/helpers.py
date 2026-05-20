def ip(de):
    line = [str(i) for i in int.to_bytes(de, 4, signed=True)]
    return f"{'.'.join(line[:3])}.0/{line[3]}"


def b(de):
    line = hex(de)[2:].zfill(12)
    line = [line[i:i+2] for i in range(0, len(line), 2)]
    return ":".join(line)
