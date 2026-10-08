rt.run("lib") rt.out("dis7.txt", "### 54f7990\n" .. rt.disfunc(BASE + 0x54f7990, 40) .. "\n### 5e44e60 around\n" .. rt.disasm(BASE + 0x5e44e60 + 0x390, 20)) rt.log("dis7 fin")
