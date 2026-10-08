rt.run("lib") rt.out("dis6.txt", rt.disasm(BASE + 0x247d90, 24) .. "\n---\n" .. rt.disasm(BASE + 0x247e40, 16)) rt.log("dis6 fin")
