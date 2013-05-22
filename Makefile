default: test

CFLAGS=-I"`ocamlc -where`" `pkg-config --cflags libavformat`
LIBS=`pkg-config --libs libavformat libswscale`

libav.o: libav.c
	gcc -c $(CFLAGS) $< $(LIBS)

libav.a : libav.o
	ocamlmklib -o libav $< $(LIBS)

test.cmx: libav.mli test.ml
	ocamlopt -c $^

test: test.cmx libav.a
	ocamlopt -verbose -o $@ graphics.cmxa $< -cclib -L. -cclib -llibav -cclib "$(LIBS)"

main:
	ocamlfind ocamlc graphics.cma main.ml

clean:
	rm -f *.[oa] *.so *.cm[ixoa] *.cmxa test
