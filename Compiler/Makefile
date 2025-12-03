
run:
	bison -d glsl.y
	flex glsl.l
	gcc glsl.tab.c lex.yy.c ast.c symbol_table.c semantic.c gpu_ir.c nir_codegen.c gpu_linker.c pisa_defs.c backend.c -o compiler -g