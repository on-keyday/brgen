con <- rawConnection(raw(0), "wb")

writeBin(32, con, size = 4, endian = "little")

buf <- rawConnectionValue(con)
close(con)
print(buf)